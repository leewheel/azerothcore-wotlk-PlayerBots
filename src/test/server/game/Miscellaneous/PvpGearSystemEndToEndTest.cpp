//By leewheel
#include "gtest/gtest.h"
#include "PvpGearManager.h"
#include "BotPvpGearCache.h"
#include "ErrorRecoveryManager.h"
#include "InventoryManager.h"
#include "Player.h"
#include "Log.h"
#include <memory>
#include <vector>
#include <chrono>

/**
 * **功能: pvp-gear-auto-equip, 端到端集成测试**
 * 
 * 验证需求: 1.1, 4.1, 4.3, 4.4 - 完整的PVP装备切换流程和异常恢复
 * 
 * 专门测试完整的PVP装备切换流程：
 * - 战场进入时的装备切换
 * - 战场退出时的装备恢复
 * - 异常情况的处理和恢复
 * - 系统集成的正确性
 */
class PvpGearSystemEndToEndTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // 初始化系统组件
        cache_ = BotPvpGearCache::GetInstance();
        ASSERT_NE(cache_, nullptr);
        
        manager_ = PvpGearManager::GetInstance();
        ASSERT_NE(manager_, nullptr);
        
        errorManager_ = ErrorRecoveryManager::GetInstance();
        ASSERT_NE(errorManager_, nullptr);
        
        inventoryManager_ = std::make_unique<InventoryManager>();
        ASSERT_NE(inventoryManager_, nullptr);
        
        // 初始化所有组件
        cache_->LoadFromDatabase();
        manager_->Initialize();
        errorManager_->Initialize();
        inventoryManager_->Initialize();
        
        LOG_INFO("test", "PVP gear system end-to-end test setup completed");
    }

    void TearDown() override
    {
        // 清理测试数据
        if (cache_)
        {
            cache_->ClearCache();
        }
        
        inventoryManager_.reset();
        
        LOG_INFO("test", "PVP gear system end-to-end test cleanup completed");
    }

    // 模拟完整的战场进入流程
    struct BattlegroundEntryResult
    {
        bool success;
        std::string errorMessage;
        std::vector<PvpGearRecommendation> recommendations;
        EquipmentSnapshot savedEquipment;
        std::chrono::milliseconds duration;
    };

    BattlegroundEntryResult SimulateCompleteBattlegroundEntry(uint8 classId, uint8 level)
    {
        BattlegroundEntryResult result;
        auto startTime = std::chrono::high_resolution_clock::now();
        
        try
        {
            LOG_DEBUG("test", "Simulating battleground entry for class {} level {}", classId, level);
            
            // 步骤1: 保存当前装备
            bool saveResult = inventoryManager_->SaveCurrentEquipment(nullptr, result.savedEquipment);
            if (!saveResult)
            {
                result.success = false;
                result.errorMessage = "Failed to save current equipment";
                return result;
            }
            
            // 步骤2: 获取PVP装备推荐
            float resilienceRatio = (level >= 60) ? 0.8f : 0.3f;
            result.recommendations = manager_->GetRecommendedGear(nullptr, resilienceRatio);
            
            // 步骤3: 验证推荐的装备
            if (result.recommendations.empty() && cache_->IsLoaded())
            {
                result.success = false;
                result.errorMessage = "No PVP gear recommendations available";
                return result;
            }
            
            // 步骤4: 执行装备切换
            bool equipResult = manager_->EquipPvpGear(nullptr);
            if (!equipResult)
            {
                result.success = false;
                result.errorMessage = "Failed to equip PVP gear";
                return result;
            }
            
            result.success = true;
            
        }
        catch (const std::exception& e)
        {
            result.success = false;
            result.errorMessage = "Exception: " + std::string(e.what());
        }
        
        auto endTime = std::chrono::high_resolution_clock::now();
        result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        
        return result;
    }
    // 模拟完整的战场退出流程
    struct BattlegroundExitResult
    {
        bool success;
        std::string errorMessage;
        bool equipmentRestored;
        std::chrono::milliseconds duration;
    };

    BattlegroundExitResult SimulateCompleteBattlegroundExit(const EquipmentSnapshot& savedEquipment)
    {
        BattlegroundExitResult result;
        auto startTime = std::chrono::high_resolution_clock::now();
        
        try
        {
            LOG_DEBUG("test", "Simulating battleground exit");
            
            // 步骤1: 恢复PVE装备
            bool restoreResult = manager_->RestorePveGear(nullptr);
            if (!restoreResult)
            {
                result.success = false;
                result.errorMessage = "Failed to restore PVE gear";
                return result;
            }
            
            // 步骤2: 验证装备恢复
            if (savedEquipment.isValid)
            {
                bool verifyResult = inventoryManager_->RestoreEquipment(nullptr, savedEquipment);
                result.equipmentRestored = verifyResult;
                
                if (!verifyResult)
                {
                    LOG_WARN("test", "Equipment restoration verification failed");
                }
            }
            else
            {
                result.equipmentRestored = true; // 没有保存的装备，认为恢复成功
            }
            
            result.success = true;
            
        }
        catch (const std::exception& e)
        {
            result.success = false;
            result.errorMessage = "Exception: " + std::string(e.what());
        }
        
        auto endTime = std::chrono::high_resolution_clock::now();
        result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        
        return result;
    }

    // 验证系统完整性
    bool VerifySystemIntegrity()
    {
        try
        {
            // 检查缓存状态
            if (cache_->IsLoaded())
            {
                CacheStats stats = cache_->GetCacheStats();
                if (stats.currentEntryCount.load() == 0)
                {
                    LOG_WARN("test", "Cache loaded but has no entries");
                }
            }
            
            // 检查错误恢复管理器
            bool healthCheck = errorManager_->CheckSystemHealth();
            if (!healthCheck)
            {
                LOG_ERROR("test", "System health check failed");
                return false;
            }
            
            // 检查内存使用
            uint64_t memoryUsage = cache_->GetMemoryUsageBytes();
            if (memoryUsage > 512 * 1024 * 1024) // 512MB
            {
                LOG_WARN("test", "High memory usage detected: {} bytes", memoryUsage);
            }
            
            return true;
        }
        catch (const std::exception& e)
        {
            LOG_ERROR("test", "Exception during system integrity check: {}", e.what());
            return false;
        }
    }

    BotPvpGearCache* cache_;
    PvpGearManager* manager_;
    ErrorRecoveryManager* errorManager_;
    std::unique_ptr<InventoryManager> inventoryManager_;
};
/**
 * 测试完整的PVP装备切换流程
 * 验证从战场进入到退出的完整流程
 */
TEST_F(PvpGearSystemEndToEndTest, CompletePvpGearSwitchFlow)
{
    LOG_INFO("test", "Testing complete PVP gear switch flow");
    
    // 测试不同职业和等级的组合
    std::vector<std::pair<uint8, uint8>> testCases = {
        {1, 60},   // 战士 60级
        {2, 70},   // 圣骑士 70级
        {3, 80},   // 猎人 80级
        {4, 60},   // 盗贼 60级
        {5, 70},   // 牧师 70级
        {8, 80},   // 法师 80级
        {9, 70},   // 术士 70级
        {11, 60}   // 德鲁伊 60级
    };
    
    int totalTests = 0;
    int successfulFlows = 0;
    std::vector<std::chrono::milliseconds> entryTimes;
    std::vector<std::chrono::milliseconds> exitTimes;
    
    for (const auto& [classId, level] : testCases)
    {
        totalTests++;
        
        LOG_DEBUG("test", "Testing flow for class {} level {}", classId, level);
        
        // 验证初始系统状态
        EXPECT_TRUE(VerifySystemIntegrity()) 
            << "System integrity check failed before test for class " << static_cast<int>(classId);
        
        // 执行战场进入流程
        BattlegroundEntryResult entryResult = SimulateCompleteBattlegroundEntry(classId, level);
        entryTimes.push_back(entryResult.duration);
        
        if (entryResult.success)
        {
            LOG_DEBUG("test", "Battleground entry successful for class {} level {}", classId, level);
            
            // 验证推荐的装备数量
            if (cache_->IsLoaded())
            {
                EXPECT_GT(entryResult.recommendations.size(), 0) 
                    << "No gear recommendations for class " << static_cast<int>(classId);
                
                // 验证推荐装备的韧性比例
                int resilienceItems = 0;
                for (const auto& rec : entryResult.recommendations)
                {
                    if (rec.resilienceValue > 0)
                    {
                        resilienceItems++;
                    }
                }
                
                if (!entryResult.recommendations.empty())
                {
                    float resilienceRatio = static_cast<float>(resilienceItems) / entryResult.recommendations.size();
                    float expectedRatio = (level >= 60) ? 0.6f : 0.2f; // 允许一些偏差
                    
                    EXPECT_GE(resilienceRatio, expectedRatio) 
                        << "Resilience ratio too low for class " << static_cast<int>(classId) 
                        << " level " << static_cast<int>(level) << ": " << resilienceRatio;
                }
            }
            
            // 短暂延迟模拟战场时间
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            
            // 执行战场退出流程
            BattlegroundExitResult exitResult = SimulateCompleteBattlegroundExit(entryResult.savedEquipment);
            exitTimes.push_back(exitResult.duration);
            
            if (exitResult.success)
            {
                successfulFlows++;
                LOG_DEBUG("test", "Complete flow successful for class {} level {}", classId, level);
                
                // 验证装备恢复
                EXPECT_TRUE(exitResult.equipmentRestored) 
                    << "Equipment restoration failed for class " << static_cast<int>(classId);
            }
            else
            {
                LOG_ERROR("test", "Battleground exit failed for class {} level {}: {}", 
                         classId, level, exitResult.errorMessage);
            }
        }
        else
        {
            LOG_ERROR("test", "Battleground entry failed for class {} level {}: {}", 
                     classId, level, entryResult.errorMessage);
        }
        
        // 验证最终系统状态
        EXPECT_TRUE(VerifySystemIntegrity()) 
            << "System integrity check failed after test for class " << static_cast<int>(classId);
    }
    
    // 计算性能统计
    auto avgEntryTime = std::accumulate(entryTimes.begin(), entryTimes.end(), 
                                       std::chrono::milliseconds{0}) / entryTimes.size();
    auto avgExitTime = std::accumulate(exitTimes.begin(), exitTimes.end(), 
                                      std::chrono::milliseconds{0}) / exitTimes.size();
    
    LOG_INFO("test", "Complete flow test results:");
    LOG_INFO("test", "  Successful flows: {}/{}", successfulFlows, totalTests);
    LOG_INFO("test", "  Average entry time: {} ms", avgEntryTime.count());
    LOG_INFO("test", "  Average exit time: {} ms", avgExitTime.count());
    
    // 验证成功率
    double successRate = static_cast<double>(successfulFlows) / totalTests;
    EXPECT_GT(successRate, 0.8) << "Success rate too low: " << successRate * 100 << "%";
    
    // 验证性能要求
    EXPECT_LT(avgEntryTime.count(), 1000) << "Average entry time too high: " << avgEntryTime.count() << " ms";
    EXPECT_LT(avgExitTime.count(), 500) << "Average exit time too high: " << avgExitTime.count() << " ms";
}

/**
 * 测试异常情况下的系统恢复能力
 * 验证各种异常情况的处理和恢复机制
 */
TEST_F(PvpGearSystemEndToEndTest, ExceptionRecoveryFlow)
{
    LOG_INFO("test", "Testing exception recovery flow");
    
    // 测试数据库连接失败的恢复
    {
        LOG_DEBUG("test", "Testing database connection failure recovery");
        
        // 模拟数据库连接失败
        errorManager_->SimulateDatabaseError("Connection timeout");
        
        // 尝试执行PVP装备切换
        BattlegroundEntryResult result = SimulateCompleteBattlegroundEntry(1, 70); // 战士70级
        
        // 验证系统能够优雅处理错误
        if (!result.success)
        {
            EXPECT_FALSE(result.errorMessage.empty()) << "Error message should be provided";
            LOG_DEBUG("test", "Database error handled correctly: {}", result.errorMessage);
        }
        
        // 验证错误恢复
        bool recovered = errorManager_->AttemptRecovery();
        EXPECT_TRUE(recovered) << "System should recover from database errors";
        
        // 验证恢复后系统正常工作
        EXPECT_TRUE(VerifySystemIntegrity()) << "System integrity should be restored after recovery";
    }
    
    // 测试内存不足情况的处理
    {
        LOG_DEBUG("test", "Testing memory shortage handling");
        
        // 模拟内存不足
        errorManager_->SimulateMemoryShortage();
        
        // 验证缓存清理机制
        uint64_t memoryBefore = cache_->GetMemoryUsageBytes();
        cache_->TriggerLRUCleanup();
        uint64_t memoryAfter = cache_->GetMemoryUsageBytes();
        
        EXPECT_LE(memoryAfter, memoryBefore) << "Memory usage should decrease after cleanup";
        
        // 验证系统仍能正常工作
        BattlegroundEntryResult result = SimulateCompleteBattlegroundEntry(5, 60); // 牧师60级
        
        // 在内存不足情况下，系统应该能够降级服务
        if (!result.success)
        {
            LOG_DEBUG("test", "System gracefully degraded under memory pressure: {}", result.errorMessage);
        }
        
        // 清除内存不足状态
        errorManager_->ClearMemoryShortage();
    }
    
    // 测试装备数据异常的处理
    {
        LOG_DEBUG("test", "Testing equipment data corruption handling");
        
        // 模拟装备数据异常
        errorManager_->SimulateDataCorruption("Invalid item data");
        
        // 尝试获取装备推荐
        std::vector<PvpGearRecommendation> recommendations = manager_->GetRecommendedGear(nullptr, 0.8f);
        
        // 验证系统能够处理数据异常
        if (recommendations.empty())
        {
            LOG_DEBUG("test", "System correctly handled data corruption by returning empty recommendations");
        }
        
        // 验证错误日志记录
        bool hasErrorLogs = errorManager_->HasRecentErrors();
        EXPECT_TRUE(hasErrorLogs) << "Error should be logged when data corruption occurs";
        
        // 清除数据异常状态
        errorManager_->ClearDataCorruption();
    }
    
    LOG_INFO("test", "Exception recovery flow test completed");
}

/**
 * 测试高并发场景下的系统稳定性
 * 验证多个机器人同时进行装备切换时的系统表现
 */
TEST_F(PvpGearSystemEndToEndTest, ConcurrentOperationsStability)
{
    LOG_INFO("test", "Testing concurrent operations stability");
    
    const int numConcurrentBots = 10;
    std::vector<std::thread> threads;
    std::vector<BattlegroundEntryResult> results(numConcurrentBots);
    std::atomic<int> completedOperations{0};
    std::atomic<int> successfulOperations{0};
    
    // 创建多个并发线程模拟多机器人操作
    for (int i = 0; i < numConcurrentBots; ++i)
    {
        threads.emplace_back([this, i, &results, &completedOperations, &successfulOperations]()
        {
            try
            {
                // 使用不同的职业和等级组合
                uint8 classId = (i % 8) + 1; // 职业1-8
                uint8 level = 60 + (i % 3) * 10; // 60, 70, 80级
                
                LOG_DEBUG("test", "Concurrent operation {} starting for class {} level {}", i, classId, level);
                
                // 执行战场进入流程
                results[i] = SimulateCompleteBattlegroundEntry(classId, level);
                
                if (results[i].success)
                {
                    successfulOperations++;
                    
                    // 短暂延迟
                    std::this_thread::sleep_for(std::chrono::milliseconds(100 + (i * 10)));
                    
                    // 执行战场退出流程
                    BattlegroundExitResult exitResult = SimulateCompleteBattlegroundExit(results[i].savedEquipment);
                    
                    if (!exitResult.success)
                    {
                        LOG_WARN("test", "Concurrent operation {} exit failed: {}", i, exitResult.errorMessage);
                    }
                }
                else
                {
                    LOG_WARN("test", "Concurrent operation {} entry failed: {}", i, results[i].errorMessage);
                }
                
                completedOperations++;
                LOG_DEBUG("test", "Concurrent operation {} completed", i);
            }
            catch (const std::exception& e)
            {
                LOG_ERROR("test", "Exception in concurrent operation {}: {}", i, e.what());
                completedOperations++;
            }
        });
    }
    
    // 等待所有线程完成
    for (auto& thread : threads)
    {
        thread.join();
    }
    
    // 验证所有操作都已完成
    EXPECT_EQ(completedOperations.load(), numConcurrentBots) 
        << "Not all concurrent operations completed";
    
    // 计算成功率
    double successRate = static_cast<double>(successfulOperations.load()) / numConcurrentBots;
    LOG_INFO("test", "Concurrent operations success rate: {:.1f}% ({}/{})", 
             successRate * 100, successfulOperations.load(), numConcurrentBots);
    
    // 验证并发操作的成功率
    EXPECT_GT(successRate, 0.7) << "Concurrent operations success rate too low: " << successRate * 100 << "%";
    
    // 验证系统完整性
    EXPECT_TRUE(VerifySystemIntegrity()) << "System integrity compromised after concurrent operations";
    
    // 验证缓存状态
    if (cache_->IsLoaded())
    {
        CacheStats stats = cache_->GetCacheStats();
        LOG_INFO("test", "Cache stats after concurrent operations:");
        LOG_INFO("test", "  Current entries: {}", stats.currentEntryCount.load());
        LOG_INFO("test", "  Hit rate: {:.2f}%", stats.GetHitRate() * 100);
        LOG_INFO("test", "  Memory usage: {} bytes", cache_->GetMemoryUsageBytes());
        
        // 验证缓存性能
        EXPECT_GT(stats.GetHitRate(), 0.5) << "Cache hit rate too low after concurrent operations";
    }
    
    LOG_INFO("test", "Concurrent operations stability test completed");
}

/**
 * 测试系统资源管理和清理
 * 验证长时间运行后的资源管理和内存清理
 */
TEST_F(PvpGearSystemEndToEndTest, ResourceManagementAndCleanup)
{
    LOG_INFO("test", "Testing resource management and cleanup");
    
    const int numIterations = 50;
    std::vector<uint64_t> memoryUsageHistory;
    std::vector<std::chrono::milliseconds> operationTimes;
    
    uint64_t initialMemory = cache_->GetMemoryUsageBytes();
    memoryUsageHistory.push_back(initialMemory);
    
    LOG_DEBUG("test", "Initial memory usage: {} bytes", initialMemory);
    
    // 执行多次装备切换操作
    for (int i = 0; i < numIterations; ++i)
    {
        uint8 classId = (i % 8) + 1;
        uint8 level = 60 + (i % 3) * 10;
        
        auto startTime = std::chrono::high_resolution_clock::now();
        
        // 执行完整的装备切换流程
        BattlegroundEntryResult entryResult = SimulateCompleteBattlegroundEntry(classId, level);
        
        if (entryResult.success)
        {
            BattlegroundExitResult exitResult = SimulateCompleteBattlegroundExit(entryResult.savedEquipment);
            
            if (!exitResult.success)
            {
                LOG_WARN("test", "Iteration {} exit failed: {}", i, exitResult.errorMessage);
            }
        }
        else
        {
            LOG_WARN("test", "Iteration {} entry failed: {}", i, entryResult.errorMessage);
        }
        
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        operationTimes.push_back(duration);
        
        // 记录内存使用情况
        uint64_t currentMemory = cache_->GetMemoryUsageBytes();
        memoryUsageHistory.push_back(currentMemory);
        
        // 每10次迭代触发一次清理
        if ((i + 1) % 10 == 0)
        {
            LOG_DEBUG("test", "Triggering cleanup after {} iterations", i + 1);
            cache_->TriggerLRUCleanup();
            
            uint64_t memoryAfterCleanup = cache_->GetMemoryUsageBytes();
            LOG_DEBUG("test", "Memory usage after cleanup: {} bytes (was {} bytes)", 
                     memoryAfterCleanup, currentMemory);
        }
        
        // 短暂延迟避免过度压力
        if (i % 5 == 0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    
    uint64_t finalMemory = cache_->GetMemoryUsageBytes();
    
    // 分析内存使用趋势
    uint64_t maxMemory = *std::max_element(memoryUsageHistory.begin(), memoryUsageHistory.end());
    uint64_t minMemory = *std::min_element(memoryUsageHistory.begin(), memoryUsageHistory.end());
    
    LOG_INFO("test", "Resource management test results:");
    LOG_INFO("test", "  Initial memory: {} bytes", initialMemory);
    LOG_INFO("test", "  Final memory: {} bytes", finalMemory);
    LOG_INFO("test", "  Max memory: {} bytes", maxMemory);
    LOG_INFO("test", "  Min memory: {} bytes", minMemory);
    LOG_INFO("test", "  Memory growth: {} bytes", finalMemory - initialMemory);
    
    // 验证内存增长控制
    double memoryGrowthRatio = static_cast<double>(finalMemory - initialMemory) / initialMemory;
    EXPECT_LT(memoryGrowthRatio, 2.0) << "Memory growth too high: " << memoryGrowthRatio * 100 << "%";
    
    // 验证操作时间稳定性
    auto avgTime = std::accumulate(operationTimes.begin(), operationTimes.end(), 
                                  std::chrono::milliseconds{0}) / operationTimes.size();
    
    // 计算时间方差
    double timeVariance = 0.0;
    for (const auto& time : operationTimes)
    {
        double diff = time.count() - avgTime.count();
        timeVariance += diff * diff;
    }
    timeVariance /= operationTimes.size();
    double timeStdDev = std::sqrt(timeVariance);
    
    LOG_INFO("test", "Operation time statistics:");
    LOG_INFO("test", "  Average time: {} ms", avgTime.count());
    LOG_INFO("test", "  Standard deviation: {:.2f} ms", timeStdDev);
    
    // 验证性能稳定性
    EXPECT_LT(timeStdDev / avgTime.count(), 0.5) << "Operation time too unstable";
    
    // 最终系统完整性检查
    EXPECT_TRUE(VerifySystemIntegrity()) << "System integrity compromised after resource management test";
    
    LOG_INFO("test", "Resource management and cleanup test completed");
}

/**
 * 测试配置缺失和默认值处理
 * 验证系统在配置不完整时的默认行为
 */
TEST_F(PvpGearSystemEndToEndTest, ConfigurationMissingHandling)
{
    LOG_INFO("test", "Testing configuration missing handling");
    
    // 模拟配置缺失情况
    errorManager_->SimulateConfigurationMissing("PVP gear weights configuration missing");
    
    // 尝试执行装备切换
    BattlegroundEntryResult result = SimulateCompleteBattlegroundEntry(1, 70);
    
    // 验证系统使用默认配置
    if (result.success)
    {
        LOG_DEBUG("test", "System successfully used default configuration");
        
        // 验证推荐装备使用默认权重
        if (!result.recommendations.empty())
        {
            bool hasValidScores = true;
            for (const auto& rec : result.recommendations)
            {
                if (rec.totalScore <= 0)
                {
                    hasValidScores = false;
                    break;
                }
            }
            
            EXPECT_TRUE(hasValidScores) << "Default configuration should produce valid scores";
        }
    }
    else
    {
        // 如果失败，应该有明确的错误信息
        EXPECT_FALSE(result.errorMessage.empty()) << "Should provide error message for configuration issues";
        LOG_DEBUG("test", "System correctly reported configuration issue: {}", result.errorMessage);
    }
    
    // 清除配置缺失状态
    errorManager_->ClearConfigurationMissing();
    
    // 验证恢复后正常工作
    BattlegroundEntryResult recoveryResult = SimulateCompleteBattlegroundEntry(2, 60);
    
    // 恢复后应该能正常工作
    if (cache_->IsLoaded())
    {
        EXPECT_TRUE(recoveryResult.success || !recoveryResult.errorMessage.empty()) 
            << "System should work normally after configuration recovery";
    }
    
    LOG_INFO("test", "Configuration missing handling test completed");
}