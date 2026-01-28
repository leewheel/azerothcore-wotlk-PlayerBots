//By leewheel
#include "gtest/gtest.h"
#include "PvpGearManager.h"
#include "BotPvpGearCache.h"
#include "ErrorRecoveryManager.h"
#include "Log.h"
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <random>
#include <algorithm>
#include <numeric>
#include <future>

/**
 * **功能: pvp-gear-auto-equip, 属性 25: 并发性能保证**
 * 
 * 验证需求: 8.1, 8.2 - 并发性能和稳定性保证
 * 
 * 使用属性测试验证系统在并发环境下的性能保证：
 * - 响应时间一致性
 * - 吞吐量线性扩展
 * - 资源使用稳定性
 * - 错误率控制
 */
class ConcurrentPerformancePropertyTest : public ::testing::Test
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
        
        // 确保系统已初始化
        cache_->LoadFromDatabase();
        manager_->Initialize();
        errorManager_->Initialize();
        
        // 初始化随机数生成器
        rng_.seed(std::chrono::steady_clock::now().time_since_epoch().count());
    }

    void TearDown() override
    {
        // 清理测试数据
        if (cache_)
        {
            cache_->ClearCache();
        }
    }

    // 性能测量结构
    struct PerformanceMetrics
    {
        std::vector<std::chrono::microseconds> responseTimes;
        std::atomic<uint64_t> totalOperations{0};
        std::atomic<uint64_t> successfulOperations{0};
        std::atomic<uint64_t> failedOperations{0};
        std::chrono::high_resolution_clock::time_point startTime;
        std::chrono::high_resolution_clock::time_point endTime;
        
        double GetSuccessRate() const
        {
            uint64_t total = totalOperations.load();
            return total > 0 ? static_cast<double>(successfulOperations.load()) / total : 0.0;
        }
        
        double GetThroughput() const
        {
            auto duration = std::chrono::duration_cast<std::chrono::seconds>(endTime - startTime);
            return duration.count() > 0 ? static_cast<double>(totalOperations.load()) / duration.count() : 0.0;
        }
        
        std::chrono::microseconds GetAverageResponseTime() const
        {
            if (responseTimes.empty()) return std::chrono::microseconds{0};
            auto sum = std::accumulate(responseTimes.begin(), responseTimes.end(), std::chrono::microseconds{0});
            return sum / responseTimes.size();
        }
        
        std::chrono::microseconds GetMaxResponseTime() const
        {
            if (responseTimes.empty()) return std::chrono::microseconds{0};
            return *std::max_element(responseTimes.begin(), responseTimes.end());
        }
        
        std::chrono::microseconds GetMinResponseTime() const
        {
            if (responseTimes.empty()) return std::chrono::microseconds{0};
            return *std::min_element(responseTimes.begin(), responseTimes.end());
        }
    };

    // 执行并发性能测试
    PerformanceMetrics RunConcurrentPerformanceTest(int numThreads, int operationsPerThread, 
                                                   std::function<bool()> operation)
    {
        PerformanceMetrics metrics;
        std::vector<std::thread> threads;
        std::mutex responseTimesMutex;
        
        metrics.startTime = std::chrono::high_resolution_clock::now();
        
        // 启动工作线程
        for (int i = 0; i < numThreads; ++i)
        {
            threads.emplace_back([&, i]() {
                std::vector<std::chrono::microseconds> localResponseTimes;
                localResponseTimes.reserve(operationsPerThread);
                
                for (int op = 0; op < operationsPerThread; ++op)
                {
                    auto opStart = std::chrono::high_resolution_clock::now();
                    
                    bool success = operation();
                    
                    auto opEnd = std::chrono::high_resolution_clock::now();
                    auto responseTime = std::chrono::duration_cast<std::chrono::microseconds>(opEnd - opStart);
                    
                    localResponseTimes.push_back(responseTime);
                    metrics.totalOperations.fetch_add(1);
                    
                    if (success)
                    {
                        metrics.successfulOperations.fetch_add(1);
                    }
                    else
                    {
                        metrics.failedOperations.fetch_add(1);
                    }
                }
                
                // 合并响应时间数据
                {
                    std::lock_guard<std::mutex> lock(responseTimesMutex);
                    metrics.responseTimes.insert(metrics.responseTimes.end(), 
                                               localResponseTimes.begin(), localResponseTimes.end());
                }
            });
        }
        
        // 等待所有线程完成
        for (auto& thread : threads)
        {
            thread.join();
        }
        
        metrics.endTime = std::chrono::high_resolution_clock::now();
        
        return metrics;
    }

    BotPvpGearCache* cache_;
    PvpGearManager* manager_;
    ErrorRecoveryManager* errorManager_;
    std::mt19937 rng_;
};

/**
 * 属性 25.1: 响应时间一致性保证
 * 验证在并发环境下响应时间的一致性
 */
TEST_F(ConcurrentPerformancePropertyTest, ResponseTimeConsistencyProperty)
{
    LOG_INFO("test", "Testing response time consistency property");
    
    const int numIterations = 10;
    const int numThreads = 8;
    const int operationsPerThread = 50;
    
    std::vector<PerformanceMetrics> allMetrics;
    
    // 运行多次测试以验证一致性
    for (int iteration = 0; iteration < numIterations; ++iteration)
    {
        auto operation = [this]() -> bool {
            uint8_t classId = (rng_() % 11) + 1;
            uint8_t level = (rng_() % 60) + 20;
            
            std::vector<PvpGearItem> gear = cache_->GetAvailableGear(classId, level, "");
            return !gear.empty() || !cache_->IsLoaded(); // 如果缓存未加载，也认为是成功的
        };
        
        PerformanceMetrics metrics = RunConcurrentPerformanceTest(numThreads, operationsPerThread, operation);
        allMetrics.push_back(metrics);
        
        LOG_DEBUG("test", "Iteration {}: Avg response time: {} μs, Success rate: {:.2f}%", 
                 iteration, metrics.GetAverageResponseTime().count(), metrics.GetSuccessRate() * 100);
    }
    
    // 计算响应时间的统计信息
    std::vector<std::chrono::microseconds> avgResponseTimes;
    std::vector<std::chrono::microseconds> maxResponseTimes;
    
    for (const auto& metrics : allMetrics)
    {
        avgResponseTimes.push_back(metrics.GetAverageResponseTime());
        maxResponseTimes.push_back(metrics.GetMaxResponseTime());
    }
    
    // 计算响应时间的变异系数
    auto avgOfAvgs = std::accumulate(avgResponseTimes.begin(), avgResponseTimes.end(), 
                                   std::chrono::microseconds{0}) / avgResponseTimes.size();
    
    double variance = 0.0;
    for (const auto& time : avgResponseTimes)
    {
        double diff = time.count() - avgOfAvgs.count();
        variance += diff * diff;
    }
    variance /= avgResponseTimes.size();
    double stdDev = std::sqrt(variance);
    double coefficientOfVariation = avgOfAvgs.count() > 0 ? stdDev / avgOfAvgs.count() : 0.0;
    
    LOG_INFO("test", "Response time consistency - Avg: {} μs, StdDev: {:.2f}, CV: {:.2f}%", 
             avgOfAvgs.count(), stdDev, coefficientOfVariation * 100);
    
    // 属性验证：响应时间的变异系数应该小于30%
    EXPECT_LT(coefficientOfVariation, 0.3) 
        << "Response time consistency violated - CV: " << coefficientOfVariation * 100 << "%";
    
    // 属性验证：平均响应时间应该在合理范围内
    EXPECT_LT(avgOfAvgs.count(), 50000) // 50ms
        << "Average response time too high: " << avgOfAvgs.count() << " μs";
    
    // 属性验证：最大响应时间不应该过高
    auto maxOfMaxs = *std::max_element(maxResponseTimes.begin(), maxResponseTimes.end());
    EXPECT_LT(maxOfMaxs.count(), 500000) // 500ms
        << "Maximum response time too high: " << maxOfMaxs.count() << " μs";
}

/**
 * 属性 25.2: 吞吐量线性扩展保证
 * 验证系统吞吐量随线程数的扩展性
 */
TEST_F(ConcurrentPerformancePropertyTest, ThroughputScalabilityProperty)
{
    LOG_INFO("test", "Testing throughput scalability property");
    
    const int operationsPerThread = 100;
    const std::vector<int> threadCounts = {1, 2, 4, 8, 16};
    
    std::vector<double> throughputs;
    
    for (int numThreads : threadCounts)
    {
        auto operation = [this]() -> bool {
            uint8_t classId = (rng_() % 11) + 1;
            uint8_t level = (rng_() % 60) + 20;
            
            std::vector<PvpGearItem> gear = cache_->GetAvailableGear(classId, level, "");
            
            // 添加一些计算负载以模拟真实场景
            volatile int sum = 0;
            for (int i = 0; i < 100; ++i)
            {
                sum += i;
            }
            
            return !gear.empty() || !cache_->IsLoaded();
        };
        
        PerformanceMetrics metrics = RunConcurrentPerformanceTest(numThreads, operationsPerThread, operation);
        double throughput = metrics.GetThroughput();
        throughputs.push_back(throughput);
        
        LOG_INFO("test", "Threads: {}, Throughput: {:.2f} ops/sec, Success rate: {:.2f}%", 
                numThreads, throughput, metrics.GetSuccessRate() * 100);
    }
    
    // 验证吞吐量扩展性
    double baselineThroughput = throughputs[0]; // 单线程吞吐量
    
    for (size_t i = 1; i < threadCounts.size(); ++i)
    {
        double expectedMinThroughput = baselineThroughput * threadCounts[i] * 0.6; // 至少60%的线性扩展
        double actualThroughput = throughputs[i];
        
        LOG_DEBUG("test", "Threads: {}, Expected min: {:.2f}, Actual: {:.2f}, Ratio: {:.2f}", 
                 threadCounts[i], expectedMinThroughput, actualThroughput, 
                 actualThroughput / baselineThroughput);
        
        // 属性验证：吞吐量应该随线程数增加而增加（至少60%的线性扩展）
        EXPECT_GE(actualThroughput, expectedMinThroughput)
            << "Throughput scalability violated for " << threadCounts[i] << " threads";
    }
    
    // 属性验证：最高吞吐量应该达到合理水平
    double maxThroughput = *std::max_element(throughputs.begin(), throughputs.end());
    EXPECT_GT(maxThroughput, baselineThroughput * 4) 
        << "Maximum throughput too low: " << maxThroughput << " ops/sec";
}

/**
 * 属性 25.3: 资源使用稳定性保证
 * 验证并发操作不会导致资源使用异常增长
 */
TEST_F(ConcurrentPerformancePropertyTest, ResourceUsageStabilityProperty)
{
    LOG_INFO("test", "Testing resource usage stability property");
    
    // 记录初始资源使用
    uint64_t initialMemory = cache_->GetMemoryUsageBytes();
    uint32_t initialEntries = cache_->GetCacheEntryCount();
    
    const int numRounds = 5;
    const int numThreads = 12;
    const int operationsPerThread = 100;
    
    std::vector<uint64_t> memoryUsages;
    std::vector<uint32_t> entryCounts;
    
    for (int round = 0; round < numRounds; ++round)
    {
        auto operation = [this]() -> bool {
            // 执行各种操作
            uint8_t classId = (rng_() % 11) + 1;
            uint8_t level = (rng_() % 60) + 20;
            
            // 查询操作
            std::vector<PvpGearItem> gear = cache_->GetAvailableGear(classId, level, "");
            
            // 统计操作
            CacheStats stats = cache_->GetCacheStats();
            
            // 健康检查
            bool healthy = errorManager_->CheckSystemHealth();
            
            return healthy;
        };
        
        PerformanceMetrics metrics = RunConcurrentPerformanceTest(numThreads, operationsPerThread, operation);
        
        // 记录资源使用
        uint64_t currentMemory = cache_->GetMemoryUsageBytes();
        uint32_t currentEntries = cache_->GetCacheEntryCount();
        
        memoryUsages.push_back(currentMemory);
        entryCounts.push_back(currentEntries);
        
        LOG_DEBUG("test", "Round {}: Memory: {} bytes, Entries: {}, Success rate: {:.2f}%", 
                 round, currentMemory, currentEntries, metrics.GetSuccessRate() * 100);
        
        // 短暂休息以观察资源释放
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // 分析资源使用稳定性
    uint64_t maxMemory = *std::max_element(memoryUsages.begin(), memoryUsages.end());
    uint64_t minMemory = *std::min_element(memoryUsages.begin(), memoryUsages.end());
    uint32_t maxEntries = *std::max_element(entryCounts.begin(), entryCounts.end());
    uint32_t minEntries = *std::min_element(entryCounts.begin(), entryCounts.end());
    
    LOG_INFO("test", "Resource stability - Memory: {} - {} bytes, Entries: {} - {}", 
             minMemory, maxMemory, minEntries, maxEntries);
    
    // 属性验证：内存使用应该稳定（变化不超过20%）
    if (initialMemory > 0)
    {
        double memoryVariation = static_cast<double>(maxMemory - minMemory) / initialMemory;
        EXPECT_LT(memoryVariation, 0.2) 
            << "Memory usage instability detected - variation: " << memoryVariation * 100 << "%";
    }
    
    // 属性验证：缓存条目数应该稳定（只读操作不应改变条目数）
    EXPECT_EQ(maxEntries, minEntries) 
        << "Cache entry count instability detected";
    
    // 属性验证：最终内存使用不应该显著增加
    uint64_t finalMemory = memoryUsages.back();
    if (initialMemory > 0)
    {
        double memoryIncrease = static_cast<double>(finalMemory - initialMemory) / initialMemory;
        EXPECT_LT(memoryIncrease, 0.1) 
            << "Memory leak detected - increase: " << memoryIncrease * 100 << "%";
    }
}

/**
 * 属性 25.4: 错误率控制保证
 * 验证并发环境下的错误率在可接受范围内
 */
TEST_F(ConcurrentPerformancePropertyTest, ErrorRateControlProperty)
{
    LOG_INFO("test", "Testing error rate control property");
    
    const int numIterations = 8;
    const int numThreads = 10;
    const int operationsPerThread = 200;
    
    std::vector<double> errorRates;
    
    for (int iteration = 0; iteration < numIterations; ++iteration)
    {
        auto operation = [this]() -> bool {
            try
            {
                uint8_t classId = (rng_() % 11) + 1;
                uint8_t level = (rng_() % 60) + 20;
                
                // 偶尔触发一些边界条件
                if (rng_() % 100 == 0)
                {
                    classId = 0; // 无效职业ID
                }
                if (rng_() % 100 == 0)
                {
                    level = 200; // 无效等级
                }
                
                std::vector<PvpGearItem> gear = cache_->GetAvailableGear(classId, level, "");
                
                // 验证返回的数据
                for (const auto& item : gear)
                {
                    if (item.itemId == 0 || item.classId == 0)
                    {
                        return false; // 数据无效
                    }
                }
                
                return true;
            }
            catch (const std::exception& e)
            {
                LOG_DEBUG("test", "Operation failed with exception: {}", e.what());
                return false;
            }
        };
        
        PerformanceMetrics metrics = RunConcurrentPerformanceTest(numThreads, operationsPerThread, operation);
        double errorRate = 1.0 - metrics.GetSuccessRate();
        errorRates.push_back(errorRate);
        
        LOG_DEBUG("test", "Iteration {}: Error rate: {:.2f}%, Throughput: {:.2f} ops/sec", 
                 iteration, errorRate * 100, metrics.GetThroughput());
    }
    
    // 计算错误率统计
    double avgErrorRate = std::accumulate(errorRates.begin(), errorRates.end(), 0.0) / errorRates.size();
    double maxErrorRate = *std::max_element(errorRates.begin(), errorRates.end());
    
    LOG_INFO("test", "Error rate control - Average: {:.2f}%, Maximum: {:.2f}%", 
             avgErrorRate * 100, maxErrorRate * 100);
    
    // 属性验证：平均错误率应该很低
    EXPECT_LT(avgErrorRate, 0.05) // 5%
        << "Average error rate too high: " << avgErrorRate * 100 << "%";
    
    // 属性验证：最大错误率不应该过高
    EXPECT_LT(maxErrorRate, 0.1) // 10%
        << "Maximum error rate too high: " << maxErrorRate * 100 << "%";
    
    // 属性验证：错误率应该稳定（变异系数小于50%）
    double errorRateVariance = 0.0;
    for (double rate : errorRates)
    {
        double diff = rate - avgErrorRate;
        errorRateVariance += diff * diff;
    }
    errorRateVariance /= errorRates.size();
    double errorRateStdDev = std::sqrt(errorRateVariance);
    double errorRateCV = avgErrorRate > 0 ? errorRateStdDev / avgErrorRate : 0.0;
    
    EXPECT_LT(errorRateCV, 0.5) 
        << "Error rate instability detected - CV: " << errorRateCV * 100 << "%";
}

/**
 * 属性 25.5: 并发安全性保证
 * 验证并发操作不会导致数据竞争或状态不一致
 */
TEST_F(ConcurrentPerformancePropertyTest, ConcurrentSafetyProperty)
{
    LOG_INFO("test", "Testing concurrent safety property");
    
    const int numThreads = 16;
    const int operationsPerThread = 150;
    
    std::atomic<uint64_t> totalItemsRetrieved{0};
    std::atomic<uint64_t> totalQueriesExecuted{0};
    std::atomic<uint64_t> inconsistentResults{0};
    
    auto operation = [&]() -> bool {
        try
        {
            uint8_t classId = (rng_() % 11) + 1;
            uint8_t level = (rng_() % 60) + 20;
            
            // 执行相同的查询多次，结果应该一致
            std::vector<PvpGearItem> gear1 = cache_->GetAvailableGear(classId, level, "");
            std::vector<PvpGearItem> gear2 = cache_->GetAvailableGear(classId, level, "");
            
            totalQueriesExecuted.fetch_add(2);
            totalItemsRetrieved.fetch_add(gear1.size() + gear2.size());
            
            // 验证结果一致性
            if (gear1.size() != gear2.size())
            {
                inconsistentResults.fetch_add(1);
                return false;
            }
            
            // 验证数据完整性
            for (size_t i = 0; i < gear1.size(); ++i)
            {
                if (gear1[i].itemId != gear2[i].itemId || 
                    gear1[i].classId != gear2[i].classId ||
                    gear1[i].levelMin != gear2[i].levelMin ||
                    gear1[i].levelMax != gear2[i].levelMax)
                {
                    inconsistentResults.fetch_add(1);
                    return false;
                }
            }
            
            return true;
        }
        catch (const std::exception& e)
        {
            LOG_DEBUG("test", "Safety test operation failed: {}", e.what());
            return false;
        }
    };
    
    PerformanceMetrics metrics = RunConcurrentPerformanceTest(numThreads, operationsPerThread, operation);
    
    LOG_INFO("test", "Concurrent safety - Queries: {}, Items: {}, Inconsistencies: {}, Success rate: {:.2f}%", 
             totalQueriesExecuted.load(), totalItemsRetrieved.load(), 
             inconsistentResults.load(), metrics.GetSuccessRate() * 100);
    
    // 属性验证：不应该有数据不一致的情况
    EXPECT_EQ(inconsistentResults.load(), 0) 
        << "Data inconsistency detected in concurrent operations";
    
    // 属性验证：成功率应该很高
    EXPECT_GT(metrics.GetSuccessRate(), 0.95) 
        << "Success rate too low: " << metrics.GetSuccessRate() * 100 << "%";
    
    // 属性验证：系统应该仍然健康
    EXPECT_TRUE(cache_->IsLoaded());
    EXPECT_TRUE(errorManager_->CheckSystemHealth());
    
    // 属性验证：缓存统计应该合理
    CacheStats stats = cache_->GetCacheStats();
    EXPECT_GT(stats.totalRequests.load(), 0);
    EXPECT_GE(stats.cacheHits.load(), stats.cacheMisses.load()); // 命中率应该不错
}