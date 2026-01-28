//By leewheel
#include "gtest/gtest.h"
#include "PvpGearManager.h"
#include "BotPvpGearCache.h"
#include "ErrorRecoveryManager.h"
#include "InventoryManager.h"
#include "EquipmentValidator.h"
#include "Mgr/Item/StatsWeightCalculator.h"
#include "Player.h"
#include "Log.h"
#include <memory>
#include <chrono>
#include <random>

/**
 * **功能: pvp-gear-auto-equip, 端到端集成测试**
 * 
 * 验证需求: 1.1, 4.1, 7.1 - 完整的PVP装备切换流程
 * 
 * 测试完整的战场进入-装备切换-退出流程：
 * - 系统初始化和组件集成
 * - 战场进入时的PVP装备切换
 * - 战场退出时的PVE装备恢复
 * - 异常情况处理和恢复
 * - 性能和稳定性验证
 */
class EndToEndIntegrationTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // 初始化所有系统组件
        InitializeSystemComponents();
        
        // 创建模拟玩家
        CreateMockPlayers();
        
        // 初始化随机数生成器
        rng_.seed(std::chrono::steady_clock::now().time_since_epoch().count());
        
        LOG_INFO("test", "End-to-end integration test setup completed");
    }

    void TearDown() override
    {
        // 清理测试数据
        CleanupTestData();
        
        LOG_INFO("test", "End-to-end integration test cleanup completed");
    }
private:
    // 初始化系统组件
    void InitializeSystemComponents()
    {
        // 初始化错误恢复管理器
        errorManager_ = ErrorRecoveryManager::GetInstance();
        ASSERT_NE(errorManager_, nullptr);
        bool errorMgrInit = errorManager_->Initialize();
        ASSERT_TRUE(errorMgrInit) << "Failed to initialize ErrorRecoveryManager";
        
        // 初始化缓存系统
        cache_ = BotPvpGearCache::GetInstance();
        ASSERT_NE(cache_, nullptr);
        bool cacheInit = cache_->LoadFromDatabase();
        if (!cacheInit)
        {
            LOG_WARN("test", "Database load failed, trying local cache");
            cacheInit = cache_->LoadCacheFromLocalFile();
        }
        
        // 初始化PVP装备管理器
        manager_ = PvpGearManager::GetInstance();
        ASSERT_NE(manager_, nullptr);
        bool managerInit = manager_->Initialize();
        ASSERT_TRUE(managerInit) << "Failed to initialize PvpGearManager";
        
        // 初始化背包管理器
        inventoryManager_ = std::make_unique<InventoryManager>();
        bool invInit = inventoryManager_->Initialize();
        ASSERT_TRUE(invInit) << "Failed to initialize InventoryManager";
        
        // 初始化装备验证器
        equipmentValidator_ = std::make_unique<EquipmentValidator>();
        bool validatorInit = equipmentValidator_->Initialize();
        ASSERT_TRUE(validatorInit) << "Failed to initialize EquipmentValidator";
        
        // 初始化统计权重计算器
        statsCalculator_ = std::make_unique<StatsWeightCalculator>();
        bool statsInit = statsCalculator_->Initialize();
        ASSERT_TRUE(statsInit) << "Failed to initialize StatsWeightCalculator";
        
        LOG_INFO("test", "All system components initialized successfully");
    }
    
    // 创建模拟玩家
    void CreateMockPlayers()
    {
        const std::vector<uint8> classes = {1, 2, 3, 4, 5, 6, 7, 8, 9, 11}; // 所有职业
        const std::vector<uint8> levels = {60, 70, 80}; // 不同等级
        
        for (uint8 classId : classes)
        {
            for (uint8 level : levels)
            {
                MockPlayer player;
                player.guid = nextPlayerGuid_++;
                player.classId = classId;
                player.level = level;
                player.name = "TestBot_" + std::to_string(classId) + "_" + std::to_string(level);
                player.inBattleground = false;
                player.hasValidEquipment = true;
                
                mockPlayers_.push_back(player);
            }
        }
        
        LOG_INFO("test", "Created {} mock players for testing", mockPlayers_.size());
    }
    // 清理测试数据
    void CleanupTestData()
    {
        if (cache_)
        {
            cache_->ClearCache();
        }
        
        mockPlayers_.clear();
        
        // 重置组件状态
        inventoryManager_.reset();
        equipmentValidator_.reset();
        statsCalculator_.reset();
    }

    // 模拟玩家数据结构
    struct MockPlayer
    {
        uint32 guid;
        uint8 classId;
        uint8 level;
        std::string name;
        bool inBattleground;
        bool hasValidEquipment;
        EquipmentSnapshot pveEquipment;
        std::vector<PvpGearRecommendation> pvpRecommendations;
        
        MockPlayer() : guid(0), classId(0), level(0), inBattleground(false), hasValidEquipment(false) {}
    };

    // 模拟战场进入流程
    bool SimulateBattlegroundEntry(MockPlayer& player)
    {
        try
        {
            LOG_DEBUG("test", "Simulating battleground entry for player {}", player.name);
            
            // 1. 保存当前PVE装备
            bool saveResult = inventoryManager_->SaveCurrentEquipment(nullptr, player.pveEquipment);
            if (!saveResult)
            {
                LOG_ERROR("test", "Failed to save PVE equipment for player {}", player.name);
                return false;
            }
            
            // 2. 获取推荐的PVP装备
            float resilienceRatio = (player.level >= 60) ? 0.8f : 0.3f;
            player.pvpRecommendations = manager_->GetRecommendedGear(nullptr, resilienceRatio);
            
            if (player.pvpRecommendations.empty() && cache_->IsLoaded())
            {
                LOG_WARN("test", "No PVP gear recommendations for player {}", player.name);
                return false;
            }
            
            // 3. 验证装备兼容性
            for (const auto& recommendation : player.pvpRecommendations)
            {
                bool isValid = equipmentValidator_->ValidateEquipment(recommendation.itemId, 
                                                                    player.classId, player.level);
                if (!isValid)
                {
                    LOG_WARN("test", "Invalid equipment recommendation {} for player {}", 
                            recommendation.itemId, player.name);
                }
            }
            
            // 4. 执行装备切换
            bool equipResult = manager_->EquipPvpGear(nullptr);
            if (!equipResult)
            {
                LOG_ERROR("test", "Failed to equip PVP gear for player {}", player.name);
                return false;
            }
            
            player.inBattleground = true;
            LOG_DEBUG("test", "Player {} successfully entered battleground", player.name);
            return true;
        }
        catch (const std::exception& e)
        {
            LOG_ERROR("test", "Exception during battleground entry for player {}: {}", 
                     player.name, e.what());
            return false;
        }
    }
    // 模拟战场退出流程
    bool SimulateBattlegroundExit(MockPlayer& player)
    {
        try
        {
            LOG_DEBUG("test", "Simulating battleground exit for player {}", player.name);
            
            if (!player.inBattleground)
            {
                LOG_WARN("test", "Player {} is not in battleground", player.name);
                return false;
            }
            
            // 1. 恢复PVE装备
            bool restoreResult = manager_->RestorePveGear(nullptr);
            if (!restoreResult)
            {
                LOG_ERROR("test", "Failed to restore PVE gear for player {}", player.name);
                return false;
            }
            
            // 2. 验证装备恢复
            if (player.pveEquipment.isValid)
            {
                bool verifyResult = inventoryManager_->RestoreEquipment(nullptr, player.pveEquipment);
                if (!verifyResult)
                {
                    LOG_WARN("test", "Equipment restoration verification failed for player {}", 
                            player.name);
                }
            }
            
            player.inBattleground = false;
            LOG_DEBUG("test", "Player {} successfully exited battleground", player.name);
            return true;
        }
        catch (const std::exception& e)
        {
            LOG_ERROR("test", "Exception during battleground exit for player {}: {}", 
                     player.name, e.what());
            return false;
        }
    }

    // 验证系统状态
    bool VerifySystemState()
    {
        try
        {
            // 验证缓存状态
            if (cache_->IsLoaded())
            {
                CacheStats stats = cache_->GetCacheStats();
                if (stats.currentEntryCount.load() == 0)
                {
                    LOG_WARN("test", "Cache is loaded but has no entries");
                }
            }
            
            // 验证错误恢复管理器状态
            bool healthCheck = errorManager_->CheckSystemHealth();
            if (!healthCheck)
            {
                LOG_WARN("test", "System health check failed");
                return false;
            }
            
            // 验证内存使用
            uint64_t memoryUsage = cache_->GetMemoryUsageBytes();
            if (memoryUsage > 1024 * 1024 * 1024) // 1GB
            {
                LOG_WARN("test", "Memory usage too high: {} bytes", memoryUsage);
                return false;
            }
            
            return true;
        }
        catch (const std::exception& e)
        {
            LOG_ERROR("test", "Exception during system state verification: {}", e.what());
            return false;
        }
    }

    // 成员变量
    BotPvpGearCache* cache_ = nullptr;
    PvpGearManager* manager_ = nullptr;
    ErrorRecoveryManager* errorManager_ = nullptr;
    std::unique_ptr<InventoryManager> inventoryManager_;
    std::unique_ptr<EquipmentValidator> equipmentValidator_;
    std::unique_ptr<StatsWeightCalculator> statsCalculator_;
    
    std::vector<MockPlayer> mockPlayers_;
    uint32 nextPlayerGuid_ = 10000;
    std::mt19937 rng_;
};
/**
 * 测试完整的战场进入和退出流程
 * 验证整个PVP装备切换系统的端到端功能
 */
TEST_F(EndToEndIntegrationTest, CompleteBattlegroundFlowTest)
{
    LOG_INFO("test", "Testing complete battleground flow");
    
    int successfulEntries = 0;
    int successfulExits = 0;
    int totalPlayers = std::min(static_cast<int>(mockPlayers_.size()), 20); // 限制测试数量
    
    for (int i = 0; i < totalPlayers; ++i)
    {
        MockPlayer& player = mockPlayers_[i];
        
        // 测试战场进入
        bool entryResult = SimulateBattlegroundEntry(player);
        if (entryResult)
        {
            successfulEntries++;
            
            // 验证系统状态
            EXPECT_TRUE(VerifySystemState()) << "System state invalid after entry for player " << player.name;
            
            // 短暂延迟模拟战场时间
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            
            // 测试战场退出
            bool exitResult = SimulateBattlegroundExit(player);
            if (exitResult)
            {
                successfulExits++;
                
                // 验证系统状态
                EXPECT_TRUE(VerifySystemState()) << "System state invalid after exit for player " << player.name;
            }
        }
    }
    
    LOG_INFO("test", "Battleground flow test completed: {}/{} entries, {}/{} exits successful", 
             successfulEntries, totalPlayers, successfulExits, successfulEntries);
    
    // 验证成功率
    double entrySuccessRate = static_cast<double>(successfulEntries) / totalPlayers;
    double exitSuccessRate = successfulEntries > 0 ? static_cast<double>(successfulExits) / successfulEntries : 0.0;
    
    EXPECT_GT(entrySuccessRate, 0.8) << "Entry success rate too low: " << entrySuccessRate * 100 << "%";
    EXPECT_GT(exitSuccessRate, 0.9) << "Exit success rate too low: " << exitSuccessRate * 100 << "%";
    
    // 验证最终系统状态
    EXPECT_TRUE(VerifySystemState()) << "Final system state verification failed";
}

/**
 * 测试异常情况下的系统恢复
 * 验证系统在各种异常情况下的恢复能力
 */
TEST_F(EndToEndIntegrationTest, ExceptionRecoveryTest)
{
    LOG_INFO("test", "Testing exception recovery scenarios");
    
    int recoveryAttempts = 0;
    int successfulRecoveries = 0;
    
    // 测试各种异常场景
    std::vector<std::function<void()>> exceptionScenarios = {
        // 场景1：模拟数据库连接失败
        [this]() {
            LOG_DEBUG("test", "Simulating database connection failure");
            // 触发数据库错误处理
            errorManager_->HandleDatabaseError("Simulated database connection failure", "EndToEndTest");
        },
        
        // 场景2：模拟内存不足
        [this]() {
            LOG_DEBUG("test", "Simulating memory shortage");
            errorManager_->HandleMemoryError("EndToEndTest");
        },
        
        // 场景3：模拟装备数据损坏
        [this]() {
            LOG_DEBUG("test", "Simulating gear data corruption");
            errorManager_->HandleGearDataError(12345, "Simulated data corruption");
        },
        
        // 场景4：模拟配置缺失
        [this]() {
            LOG_DEBUG("test", "Simulating configuration missing");
            errorManager_->HandleConfigurationError("PvpGearConfig", "Configuration file not found");
        }
    };
    
    for (auto& scenario : exceptionScenarios)
    {
        recoveryAttempts++;
        
        try
        {
            // 触发异常场景
            scenario();
            
            // 验证系统是否能够恢复
            bool healthCheck = errorManager_->CheckSystemHealth();
            if (healthCheck)
            {
                successfulRecoveries++;
                LOG_DEBUG("test", "Recovery scenario {} succeeded", recoveryAttempts);
            }
            else
            {
                LOG_WARN("test", "Recovery scenario {} failed health check", recoveryAttempts);
            }
            
            // 尝试执行正常操作以验证系统功能
            if (cache_->IsLoaded())
            {
                std::vector<PvpGearItem> testGear = cache_->GetAvailableGear(1, 70, "");
                LOG_DEBUG("test", "Post-recovery test query returned {} items", testGear.size());
            }
            
        }
        catch (const std::exception& e)
        {
            LOG_ERROR("test", "Exception during recovery test {}: {}", recoveryAttempts, e.what());
        }
        
        // 短暂延迟让系统稳定
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    LOG_INFO("test", "Exception recovery test completed: {}/{} scenarios recovered successfully", 
             successfulRecoveries, recoveryAttempts);
    
    // 验证恢复成功率
    double recoveryRate = static_cast<double>(successfulRecoveries) / recoveryAttempts;
    EXPECT_GT(recoveryRate, 0.75) << "Recovery success rate too low: " << recoveryRate * 100 << "%";
}
/**
 * 测试系统性能和稳定性
 * 验证系统在长时间运行下的性能和稳定性
 */
TEST_F(EndToEndIntegrationTest, PerformanceStabilityTest)
{
    LOG_INFO("test", "Testing system performance and stability");
    
    const int testDurationSeconds = 30;
    const int operationIntervalMs = 100;
    
    auto startTime = std::chrono::high_resolution_clock::now();
    auto endTime = startTime + std::chrono::seconds(testDurationSeconds);
    
    int totalOperations = 0;
    int successfulOperations = 0;
    std::vector<std::chrono::microseconds> operationTimes;
    
    uint64_t initialMemory = cache_->GetMemoryUsageBytes();
    
    while (std::chrono::high_resolution_clock::now() < endTime)
    {
        auto operationStart = std::chrono::high_resolution_clock::now();
        
        // 随机选择一个玩家进行操作
        int playerIndex = rng_() % mockPlayers_.size();
        MockPlayer& player = mockPlayers_[playerIndex];
        
        bool operationSuccess = false;
        
        if (!player.inBattleground)
        {
            // 尝试进入战场
            operationSuccess = SimulateBattlegroundEntry(player);
        }
        else
        {
            // 尝试退出战场
            operationSuccess = SimulateBattlegroundExit(player);
        }
        
        auto operationEnd = std::chrono::high_resolution_clock::now();
        auto operationTime = std::chrono::duration_cast<std::chrono::microseconds>(operationEnd - operationStart);
        
        operationTimes.push_back(operationTime);
        totalOperations++;
        
        if (operationSuccess)
        {
            successfulOperations++;
        }
        
        // 定期验证系统状态
        if (totalOperations % 50 == 0)
        {
            bool systemHealthy = VerifySystemState();
            if (!systemHealthy)
            {
                LOG_WARN("test", "System health check failed at operation {}", totalOperations);
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(operationIntervalMs));
    }
    
    uint64_t finalMemory = cache_->GetMemoryUsageBytes();
    
    // 计算性能统计
    auto avgOperationTime = std::accumulate(operationTimes.begin(), operationTimes.end(), 
                                          std::chrono::microseconds{0}) / operationTimes.size();
    auto maxOperationTime = *std::max_element(operationTimes.begin(), operationTimes.end());
    double successRate = static_cast<double>(successfulOperations) / totalOperations;
    double throughput = static_cast<double>(totalOperations) / testDurationSeconds;
    
    LOG_INFO("test", "Performance stability test completed:");
    LOG_INFO("test", "  Total operations: {}", totalOperations);
    LOG_INFO("test", "  Success rate: {:.2f}%", successRate * 100);
    LOG_INFO("test", "  Throughput: {:.2f} ops/sec", throughput);
    LOG_INFO("test", "  Avg operation time: {} μs", avgOperationTime.count());
    LOG_INFO("test", "  Max operation time: {} μs", maxOperationTime.count());
    LOG_INFO("test", "  Memory usage: {} -> {} bytes", initialMemory, finalMemory);
    
    // 验证性能要求
    EXPECT_GT(successRate, 0.85) << "Success rate too low during stability test";
    EXPECT_LT(avgOperationTime.count(), 100000) << "Average operation time too high: " << avgOperationTime.count() << " μs";
    EXPECT_LT(maxOperationTime.count(), 1000000) << "Maximum operation time too high: " << maxOperationTime.count() << " μs";
    
    // 验证内存稳定性
    uint64_t memoryIncrease = finalMemory > initialMemory ? finalMemory - initialMemory : 0;
    EXPECT_LT(memoryIncrease, 50 * 1024 * 1024) << "Memory usage increased too much: " << memoryIncrease << " bytes";
    
    // 验证最终系统状态
    EXPECT_TRUE(VerifySystemState()) << "Final system state verification failed";
}

/**
 * 测试多职业兼容性
 * 验证系统对所有职业的支持
 */
TEST_F(EndToEndIntegrationTest, MultiClassCompatibilityTest)
{
    LOG_INFO("test", "Testing multi-class compatibility");
    
    std::map<uint8, int> classSuccessCount;
    std::map<uint8, int> classTotalCount;
    
    // 为每个职业测试装备切换
    for (auto& player : mockPlayers_)
    {
        classTotalCount[player.classId]++;
        
        bool success = SimulateBattlegroundEntry(player);
        if (success)
        {
            classSuccessCount[player.classId]++;
            
            // 验证推荐的装备是否适合该职业
            for (const auto& recommendation : player.pvpRecommendations)
            {
                bool isValidForClass = equipmentValidator_->ValidateEquipment(
                    recommendation.itemId, player.classId, player.level);
                
                EXPECT_TRUE(isValidForClass) 
                    << "Invalid equipment " << recommendation.itemId 
                    << " recommended for class " << static_cast<int>(player.classId);
            }
            
            // 退出战场
            SimulateBattlegroundExit(player);
        }
    }
    
    // 验证每个职业的成功率
    for (const auto& [classId, total] : classTotalCount)
    {
        int successful = classSuccessCount[classId];
        double successRate = static_cast<double>(successful) / total;
        
        LOG_INFO("test", "Class {} success rate: {:.2f}% ({}/{})", 
                static_cast<int>(classId), successRate * 100, successful, total);
        
        EXPECT_GT(successRate, 0.7) 
            << "Success rate too low for class " << static_cast<int>(classId) 
            << ": " << successRate * 100 << "%";
    }
    
    // 验证所有职业都被测试了
    EXPECT_GE(classTotalCount.size(), 9) << "Not all classes were tested";
}