//By leewheel
#include "gtest/gtest.h"
#include "PvpGearManager.h"
#include "BotPvpGearCache.h"
#include "ErrorRecoveryManager.h"
#include "Player.h"
#include "Log.h"
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <random>
#include <future>
#include <mutex>

/**
 * **功能: pvp-gear-auto-equip, 并发测试场景**
 * 
 * 验证需求: 8.1, 8.2 - 并发性能和稳定性
 * 
 * 测试多机器人同时进入战场时的系统行为：
 * - 并发装备切换性能
 * - 系统稳定性和线程安全
 * - 资源竞争处理
 * - 错误恢复机制
 */
class ConcurrentBattlegroundTest : public ::testing::Test
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
        
        // 初始化组件
        cache_->LoadFromDatabase();
        manager_->Initialize();
        errorManager_->Initialize();
        
        // 初始化随机数生成器
        rng_.seed(std::chrono::steady_clock::now().time_since_epoch().count());
        
        // 重置统计计数器
        successCount_.store(0);
        failureCount_.store(0);
        totalOperationTime_.store(0);
    }

    void TearDown() override
    {
        // 清理测试数据
        if (cache_)
        {
            cache_->ClearCache();
        }
        
        // 输出测试统计信息
        LOG_INFO("test", "Test completed - Success: {}, Failures: {}, Avg time: {} ms", 
                successCount_.load(), failureCount_.load(), 
                totalOperationTime_.load() / std::max(1, successCount_.load() + failureCount_.load()));
    }

    // 模拟玩家数据结构（简化版本）
    struct MockPlayer
    {
        uint32 guid;
        uint8 classId;
        uint8 level;
        std::string name;
        bool inBattleground;
        
        MockPlayer(uint32 g, uint8 c, uint8 l, const std::string& n) 
            : guid(g), classId(c), level(l), name(n), inBattleground(false) {}
    };

    // 创建模拟玩家
    std::vector<MockPlayer> CreateMockPlayers(int count)
    {
        std::vector<MockPlayer> players;
        players.reserve(count);
        
        for (int i = 0; i < count; ++i)
        {
            uint32 guid = 1000 + i;
            uint8 classId = (rng_() % 11) + 1;  // 1-11
            uint8 level = (rng_() % 40) + 40;   // 40-80
            std::string name = "TestBot" + std::to_string(i);
            
            players.emplace_back(guid, classId, level, name);
        }
        
        return players;
    }

    // 模拟装备切换操作
    bool SimulateEquipmentSwitch(const MockPlayer& player, bool enterBattleground)
    {
        try
        {
            auto startTime = std::chrono::high_resolution_clock::now();
            
            // 模拟装备切换逻辑
            if (enterBattleground)
            {
                // 获取推荐的PVP装备
                std::vector<PvpGearRecommendation> recommendations = 
                    manager_->GetRecommendedGear(nullptr, 0.8f); // 使用nullptr作为Player*的占位符
                
                // 模拟装备过程
                std::this_thread::sleep_for(std::chrono::milliseconds(rng_() % 50 + 10)); // 10-60ms
            }
            else
            {
                // 模拟恢复PVE装备
                std::this_thread::sleep_for(std::chrono::milliseconds(rng_() % 30 + 5)); // 5-35ms
            }
            
            auto endTime = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
            
            totalOperationTime_.fetch_add(duration.count());
            return true;
        }
        catch (const std::exception& e)
        {
            LOG_ERROR("test", "Equipment switch failed for player {}: {}", player.name, e.what());
            return false;
        }
    }

    BotPvpGearCache* cache_;
    PvpGearManager* manager_;
    ErrorRecoveryManager* errorManager_;
    std::mt19937 rng_;
    
    // 统计计数器
    std::atomic<int> successCount_{0};
    std::atomic<int> failureCount_{0};
    std::atomic<long long> totalOperationTime_{0};
    
    // 同步原语
    std::mutex logMutex_;
};

/**
 * 测试基本并发装备切换
 * 验证多个机器人同时进行装备切换的基本功能
 */
TEST_F(ConcurrentBattlegroundTest, BasicConcurrentEquipmentSwitch)
{
    LOG_INFO("test", "Testing basic concurrent equipment switch");
    
    const int numBots = 10;
    const int numOperations = 5;
    
    std::vector<MockPlayer> players = CreateMockPlayers(numBots);
    std::vector<std::thread> threads;
    
    // 启动多个线程模拟并发装备切换
    for (int i = 0; i < numBots; ++i)
    {
        threads.emplace_back([this, &players, i, numOperations]() {
            for (int op = 0; op < numOperations; ++op)
            {
                bool enterBG = (op % 2 == 0); // 交替进入和离开战场
                
                if (SimulateEquipmentSwitch(players[i], enterBG))
                {
                    successCount_.fetch_add(1);
                    
                    std::lock_guard<std::mutex> lock(logMutex_);
                    LOG_DEBUG("test", "Bot {} {} battleground successfully", 
                             players[i].name, enterBG ? "entered" : "left");
                }
                else
                {
                    failureCount_.fetch_add(1);
                }
                
                // 随机延迟模拟真实场景
                std::this_thread::sleep_for(std::chrono::milliseconds(rng_() % 100));
            }
        });
    }
    
    // 等待所有线程完成
    for (auto& thread : threads)
    {
        thread.join();
    }
    
    int totalOperations = numBots * numOperations;
    int actualOperations = successCount_.load() + failureCount_.load();
    
    LOG_INFO("test", "Concurrent test completed: {}/{} operations successful", 
             successCount_.load(), totalOperations);
    
    // 验证结果
    EXPECT_EQ(actualOperations, totalOperations);
    EXPECT_GT(successCount_.load(), totalOperations * 0.9); // 至少90%成功率
    
    // 验证平均响应时间
    if (actualOperations > 0)
    {
        long long avgTime = totalOperationTime_.load() / actualOperations;
        EXPECT_LT(avgTime, 200); // 平均响应时间应该在200ms内
        LOG_INFO("test", "Average operation time: {} ms", avgTime);
    }
}

/**
 * 测试高并发场景
 * 验证系统在高并发负载下的稳定性
 */
TEST_F(ConcurrentBattlegroundTest, HighConcurrencyStressTest)
{
    LOG_INFO("test", "Testing high concurrency stress scenario");
    
    const int numBots = 50;
    const int numOperations = 10;
    
    std::vector<MockPlayer> players = CreateMockPlayers(numBots);
    std::vector<std::future<int>> futures;
    
    // 使用异步任务来创建更高的并发度
    for (int i = 0; i < numBots; ++i)
    {
        futures.push_back(std::async(std::launch::async, [this, &players, i, numOperations]() -> int {
            int localSuccess = 0;
            
            for (int op = 0; op < numOperations; ++op)
            {
                bool enterBG = (op % 2 == 0);
                
                if (SimulateEquipmentSwitch(players[i], enterBG))
                {
                    localSuccess++;
                    successCount_.fetch_add(1);
                }
                else
                {
                    failureCount_.fetch_add(1);
                }
                
                // 更短的延迟以增加并发压力
                std::this_thread::sleep_for(std::chrono::milliseconds(rng_() % 20));
            }
            
            return localSuccess;
        }));
    }
    
    // 等待所有异步任务完成
    int totalLocalSuccess = 0;
    for (auto& future : futures)
    {
        totalLocalSuccess += future.get();
    }
    
    int totalOperations = numBots * numOperations;
    
    LOG_INFO("test", "High concurrency test completed: {}/{} operations successful", 
             successCount_.load(), totalOperations);
    
    // 验证结果
    EXPECT_EQ(totalLocalSuccess, successCount_.load());
    EXPECT_GT(successCount_.load(), totalOperations * 0.85); // 至少85%成功率（高并发下要求稍低）
    
    // 验证系统没有崩溃或死锁
    EXPECT_TRUE(cache_->IsLoaded());
    EXPECT_TRUE(errorManager_->CheckSystemHealth());
}

/**
 * 测试资源竞争处理
 * 验证系统在资源竞争情况下的行为
 */
TEST_F(ConcurrentBattlegroundTest, ResourceContentionTest)
{
    LOG_INFO("test", "Testing resource contention handling");
    
    const int numThreads = 20;
    const int operationsPerThread = 20;
    
    std::vector<std::thread> threads;
    std::atomic<int> cacheHits{0};
    std::atomic<int> cacheMisses{0};
    
    // 创建多个线程同时访问相同的资源
    for (int i = 0; i < numThreads; ++i)
    {
        threads.emplace_back([this, i, operationsPerThread, &cacheHits, &cacheMisses]() {
            for (int op = 0; op < operationsPerThread; ++op)
            {
                // 所有线程都查询相同的职业和等级，增加资源竞争
                uint8_t classId = 1; // 战士
                uint8_t level = 70;
                
                auto startTime = std::chrono::high_resolution_clock::now();
                
                std::vector<PvpGearItem> gear = cache_->GetAvailableGear(classId, level, "");
                
                auto endTime = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
                
                if (!gear.empty())
                {
                    cacheHits.fetch_add(1);
                    successCount_.fetch_add(1);
                }
                else
                {
                    cacheMisses.fetch_add(1);
                    failureCount_.fetch_add(1);
                }
                
                totalOperationTime_.fetch_add(duration.count() / 1000); // 转换为毫秒
                
                // 短暂延迟
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        });
    }
    
    // 等待所有线程完成
    for (auto& thread : threads)
    {
        thread.join();
    }
    
    int totalOperations = numThreads * operationsPerThread;
    
    LOG_INFO("test", "Resource contention test completed: Hits: {}, Misses: {}, Total: {}", 
             cacheHits.load(), cacheMisses.load(), totalOperations);
    
    // 验证结果
    EXPECT_EQ(cacheHits.load() + cacheMisses.load(), totalOperations);
    
    // 在资源竞争情况下，响应时间可能会增加，但应该保持在合理范围内
    if (totalOperations > 0)
    {
        long long avgTime = totalOperationTime_.load() / totalOperations;
        EXPECT_LT(avgTime, 100); // 平均响应时间应该在100ms内
        LOG_INFO("test", "Average response time under contention: {} ms", avgTime);
    }
}

/**
 * 测试错误恢复机制的并发性
 * 验证错误恢复系统在并发环境下的正确性
 */
TEST_F(ConcurrentBattlegroundTest, ConcurrentErrorRecoveryTest)
{
    LOG_INFO("test", "Testing concurrent error recovery");
    
    const int numThreads = 15;
    std::vector<std::thread> threads;
    std::atomic<int> errorsHandled{0};
    std::atomic<int> recoveryAttempts{0};
    
    // 创建多个线程同时触发和处理错误
    for (int i = 0; i < numThreads; ++i)
    {
        threads.emplace_back([this, i, &errorsHandled, &recoveryAttempts]() {
            for (int op = 0; op < 10; ++op)
            {
                try
                {
                    // 模拟各种类型的错误
                    ErrorType errorType = static_cast<ErrorType>(rng_() % 7); // 0-6
                    std::string errorMsg = "Concurrent test error " + std::to_string(i) + "_" + std::to_string(op);
                    std::string context = "Thread " + std::to_string(i);
                    
                    // 触发错误处理
                    bool handled = errorManager_->HandleError(errorType, ErrorSeverity::MEDIUM, errorMsg, context);
                    
                    if (handled)
                    {
                        errorsHandled.fetch_add(1);
                        successCount_.fetch_add(1);
                    }
                    else
                    {
                        failureCount_.fetch_add(1);
                    }
                    
                    recoveryAttempts.fetch_add(1);
                    
                    // 随机延迟
                    std::this_thread::sleep_for(std::chrono::milliseconds(rng_() % 50));
                }
                catch (const std::exception& e)
                {
                    LOG_ERROR("test", "Exception in error recovery test: {}", e.what());
                    failureCount_.fetch_add(1);
                }
            }
        });
    }
    
    // 等待所有线程完成
    for (auto& thread : threads)
    {
        thread.join();
    }
    
    LOG_INFO("test", "Concurrent error recovery test completed: {}/{} errors handled", 
             errorsHandled.load(), recoveryAttempts.load());
    
    // 验证错误恢复机制的有效性
    EXPECT_GT(errorsHandled.load(), recoveryAttempts.load() * 0.8); // 至少80%的错误被成功处理
    
    // 验证系统仍然健康
    EXPECT_TRUE(errorManager_->CheckSystemHealth());
}

/**
 * 测试内存使用的并发安全性
 * 验证并发操作不会导致内存泄漏或损坏
 */
TEST_F(ConcurrentBattlegroundTest, ConcurrentMemoryUsageTest)
{
    LOG_INFO("test", "Testing concurrent memory usage safety");
    
    // 记录初始内存使用
    uint64_t initialMemory = cache_->GetMemoryUsageBytes();
    uint32_t initialEntries = cache_->GetCacheEntryCount();
    
    const int numThreads = 12;
    const int operationsPerThread = 25;
    
    std::vector<std::thread> threads;
    
    // 创建多个线程进行内存密集型操作
    for (int i = 0; i < numThreads; ++i)
    {
        threads.emplace_back([this, i, operationsPerThread]() {
            for (int op = 0; op < operationsPerThread; ++op)
            {
                // 执行各种操作来测试内存安全性
                uint8_t classId = (rng_() % 11) + 1;
                uint8_t level = (rng_() % 60) + 20;
                
                // 查询操作
                std::vector<PvpGearItem> gear = cache_->GetAvailableGear(classId, level, "");
                
                // 统计操作
                CacheStats stats = cache_->GetCacheStats();
                
                // 健康检查
                bool healthy = errorManager_->CheckSystemHealth();
                
                if (healthy && !gear.empty())
                {
                    successCount_.fetch_add(1);
                }
                else
                {
                    failureCount_.fetch_add(1);
                }
                
                // 短暂延迟
                std::this_thread::sleep_for(std::chrono::milliseconds(rng_() % 10));
            }
        });
    }
    
    // 等待所有线程完成
    for (auto& thread : threads)
    {
        thread.join();
    }
    
    // 检查最终内存使用
    uint64_t finalMemory = cache_->GetMemoryUsageBytes();
    uint32_t finalEntries = cache_->GetCacheEntryCount();
    
    LOG_INFO("test", "Memory usage - Initial: {} bytes ({} entries), Final: {} bytes ({} entries)", 
             initialMemory, initialEntries, finalMemory, finalEntries);
    
    // 验证内存使用没有异常增长
    uint64_t memoryIncrease = finalMemory > initialMemory ? finalMemory - initialMemory : 0;
    EXPECT_LT(memoryIncrease, 50 * 1024 * 1024); // 内存增长不应超过50MB
    
    // 验证缓存条目数没有异常变化
    EXPECT_EQ(finalEntries, initialEntries); // 只读操作不应改变条目数
    
    // 验证系统仍然正常工作
    EXPECT_TRUE(cache_->IsLoaded());
    EXPECT_GT(successCount_.load(), numThreads * operationsPerThread * 0.9);
}