//By leewheel
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "BotPvpGearCache.h"
#include "ErrorRecoveryManager.h"
#include "DatabaseEnv.h"
#include "ObjectMgr.h"
#include "Log.h"
#include <random>
#include <thread>
#include <chrono>
#include <vector>
#include <unordered_set>

using namespace testing;

/**
 * Property-based test for BotPvpGearCache LRU cleanup mechanism
 * **功能: pvp-gear-auto-equip, 属性 15: 内存管理策略**
 * **验证需求: 5.4, 8.5**
 */
class BotPvpGearCacheLRUPropertyTest : public Test
{
protected:
    void SetUp() override
    {
        // Initialize cache with test configuration
        cache = BotPvpGearCache::GetInstance();
        cache->ClearCache();
        
        // Set test-friendly thresholds
        cache->SetMaxCacheSize(100);  // Small cache for testing
        cache->SetMemoryThreshold(1024 * 1024);  // 1MB threshold
        cache->SetCleanupRatio(0.3f);  // Remove 30% during cleanup
        
        // Reset statistics
        cache->ResetCacheStats();
        
        // Initialize random number generator
        rng.seed(std::chrono::steady_clock::now().time_since_epoch().count());
    }
    
    void TearDown() override
    {
        if (cache)
        {
            cache->ClearCache();
        }
    }
    
    // Generate random PVP gear item for testing
    PvpGearItem GenerateRandomGearItem(uint32 itemId = 0)
    {
        if (itemId == 0)
        {
            itemId = itemIdDistribution(rng);
        }
        
        PvpGearItem item;
        item.itemId = itemId;
        item.itemName = "TestItem_" + std::to_string(itemId);
        item.itemCount = 1;
        item.classId = classDistribution(rng);
        item.talent = talents[talentDistribution(rng)];
        item.levelMin = levelMinDistribution(rng);
        item.levelMax = item.levelMin + levelRangeDistribution(rng);
        item.lastAccessTime = 0;
        item.accessCount = 0;
        
        return item;
    }
    
    // Populate cache with test data
    void PopulateCacheWithTestData(uint32 itemCount)
    {
        // Use friend class access to populate cache for testing
        std::lock_guard<std::mutex> lock(cache->cacheMutex_);
        
        for (uint32 i = 0; i < itemCount; ++i)
        {
            PvpGearItem item = GenerateRandomGearItem(baseItemId + i);
            
            // Add to main cache
            cache->gearCache_[item.itemId] = item;
            
            // Add to class cache
            uint32 key = item.classId * 1000;
            if (!item.talent.empty())
            {
                key += std::hash<std::string>{}(item.talent) % 999;
            }
            
            cache->classGearCache_[key].push_back(item.itemId);
            
            // Add to LRU tracking
            cache->AddToLRUList(item.itemId, cache->EstimateItemMemorySize(item));
        }
        
        // Update statistics
        cache->stats_.currentEntryCount = itemCount;
        cache->stats_.memoryUsageBytes = itemCount * 200; // Estimate
        
        // Mark as loaded
        cache->isLoaded_ = true;
    }
    
    // Simulate cache access patterns
    void SimulateRandomAccess(uint32 accessCount)
    {
        for (uint32 i = 0; i < accessCount; ++i)
        {
            uint32 itemId = baseItemId + (accessDistribution(rng) % cache->GetCacheEntryCount());
            
            // Randomly choose access method
            int accessType = accessTypeDistribution(rng);
            switch (accessType)
            {
                case 0:
                    cache->GetGearDetails(itemId);
                    break;
                case 1:
                    cache->IsResilienceGear(itemId);
                    break;
                case 2:
                    {
                        uint8 playerClass = classDistribution(rng);
                        uint8 level = levelDistribution(rng);
                        cache->GetAvailableGear(playerClass, level);
                    }
                    break;
            }
            
            // Small delay to simulate realistic access patterns
            std::this_thread::sleep_for(std::chrono::microseconds(10));
        }
    }

protected:
    BotPvpGearCache* cache;
    std::mt19937 rng;
    
    // Test data generation parameters
    static constexpr uint32 baseItemId = 10000;
    std::uniform_int_distribution<uint32> itemIdDistribution{baseItemId, baseItemId + 10000};
    std::uniform_int_distribution<uint8> classDistribution{1, 11};  // WoW classes 1-11
    std::uniform_int_distribution<uint8> levelMinDistribution{1, 70};
    std::uniform_int_distribution<uint8> levelRangeDistribution{1, 10};
    std::uniform_int_distribution<uint8> levelDistribution{1, 80};
    std::uniform_int_distribution<size_t> talentDistribution{0, 2};
    std::uniform_int_distribution<uint32> accessDistribution{0, UINT32_MAX};
    std::uniform_int_distribution<int> accessTypeDistribution{0, 2};
    
    std::vector<std::string> talents = {"", "Holy", "Protection", "Retribution"};
};

/**
 * Property: LRU cleanup should maintain cache size within limits
 * **验证需求: 5.4 (缓存性能优化)**
 */
TEST_F(BotPvpGearCacheLRUPropertyTest, LRUCleanupMaintainsCacheSizeWithinLimits)
{
    // Property: For any cache size limit and cleanup operation,
    // the cache size after cleanup should not exceed the limit
    
    for (int iteration = 0; iteration < 50; ++iteration)
    {
        // Generate random test parameters
        uint32 maxCacheSize = std::uniform_int_distribution<uint32>{50, 200}(rng);
        uint32 initialItems = std::uniform_int_distribution<uint32>{maxCacheSize + 10, maxCacheSize + 50}(rng);
        float cleanupRatio = std::uniform_real_distribution<float>{0.1f, 0.5f}(rng);
        
        // Configure cache
        cache->ClearCache();
        cache->SetMaxCacheSize(maxCacheSize);
        cache->SetCleanupRatio(cleanupRatio);
        
        // Populate cache beyond limit
        PopulateCacheWithTestData(initialItems);
        
        ASSERT_GT(cache->GetCacheEntryCount(), maxCacheSize) 
            << "Initial cache size should exceed limit for iteration " << iteration;
        
        // Trigger cleanup
        bool cleanupResult = cache->CleanupLRUCache();
        
        // Verify cleanup was successful
        EXPECT_TRUE(cleanupResult) 
            << "Cleanup should succeed for iteration " << iteration;
        
        // Verify cache size is within reasonable bounds after cleanup
        uint32 finalSize = cache->GetCacheEntryCount();
        uint32 expectedMaxAfterCleanup = static_cast<uint32>(initialItems * (1.0f - cleanupRatio)) + 10; // Allow some tolerance
        
        EXPECT_LE(finalSize, expectedMaxAfterCleanup)
            << "Cache size after cleanup should be reduced. Iteration: " << iteration
            << ", Initial: " << initialItems << ", Final: " << finalSize 
            << ", Cleanup ratio: " << cleanupRatio;
        
        // Verify cache is still functional
        EXPECT_GT(finalSize, 0) << "Cache should not be empty after cleanup";
        
        // Verify statistics were updated
        CacheStats stats = cache->GetCacheStats();
        EXPECT_GT(stats.evictions.load(), 0) 
            << "Eviction count should be updated for iteration " << iteration;
    }
}

/**
 * Property: Memory usage should decrease after cleanup when threshold is exceeded
 * **验证需求: 8.5 (内存管理策略)**
 */
TEST_F(BotPvpGearCacheLRUPropertyTest, MemoryUsageDecreasesAfterCleanup)
{
    // Property: For any memory threshold and cache state exceeding that threshold,
    // cleanup should reduce memory usage
    
    for (int iteration = 0; iteration < 30; ++iteration)
    {
        // Generate random test parameters
        uint64 memoryThreshold = std::uniform_int_distribution<uint64>{512 * 1024, 2 * 1024 * 1024}(rng); // 512KB - 2MB
        uint32 itemCount = std::uniform_int_distribution<uint32>{200, 500}(rng);
        
        // Configure cache
        cache->ClearCache();
        cache->SetMemoryThreshold(memoryThreshold);
        cache->SetMaxCacheSize(itemCount + 100); // Ensure size limit doesn't interfere
        
        // Populate cache
        PopulateCacheWithTestData(itemCount);
        
        // Force memory usage calculation
        const_cast<BotPvpGearCache*>(cache)->UpdateMemoryUsage();
        
        uint64 initialMemory = cache->GetMemoryUsageBytes();
        
        // Only test if we actually exceed the threshold
        if (initialMemory <= memoryThreshold)
        {
            continue; // Skip this iteration
        }
        
        // Trigger memory optimization
        bool optimizeResult = cache->OptimizeMemoryUsage();
        
        // Verify optimization was attempted
        EXPECT_TRUE(optimizeResult) 
            << "Memory optimization should succeed for iteration " << iteration;
        
        // Verify memory usage decreased
        uint64 finalMemory = cache->GetMemoryUsageBytes();
        EXPECT_LT(finalMemory, initialMemory)
            << "Memory usage should decrease after optimization. Iteration: " << iteration
            << ", Initial: " << initialMemory << " bytes, Final: " << finalMemory << " bytes"
            << ", Threshold: " << memoryThreshold << " bytes";
        
        // Verify cache is still functional
        EXPECT_GT(cache->GetCacheEntryCount(), 0) 
            << "Cache should not be empty after memory optimization";
    }
}

/**
 * Property: LRU eviction should remove least recently used items
 * **验证需求: 5.4 (缓存性能优化)**
 */
TEST_F(BotPvpGearCacheLRUPropertyTest, LRUEvictionRemovesLeastRecentlyUsedItems)
{
    // Property: For any cache with access patterns, LRU cleanup should preferentially
    // remove items that haven't been accessed recently
    
    for (int iteration = 0; iteration < 20; ++iteration)
    {
        // Setup cache with known items
        cache->ClearCache();
        cache->SetMaxCacheSize(50);
        cache->SetCleanupRatio(0.4f); // Remove 40%
        
        uint32 itemCount = 60; // Exceed cache limit
        PopulateCacheWithTestData(itemCount);
        
        // Create access pattern - access first half of items multiple times
        uint32 frequentlyAccessedCount = itemCount / 2;
        for (uint32 accessRound = 0; accessRound < 5; ++accessRound)
        {
            for (uint32 i = 0; i < frequentlyAccessedCount; ++i)
            {
                uint32 itemId = baseItemId + i;
                cache->GetGearDetails(itemId); // This updates LRU tracking
            }
        }
        
        // Record which items were frequently accessed
        std::unordered_set<uint32> frequentlyAccessed;
        for (uint32 i = 0; i < frequentlyAccessedCount; ++i)
        {
            frequentlyAccessed.insert(baseItemId + i);
        }
        
        // Trigger cleanup
        uint32 initialCount = cache->GetCacheEntryCount();
        bool cleanupResult = cache->CleanupLRUCache();
        uint32 finalCount = cache->GetCacheEntryCount();
        
        EXPECT_TRUE(cleanupResult) << "Cleanup should succeed for iteration " << iteration;
        EXPECT_LT(finalCount, initialCount) << "Cache size should decrease after cleanup";
        
        // Verify that frequently accessed items are more likely to remain
        uint32 frequentlyAccessedRemaining = 0;
        uint32 totalRemaining = 0;
        
        for (uint32 i = 0; i < itemCount; ++i)
        {
            uint32 itemId = baseItemId + i;
            if (cache->GetGearDetails(itemId) != nullptr)
            {
                totalRemaining++;
                if (frequentlyAccessed.count(itemId) > 0)
                {
                    frequentlyAccessedRemaining++;
                }
            }
        }
        
        // The retention rate for frequently accessed items should be higher
        if (totalRemaining > 0 && frequentlyAccessedCount > 0)
        {
            float frequentRetentionRate = static_cast<float>(frequentlyAccessedRemaining) / frequentlyAccessedCount;
            float overallRetentionRate = static_cast<float>(totalRemaining) / itemCount;
            
            EXPECT_GE(frequentRetentionRate, overallRetentionRate)
                << "Frequently accessed items should have higher retention rate. Iteration: " << iteration
                << ", Frequent retention: " << frequentRetentionRate 
                << ", Overall retention: " << overallRetentionRate;
        }
    }
}

/**
 * Property: Cache performance statistics should be accurately maintained
 * **验证需求: 5.4 (缓存性能优化)**
 */
TEST_F(BotPvpGearCacheLRUPropertyTest, CacheStatisticsAreAccuratelyMaintained)
{
    // Property: For any sequence of cache operations, statistics should accurately
    // reflect the actual operations performed
    
    for (int iteration = 0; iteration < 25; ++iteration)
    {
        // Setup
        cache->ClearCache();
        cache->ResetCacheStats();
        
        uint32 itemCount = std::uniform_int_distribution<uint32>{50, 150}(rng);
        PopulateCacheWithTestData(itemCount);
        
        CacheStats initialStats = cache->GetCacheStats();
        
        // Perform random access operations
        uint32 accessCount = std::uniform_int_distribution<uint32>{20, 100}(rng);
        uint32 expectedHits = 0;
        uint32 expectedMisses = 0;
        
        for (uint32 i = 0; i < accessCount; ++i)
        {
            uint32 itemId;
            bool shouldHit = std::uniform_real_distribution<float>{0.0f, 1.0f}(rng) < 0.7f; // 70% hit rate
            
            if (shouldHit)
            {
                // Access existing item
                itemId = baseItemId + (accessDistribution(rng) % itemCount);
                expectedHits++;
            }
            else
            {
                // Access non-existing item
                itemId = baseItemId + itemCount + (accessDistribution(rng) % 1000);
                expectedMisses++;
            }
            
            cache->GetGearDetails(itemId);
        }
        
        CacheStats finalStats = cache->GetCacheStats();
        
        // Verify statistics accuracy
        uint64 actualTotalRequests = finalStats.totalRequests.load() - initialStats.totalRequests.load();
        uint64 actualHits = finalStats.cacheHits.load() - initialStats.cacheHits.load();
        uint64 actualMisses = finalStats.cacheMisses.load() - initialStats.cacheMisses.load();
        
        EXPECT_EQ(actualTotalRequests, accessCount)
            << "Total requests should match access count. Iteration: " << iteration;
        
        EXPECT_EQ(actualTotalRequests, actualHits + actualMisses)
            << "Total requests should equal hits + misses. Iteration: " << iteration;
        
        // Allow some tolerance for timing-related variations
        EXPECT_NEAR(actualHits, expectedHits, 5)
            << "Hit count should be approximately correct. Iteration: " << iteration
            << ", Expected: " << expectedHits << ", Actual: " << actualHits;
        
        EXPECT_NEAR(actualMisses, expectedMisses, 5)
            << "Miss count should be approximately correct. Iteration: " << iteration
            << ", Expected: " << expectedMisses << ", Actual: " << actualMisses;
        
        // Verify hit ratio calculation
        if (actualTotalRequests > 0)
        {
            double expectedHitRatio = static_cast<double>(actualHits) / actualTotalRequests;
            double actualHitRatio = finalStats.GetHitRatio();
            
            EXPECT_NEAR(actualHitRatio, expectedHitRatio, 0.01)
                << "Hit ratio calculation should be accurate. Iteration: " << iteration;
        }
    }
}

/**
 * Property: Concurrent access should not corrupt cache state
 * **验证需求: 8.5 (内存管理策略)**
 */
TEST_F(BotPvpGearCacheLRUPropertyTest, ConcurrentAccessDoesNotCorruptCacheState)
{
    // Property: For any concurrent access pattern, cache should maintain consistency
    // and not crash or corrupt data
    
    for (int iteration = 0; iteration < 10; ++iteration)
    {
        // Setup
        cache->ClearCache();
        cache->SetMaxCacheSize(200);
        
        uint32 itemCount = 100;
        PopulateCacheWithTestData(itemCount);
        
        uint32 initialCount = cache->GetCacheEntryCount();
        CacheStats initialStats = cache->GetCacheStats();
        
        // Launch multiple threads performing different operations
        std::vector<std::thread> threads;
        std::atomic<bool> stopFlag{false};
        std::atomic<uint32> operationCount{0};
        
        // Thread 1: Random access
        threads.emplace_back([&]() {
            std::mt19937 localRng(rng());
            std::uniform_int_distribution<uint32> itemDist(baseItemId, baseItemId + itemCount - 1);
            
            while (!stopFlag.load())
            {
                uint32 itemId = itemDist(localRng);
                cache->GetGearDetails(itemId);
                operationCount.fetch_add(1);
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        });
        
        // Thread 2: Cleanup operations
        threads.emplace_back([&]() {
            while (!stopFlag.load())
            {
                if (cache->ShouldTriggerCleanup())
                {
                    cache->CleanupLRUCache(10); // Small cleanup
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        });
        
        // Thread 3: Statistics queries
        threads.emplace_back([&]() {
            while (!stopFlag.load())
            {
                cache->GetCacheStats();
                cache->GetMemoryUsageBytes();
                cache->GetCacheEntryCount();
                std::this_thread::sleep_for(std::chrono::milliseconds(25));
            }
        });
        
        // Run for a short time
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        stopFlag.store(true);
        
        // Wait for all threads to complete
        for (auto& thread : threads)
        {
            thread.join();
        }
        
        // Verify cache is still in a valid state
        uint32 finalCount = cache->GetCacheEntryCount();
        CacheStats finalStats = cache->GetCacheStats();
        
        EXPECT_LE(finalCount, initialCount) 
            << "Cache size should not increase during concurrent operations. Iteration: " << iteration;
        
        EXPECT_GE(finalStats.totalRequests.load(), initialStats.totalRequests.load())
            << "Request count should not decrease. Iteration: " << iteration;
        
        EXPECT_GT(operationCount.load(), 0)
            << "Operations should have been performed. Iteration: " << iteration;
        
        // Verify cache is still functional
        uint32 testItemId = baseItemId;
        PvpGearItem* item = cache->GetGearDetails(testItemId);
        // Item might have been evicted, but call should not crash
        
        // Verify no memory corruption by checking cache status
        std::string statusReport = cache->GetCacheStatusReport();
        EXPECT_FALSE(statusReport.empty())
            << "Status report should be available after concurrent operations. Iteration: " << iteration;
    }
}

/**
 * Property 13: Query performance requirements - all queries should complete within 100ms
 * **功能: pvp-gear-auto-equip, 属性 13: 查询性能要求**
 * **验证需求: 5.2**
 */
TEST_F(BotPvpGearCacheLRUPropertyTest, QueryPerformanceWithin100Milliseconds)
{
    // Property: For any cache query operation, response time should be within 100ms
    
    for (int iteration = 0; iteration < 100; ++iteration)
    {
        // Setup cache with varying data sizes
        cache->ClearCache();
        uint32 itemCount = std::uniform_int_distribution<uint32>{100, 1000}(rng);
        PopulateCacheWithTestData(itemCount);
        
        // Test different query types with performance measurement
        std::vector<std::pair<std::string, std::function<void()>>> queryOperations = {
            {"GetGearDetails", [&]() {
                uint32 itemId = baseItemId + (accessDistribution(rng) % itemCount);
                cache->GetGearDetails(itemId);
            }},
            {"IsResilienceGear", [&]() {
                uint32 itemId = baseItemId + (accessDistribution(rng) % itemCount);
                cache->IsResilienceGear(itemId);
            }},
            {"GetAvailableGear", [&]() {
                uint8 playerClass = classDistribution(rng);
                uint8 level = levelDistribution(rng);
                cache->GetAvailableGear(playerClass, level);
            }},
            {"GetCacheStats", [&]() {
                cache->GetCacheStats();
            }},
            {"GetMemoryUsageBytes", [&]() {
                cache->GetMemoryUsageBytes();
            }}
        };
        
        for (const auto& operation : queryOperations)
        {
            // Measure query performance
            auto startTime = std::chrono::high_resolution_clock::now();
            
            operation.second(); // Execute the query
            
            auto endTime = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
            
            EXPECT_LE(duration.count(), 100)
                << "Query operation '" << operation.first << "' took " << duration.count() 
                << "ms, exceeding 100ms limit. Iteration: " << iteration
                << ", Cache size: " << itemCount << " items";
        }
        
        // Test batch query performance
        auto batchStartTime = std::chrono::high_resolution_clock::now();
        
        // Perform 10 random queries in sequence
        for (int batchQuery = 0; batchQuery < 10; ++batchQuery)
        {
            uint32 itemId = baseItemId + (accessDistribution(rng) % itemCount);
            cache->GetGearDetails(itemId);
        }
        
        auto batchEndTime = std::chrono::high_resolution_clock::now();
        auto batchDuration = std::chrono::duration_cast<std::chrono::milliseconds>(batchEndTime - batchStartTime);
        
        EXPECT_LE(batchDuration.count(), 100)
            << "Batch of 10 queries took " << batchDuration.count() 
            << "ms, exceeding 100ms limit. Iteration: " << iteration
            << ", Cache size: " << itemCount << " items";
    }
}

/**
 * Property 15: Memory management strategy - LRU cleanup and garbage collection effectiveness
 * **功能: pvp-gear-auto-equip, 属性 15: 内存管理策略**
 * **验证需求: 5.4, 8.5**
 */
TEST_F(BotPvpGearCacheLRUPropertyTest, MemoryManagementStrategyEffectiveness)
{
    // Property: For any memory pressure situation, the memory management strategy
    // should effectively reduce memory usage while maintaining cache functionality
    
    for (int iteration = 0; iteration < 50; ++iteration)
    {
        // Setup cache with random configuration
        cache->ClearCache();
        
        uint64 memoryThreshold = std::uniform_int_distribution<uint64>{256 * 1024, 1024 * 1024}(rng); // 256KB - 1MB
        uint32 maxCacheSize = std::uniform_int_distribution<uint32>{200, 800}(rng);
        float cleanupRatio = std::uniform_real_distribution<float>{0.2f, 0.6f}(rng);
        
        cache->SetMemoryThreshold(memoryThreshold);
        cache->SetMaxCacheSize(maxCacheSize);
        cache->SetCleanupRatio(cleanupRatio);
        
        // Populate cache to exceed memory threshold
        uint32 itemCount = maxCacheSize + std::uniform_int_distribution<uint32>{50, 200}(rng);
        PopulateCacheWithTestData(itemCount);
        
        // Force memory usage calculation
        const_cast<BotPvpGearCache*>(cache)->UpdateMemoryUsage();
        
        uint64 initialMemory = cache->GetMemoryUsageBytes();
        uint32 initialEntryCount = cache->GetCacheEntryCount();
        CacheStats initialStats = cache->GetCacheStats();
        
        // Simulate memory pressure by accessing items to create realistic usage patterns
        SimulateRandomAccess(std::uniform_int_distribution<uint32>{50, 200}(rng));
        
        // Test memory management effectiveness
        bool memoryOptimized = false;
        bool lruCleanupPerformed = false;
        
        // Check if memory threshold is exceeded and trigger appropriate cleanup
        if (initialMemory > memoryThreshold)
        {
            memoryOptimized = cache->OptimizeMemoryUsage();
            EXPECT_TRUE(memoryOptimized) 
                << "Memory optimization should succeed when threshold exceeded. Iteration: " << iteration;
        }
        
        // Check if cache size exceeds limit and trigger LRU cleanup
        if (initialEntryCount > maxCacheSize)
        {
            lruCleanupPerformed = cache->CleanupLRUCache();
            EXPECT_TRUE(lruCleanupPerformed)
                << "LRU cleanup should succeed when cache size exceeded. Iteration: " << iteration;
        }
        
        // Verify memory management effectiveness
        uint64 finalMemory = cache->GetMemoryUsageBytes();
        uint32 finalEntryCount = cache->GetCacheEntryCount();
        CacheStats finalStats = cache->GetCacheStats();
        
        if (memoryOptimized || lruCleanupPerformed)
        {
            // Memory usage should be reduced
            EXPECT_LT(finalMemory, initialMemory)
                << "Memory usage should decrease after management operations. Iteration: " << iteration
                << ", Initial: " << initialMemory << " bytes, Final: " << finalMemory << " bytes";
            
            // Entry count should be reduced
            EXPECT_LT(finalEntryCount, initialEntryCount)
                << "Cache entry count should decrease after cleanup. Iteration: " << iteration
                << ", Initial: " << initialEntryCount << ", Final: " << finalEntryCount;
            
            // Eviction statistics should be updated
            EXPECT_GT(finalStats.evictions.load(), initialStats.evictions.load())
                << "Eviction count should increase after cleanup. Iteration: " << iteration;
        }
        
        // Verify cache remains functional after memory management
        EXPECT_GT(finalEntryCount, 0) 
            << "Cache should not be empty after memory management. Iteration: " << iteration;
        
        // Test cache functionality after cleanup
        uint32 testItemId = baseItemId + (accessDistribution(rng) % std::min(itemCount, finalEntryCount));
        auto testStartTime = std::chrono::high_resolution_clock::now();
        PvpGearItem* testItem = cache->GetGearDetails(testItemId);
        auto testEndTime = std::chrono::high_resolution_clock::now();
        auto testDuration = std::chrono::duration_cast<std::chrono::milliseconds>(testEndTime - testStartTime);
        
        // Cache should still respond quickly after cleanup
        EXPECT_LE(testDuration.count(), 100)
            << "Cache should maintain performance after memory management. Iteration: " << iteration;
        
        // Memory usage should stay within reasonable bounds
        if (finalMemory > memoryThreshold * 1.2) // Allow 20% tolerance
        {
            EXPECT_LE(finalMemory, memoryThreshold * 1.2)
                << "Memory usage should be controlled after management. Iteration: " << iteration
                << ", Final memory: " << finalMemory << ", Threshold: " << memoryThreshold;
        }
    }
}

/**
 * Property 26: Scalability guarantee - cache performance should not degrade significantly with data growth
 * **功能: pvp-gear-auto-equip, 属性 26: 可扩展性保证**
 * **验证需求: 8.3**
 */
TEST_F(BotPvpGearCacheLRUPropertyTest, ScalabilityPerformanceGuarantee)
{
    // Property: For any increase in cache data size, query performance degradation
    // should be minimal and within acceptable bounds
    
    std::vector<uint32> dataSizes = {100, 500, 1000, 2000, 5000};
    std::vector<double> averageQueryTimes;
    
    for (int iteration = 0; iteration < 20; ++iteration)
    {
        averageQueryTimes.clear();
        
        for (uint32 dataSize : dataSizes)
        {
            // Setup cache with specific data size
            cache->ClearCache();
            cache->SetMaxCacheSize(dataSize + 1000); // Ensure size doesn't limit us
            cache->SetMemoryThreshold(10 * 1024 * 1024); // 10MB - high threshold
            
            PopulateCacheWithTestData(dataSize);
            
            // Warm up cache with some accesses
            for (uint32 warmup = 0; warmup < std::min(dataSize / 10, 50u); ++warmup)
            {
                uint32 itemId = baseItemId + (accessDistribution(rng) % dataSize);
                cache->GetGearDetails(itemId);
            }
            
            // Measure query performance for this data size
            const uint32 queryCount = 100;
            auto totalStartTime = std::chrono::high_resolution_clock::now();
            
            for (uint32 query = 0; query < queryCount; ++query)
            {
                // Mix different query types
                switch (query % 4)
                {
                    case 0:
                        {
                            uint32 itemId = baseItemId + (accessDistribution(rng) % dataSize);
                            cache->GetGearDetails(itemId);
                        }
                        break;
                    case 1:
                        {
                            uint32 itemId = baseItemId + (accessDistribution(rng) % dataSize);
                            cache->IsResilienceGear(itemId);
                        }
                        break;
                    case 2:
                        {
                            uint8 playerClass = classDistribution(rng);
                            uint8 level = levelDistribution(rng);
                            cache->GetAvailableGear(playerClass, level);
                        }
                        break;
                    case 3:
                        cache->GetCacheStats();
                        break;
                }
            }
            
            auto totalEndTime = std::chrono::high_resolution_clock::now();
            auto totalDuration = std::chrono::duration_cast<std::chrono::microseconds>(totalEndTime - totalStartTime);
            
            double averageQueryTime = static_cast<double>(totalDuration.count()) / queryCount / 1000.0; // Convert to milliseconds
            averageQueryTimes.push_back(averageQueryTime);
            
            // Each individual query should still be fast
            EXPECT_LE(averageQueryTime, 50.0) // 50ms average should be reasonable
                << "Average query time too high for data size " << dataSize 
                << ": " << averageQueryTime << "ms. Iteration: " << iteration;
        }
        
        // Verify scalability - performance degradation should be reasonable
        if (averageQueryTimes.size() >= 2)
        {
            double baselineTime = averageQueryTimes[0]; // Smallest data size performance
            
            for (size_t i = 1; i < averageQueryTimes.size(); ++i)
            {
                double currentTime = averageQueryTimes[i];
                double performanceDegradation = (currentTime - baselineTime) / baselineTime;
                
                // Performance degradation should be reasonable (less than 300% increase)
                EXPECT_LE(performanceDegradation, 3.0)
                    << "Performance degradation too high. Iteration: " << iteration
                    << ", Data size: " << dataSizes[i] << ", Baseline: " << baselineTime << "ms"
                    << ", Current: " << currentTime << "ms, Degradation: " << (performanceDegradation * 100) << "%";
                
                // Absolute performance should still be reasonable
                EXPECT_LE(currentTime, 100.0)
                    << "Absolute query time too high for data size " << dataSizes[i]
                    << ": " << currentTime << "ms. Iteration: " << iteration;
            }
        }
        
        // Test concurrent scalability
        if (iteration % 5 == 0) // Every 5th iteration, test concurrency
        {
            uint32 largeDataSize = dataSizes.back();
            cache->ClearCache();
            cache->SetMaxCacheSize(largeDataSize + 1000);
            PopulateCacheWithTestData(largeDataSize);
            
            // Launch multiple threads performing queries
            std::vector<std::thread> threads;
            std::atomic<bool> stopFlag{false};
            std::atomic<uint32> totalQueries{0};
            std::atomic<uint64> totalQueryTime{0}; // in microseconds
            
            const uint32 threadCount = 4;
            for (uint32 t = 0; t < threadCount; ++t)
            {
                threads.emplace_back([&]() {
                    std::mt19937 localRng(rng() + t);
                    std::uniform_int_distribution<uint32> itemDist(baseItemId, baseItemId + largeDataSize - 1);
                    
                    while (!stopFlag.load())
                    {
                        auto queryStart = std::chrono::high_resolution_clock::now();
                        
                        uint32 itemId = itemDist(localRng);
                        cache->GetGearDetails(itemId);
                        
                        auto queryEnd = std::chrono::high_resolution_clock::now();
                        auto queryDuration = std::chrono::duration_cast<std::chrono::microseconds>(queryEnd - queryStart);
                        
                        totalQueries.fetch_add(1);
                        totalQueryTime.fetch_add(queryDuration.count());
                        
                        std::this_thread::sleep_for(std::chrono::microseconds(500));
                    }
                });
            }
            
            // Run concurrent test for short duration
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            stopFlag.store(true);
            
            for (auto& thread : threads)
            {
                thread.join();
            }
            
            // Verify concurrent performance
            if (totalQueries.load() > 0)
            {
                double avgConcurrentQueryTime = static_cast<double>(totalQueryTime.load()) / totalQueries.load() / 1000.0; // Convert to ms
                
                EXPECT_LE(avgConcurrentQueryTime, 100.0)
                    << "Concurrent query performance too slow: " << avgConcurrentQueryTime 
                    << "ms average. Iteration: " << iteration << ", Queries: " << totalQueries.load();
                
                EXPECT_GT(totalQueries.load(), threadCount * 5) // Should have performed reasonable number of queries
                    << "Too few concurrent queries performed. Iteration: " << iteration;
            }
        }
    }
}

//End leewheel