//By leewheel
#include "gtest/gtest.h"
#include "BotPvpGearCache.h"
#include "PvpGearManager.h"
#include "ErrorRecoveryManager.h"
#include "Log.h"
#include <chrono>
#include <memory>
#include <random>

/**
 * **功能: pvp-gear-auto-equip, 属性 27: 启动性能要求**
 * 
 * 验证需求: 8.4 - 系统启动性能要求
 * 
 * 测试系统启动性能的各个方面：
 * - 缓存初始化时间
 * - 管理器启动时间
 * - 内存使用效率
 * - 并发启动性能
 */
class SystemStartupPerformanceTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // 记录测试开始时间
        testStartTime_ = std::chrono::high_resolution_clock::now();
        
        // 初始化随机数生成器
        rng_.seed(std::chrono::steady_clock::now().time_since_epoch().count());
    }

    void TearDown() override
    {
        // 清理测试资源
        if (cache_)
        {
            cache_->ClearCache();
        }
        
        auto testEndTime = std::chrono::high_resolution_clock::now();
        auto testDuration = std::chrono::duration_cast<std::chrono::milliseconds>(testEndTime - testStartTime_);
        LOG_INFO("test", "Test completed in {} ms", testDuration.count());
    }

    // 测量执行时间的辅助函数
    template<typename Func>
    std::chrono::milliseconds MeasureExecutionTime(Func&& func)
    {
        auto start = std::chrono::high_resolution_clock::now();
        func();
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    }

    // 获取当前内存使用量（简化版本）
    uint64_t GetCurrentMemoryUsage()
    {
        // 这里应该实现实际的内存使用量获取
        // 作为测试，我们使用缓存的内存统计
        if (cache_)
        {
            return cache_->GetMemoryUsageBytes();
        }
        return 0;
    }

    BotPvpGearCache* cache_ = nullptr;
    PvpGearManager* manager_ = nullptr;
    ErrorRecoveryManager* errorManager_ = nullptr;
    std::chrono::high_resolution_clock::time_point testStartTime_;
    std::mt19937 rng_;
};

/**
 * 属性 27.1: 缓存初始化性能
 * 验证缓存系统能够在规定时间内完成初始化
 */
TEST_F(SystemStartupPerformanceTest, CacheInitializationPerformance)
{
    LOG_INFO("test", "Testing cache initialization performance");
    
    // 获取缓存实例
    cache_ = BotPvpGearCache::GetInstance();
    ASSERT_NE(cache_, nullptr);
    
    // 清理缓存以确保干净的启动状态
    cache_->ClearCache();
    
    uint64_t memoryBefore = GetCurrentMemoryUsage();
    
    // 测量缓存初始化时间
    auto initTime = MeasureExecutionTime([this]() {
        bool initResult = cache_->LoadFromDatabase();
        if (!initResult)
        {
            // 如果数据库加载失败，尝试从本地文件加载
            cache_->LoadCacheFromLocalFile();
        }
    });
    
    uint64_t memoryAfter = GetCurrentMemoryUsage();
    uint64_t memoryUsed = memoryAfter - memoryBefore;
    
    LOG_INFO("test", "Cache initialization completed in {} ms, memory used: {} bytes", 
             initTime.count(), memoryUsed);
    
    // 验证启动性能要求
    // 缓存初始化应该在5秒内完成
    EXPECT_LT(initTime.count(), 5000) << "Cache initialization took too long: " << initTime.count() << " ms";
    
    // 内存使用应该合理（不超过512MB）
    EXPECT_LT(memoryUsed, 512 * 1024 * 1024) << "Cache uses too much memory: " << memoryUsed << " bytes";
    
    // 验证缓存状态
    if (cache_->IsLoaded())
    {
        EXPECT_GT(cache_->GetCacheEntryCount(), 0);
        LOG_INFO("test", "Cache loaded with {} entries", cache_->GetCacheEntryCount());
    }
}

/**
 * 属性 27.2: 管理器启动性能
 * 验证PVP装备管理器能够快速启动
 */
TEST_F(SystemStartupPerformanceTest, ManagerInitializationPerformance)
{
    LOG_INFO("test", "Testing manager initialization performance");
    
    // 先初始化缓存
    cache_ = BotPvpGearCache::GetInstance();
    cache_->LoadFromDatabase();
    
    // 测量管理器初始化时间
    auto initTime = MeasureExecutionTime([this]() {
        manager_ = PvpGearManager::GetInstance();
        ASSERT_NE(manager_, nullptr);
        
        bool initResult = manager_->Initialize();
        EXPECT_TRUE(initResult);
    });
    
    LOG_INFO("test", "Manager initialization completed in {} ms", initTime.count());
    
    // 管理器初始化应该在1秒内完成
    EXPECT_LT(initTime.count(), 1000) << "Manager initialization took too long: " << initTime.count() << " ms";
}

/**
 * 属性 27.3: 错误恢复管理器启动性能
 * 验证错误恢复系统能够快速启动
 */
TEST_F(SystemStartupPerformanceTest, ErrorManagerInitializationPerformance)
{
    LOG_INFO("test", "Testing error manager initialization performance");
    
    // 测量错误恢复管理器初始化时间
    auto initTime = MeasureExecutionTime([this]() {
        errorManager_ = ErrorRecoveryManager::GetInstance();
        ASSERT_NE(errorManager_, nullptr);
        
        bool initResult = errorManager_->Initialize();
        EXPECT_TRUE(initResult);
    });
    
    LOG_INFO("test", "Error manager initialization completed in {} ms", initTime.count());
    
    // 错误恢复管理器初始化应该在500毫秒内完成
    EXPECT_LT(initTime.count(), 500) << "Error manager initialization took too long: " << initTime.count() << " ms";
}

/**
 * 属性 27.4: 完整系统启动性能
 * 验证整个系统能够在规定时间内完成启动
 */
TEST_F(SystemStartupPerformanceTest, CompleteSystemStartupPerformance)
{
    LOG_INFO("test", "Testing complete system startup performance");
    
    uint64_t memoryBefore = GetCurrentMemoryUsage();
    
    // 测量完整系统启动时间
    auto startupTime = MeasureExecutionTime([this]() {
        // 1. 初始化错误恢复管理器
        errorManager_ = ErrorRecoveryManager::GetInstance();
        ASSERT_NE(errorManager_, nullptr);
        bool errorMgrInit = errorManager_->Initialize();
        EXPECT_TRUE(errorMgrInit);
        
        // 2. 初始化缓存系统
        cache_ = BotPvpGearCache::GetInstance();
        ASSERT_NE(cache_, nullptr);
        bool cacheInit = cache_->LoadFromDatabase();
        if (!cacheInit)
        {
            cache_->LoadCacheFromLocalFile();
        }
        
        // 3. 初始化PVP装备管理器
        manager_ = PvpGearManager::GetInstance();
        ASSERT_NE(manager_, nullptr);
        bool managerInit = manager_->Initialize();
        EXPECT_TRUE(managerInit);
    });
    
    uint64_t memoryAfter = GetCurrentMemoryUsage();
    uint64_t totalMemoryUsed = memoryAfter - memoryBefore;
    
    LOG_INFO("test", "Complete system startup completed in {} ms, total memory used: {} bytes", 
             startupTime.count(), totalMemoryUsed);
    
    // 完整系统启动应该在10秒内完成
    EXPECT_LT(startupTime.count(), 10000) << "System startup took too long: " << startupTime.count() << " ms";
    
    // 总内存使用应该合理（不超过1GB）
    EXPECT_LT(totalMemoryUsed, 1024 * 1024 * 1024) << "System uses too much memory: " << totalMemoryUsed << " bytes";
}

/**
 * 属性 27.5: 并发启动性能
 * 验证系统在多线程环境下的启动性能
 */
TEST_F(SystemStartupPerformanceTest, ConcurrentStartupPerformance)
{
    LOG_INFO("test", "Testing concurrent startup performance");
    
    const int numThreads = 4;
    std::vector<std::thread> threads;
    std::vector<std::chrono::milliseconds> threadTimes(numThreads);
    std::atomic<int> successCount{0};
    
    // 启动多个线程同时初始化系统组件
    for (int i = 0; i < numThreads; ++i)
    {
        threads.emplace_back([this, i, &threadTimes, &successCount]() {
            auto threadStartTime = std::chrono::high_resolution_clock::now();
            
            try
            {
                // 每个线程尝试获取和初始化组件
                auto cache = BotPvpGearCache::GetInstance();
                auto manager = PvpGearManager::GetInstance();
                auto errorMgr = ErrorRecoveryManager::GetInstance();
                
                if (cache && manager && errorMgr)
                {
                    successCount.fetch_add(1);
                }
                
                // 模拟一些工作负载
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            catch (const std::exception& e)
            {
                LOG_ERROR("test", "Thread {} failed: {}", i, e.what());
            }
            
            auto threadEndTime = std::chrono::high_resolution_clock::now();
            threadTimes[i] = std::chrono::duration_cast<std::chrono::milliseconds>(threadEndTime - threadStartTime);
        });
    }
    
    // 等待所有线程完成
    for (auto& thread : threads)
    {
        thread.join();
    }
    
    // 计算统计信息
    auto maxTime = *std::max_element(threadTimes.begin(), threadTimes.end());
    auto minTime = *std::min_element(threadTimes.begin(), threadTimes.end());
    auto avgTime = std::accumulate(threadTimes.begin(), threadTimes.end(), std::chrono::milliseconds{0}) / numThreads;
    
    LOG_INFO("test", "Concurrent startup - Min: {} ms, Max: {} ms, Avg: {} ms, Success: {}/{}", 
             minTime.count(), maxTime.count(), avgTime.count(), successCount.load(), numThreads);
    
    // 验证并发性能
    EXPECT_EQ(successCount.load(), numThreads) << "Not all threads succeeded";
    EXPECT_LT(maxTime.count(), 2000) << "Concurrent startup took too long: " << maxTime.count() << " ms";
}

/**
 * 属性 27.6: 启动后查询性能
 * 验证系统启动后的查询响应性能
 */
TEST_F(SystemStartupPerformanceTest, PostStartupQueryPerformance)
{
    LOG_INFO("test", "Testing post-startup query performance");
    
    // 先完成系统启动
    cache_ = BotPvpGearCache::GetInstance();
    cache_->LoadFromDatabase();
    
    manager_ = PvpGearManager::GetInstance();
    manager_->Initialize();
    
    // 等待系统稳定
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // 测试多次查询的性能
    const int numQueries = 100;
    std::vector<std::chrono::microseconds> queryTimes;
    queryTimes.reserve(numQueries);
    
    for (int i = 0; i < numQueries; ++i)
    {
        uint8_t randomClass = (rng_() % 11) + 1;  // 1-11
        uint8_t randomLevel = (rng_() % 60) + 20; // 20-80
        
        auto queryStart = std::chrono::high_resolution_clock::now();
        
        std::vector<PvpGearItem> gear = cache_->GetAvailableGear(randomClass, randomLevel, "");
        
        auto queryEnd = std::chrono::high_resolution_clock::now();
        auto queryTime = std::chrono::duration_cast<std::chrono::microseconds>(queryEnd - queryStart);
        
        queryTimes.push_back(queryTime);
    }
    
    // 计算查询性能统计
    auto maxQuery = *std::max_element(queryTimes.begin(), queryTimes.end());
    auto minQuery = *std::min_element(queryTimes.begin(), queryTimes.end());
    auto avgQuery = std::accumulate(queryTimes.begin(), queryTimes.end(), std::chrono::microseconds{0}) / numQueries;
    
    LOG_INFO("test", "Query performance - Min: {} μs, Max: {} μs, Avg: {} μs", 
             minQuery.count(), maxQuery.count(), avgQuery.count());
    
    // 验证查询性能要求
    // 平均查询时间应该在100毫秒内
    EXPECT_LT(avgQuery.count(), 100000) << "Average query time too slow: " << avgQuery.count() << " μs";
    
    // 最大查询时间应该在500毫秒内
    EXPECT_LT(maxQuery.count(), 500000) << "Maximum query time too slow: " << maxQuery.count() << " μs";
}

/**
 * 属性 27.7: 内存使用效率
 * 验证系统启动后的内存使用效率
 */
TEST_F(SystemStartupPerformanceTest, MemoryUsageEfficiency)
{
    LOG_INFO("test", "Testing memory usage efficiency");
    
    uint64_t baselineMemory = GetCurrentMemoryUsage();
    
    // 初始化系统
    cache_ = BotPvpGearCache::GetInstance();
    bool cacheLoaded = cache_->LoadFromDatabase();
    
    manager_ = PvpGearManager::GetInstance();
    manager_->Initialize();
    
    errorManager_ = ErrorRecoveryManager::GetInstance();
    errorManager_->Initialize();
    
    uint64_t afterInitMemory = GetCurrentMemoryUsage();
    uint64_t initMemoryUsage = afterInitMemory - baselineMemory;
    
    // 执行一些操作来测试内存使用
    if (cacheLoaded && cache_->GetCacheEntryCount() > 0)
    {
        for (int i = 0; i < 50; ++i)
        {
            uint8_t classId = (rng_() % 11) + 1;
            uint8_t level = (rng_() % 60) + 20;
            cache_->GetAvailableGear(classId, level, "");
        }
    }
    
    uint64_t afterOperationsMemory = GetCurrentMemoryUsage();
    uint64_t operationsMemoryIncrease = afterOperationsMemory - afterInitMemory;
    
    LOG_INFO("test", "Memory usage - Init: {} bytes, Operations increase: {} bytes", 
             initMemoryUsage, operationsMemoryIncrease);
    
    // 验证内存使用效率
    // 初始化内存使用应该合理
    EXPECT_LT(initMemoryUsage, 256 * 1024 * 1024) << "Initialization uses too much memory: " << initMemoryUsage << " bytes";
    
    // 操作过程中的内存增长应该很小
    EXPECT_LT(operationsMemoryIncrease, 10 * 1024 * 1024) << "Operations cause too much memory growth: " << operationsMemoryIncrease << " bytes";
    
    // 获取缓存统计信息
    if (cache_->IsLoaded())
    {
        CacheStats stats = cache_->GetCacheStats();
        uint32_t entryCount = stats.currentEntryCount.load();
        uint64_t cacheMemory = stats.memoryUsageBytes.load();
        
        if (entryCount > 0)
        {
            uint64_t memoryPerEntry = cacheMemory / entryCount;
            LOG_INFO("test", "Memory efficiency: {} bytes per cache entry", memoryPerEntry);
            
            // 每个缓存条目的内存使用应该合理（不超过1KB）
            EXPECT_LT(memoryPerEntry, 1024) << "Memory per cache entry too high: " << memoryPerEntry << " bytes";
        }
    }
}