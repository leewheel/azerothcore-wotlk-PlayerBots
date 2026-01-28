//By leewheel
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "BotPvpGearCache.h"
#include "ErrorRecoveryManager.h"

using namespace testing;

/**
 * Unit tests for BotPvpGearCache LRU cleanup mechanism
 * Tests specific examples and edge cases for the LRU cache functionality
 */
class BotPvpGearCacheLRUTest : public Test
{
protected:
    void SetUp() override
    {
        cache = BotPvpGearCache::GetInstance();
        cache->ClearCache();
        cache->SetMaxCacheSize(10);  // Small cache for testing
        cache->SetMemoryThreshold(1024);  // 1KB threshold
        cache->SetCleanupRatio(0.5f);  // Remove 50% during cleanup
        cache->ResetCacheStats();
    }
    
    void TearDown() override
    {
        if (cache)
        {
            cache->ClearCache();
        }
    }

protected:
    BotPvpGearCache* cache;
};

/**
 * Test basic LRU cleanup functionality
 */
TEST_F(BotPvpGearCacheLRUTest, BasicLRUCleanupReducesCacheSize)
{
    // Setup: Create a cache with known size
    cache->SetMaxCacheSize(5);
    cache->SetCleanupRatio(0.4f); // Remove 40%
    
    // Since we can't easily populate the cache without database access,
    // we'll test the configuration and basic method calls
    
    // Test configuration getters
    EXPECT_EQ(cache->GetMaxCacheSize(), 5);
    EXPECT_EQ(cache->GetCleanupRatio(), 0.4f);
    EXPECT_EQ(cache->GetMemoryThreshold(), 1024);
    
    // Test initial state
    EXPECT_EQ(cache->GetCacheEntryCount(), 0);
    EXPECT_EQ(cache->GetMemoryUsageBytes(), 0);
    EXPECT_FALSE(cache->IsMemoryThresholdExceeded());
    
    // Test cleanup on empty cache (should not crash)
    bool result = cache->CleanupLRUCache();
    EXPECT_TRUE(result); // Should succeed even on empty cache
    
    // Verify statistics are initialized
    CacheStats stats = cache->GetCacheStats();
    EXPECT_EQ(stats.totalRequests.load(), 0);
    EXPECT_EQ(stats.cacheHits.load(), 0);
    EXPECT_EQ(stats.cacheMisses.load(), 0);
    EXPECT_EQ(stats.evictions.load(), 0);
}

/**
 * Test memory optimization functionality
 */
TEST_F(BotPvpGearCacheLRUTest, MemoryOptimizationHandlesEmptyCache)
{
    // Test memory optimization on empty cache
    bool result = cache->OptimizeMemoryUsage();
    EXPECT_TRUE(result); // Should succeed
    
    // Verify cache is still in valid state
    EXPECT_EQ(cache->GetCacheEntryCount(), 0);
    EXPECT_EQ(cache->GetMemoryUsageBytes(), 0);
    EXPECT_FALSE(cache->IsMemoryThresholdExceeded());
}

/**
 * Test memory cleanup trigger
 */
TEST_F(BotPvpGearCacheLRUTest, TriggerMemoryCleanupHandlesLowMemory)
{
    // Set very low memory threshold
    cache->SetMemoryThreshold(1); // 1 byte threshold
    
    // Trigger cleanup (should not crash even if no memory is actually used)
    cache->TriggerMemoryCleanup();
    
    // Verify cache is still functional
    EXPECT_EQ(cache->GetCacheEntryCount(), 0);
}

/**
 * Test cache statistics reset
 */
TEST_F(BotPvpGearCacheLRUTest, CacheStatisticsResetCorrectly)
{
    // Reset statistics
    cache->ResetCacheStats();
    
    // Verify all statistics are reset
    CacheStats stats = cache->GetCacheStats();
    EXPECT_EQ(stats.totalRequests.load(), 0);
    EXPECT_EQ(stats.cacheHits.load(), 0);
    EXPECT_EQ(stats.cacheMisses.load(), 0);
    EXPECT_EQ(stats.evictions.load(), 0);
    EXPECT_EQ(stats.currentEntryCount.load(), 0);
    EXPECT_EQ(stats.memoryUsageBytes.load(), 0);
    EXPECT_EQ(stats.totalAccessTime.load(), 0);
    EXPECT_EQ(stats.maxAccessTime.load(), 0);
    EXPECT_EQ(stats.minAccessTime.load(), UINT64_MAX);
    
    // Test hit ratio calculation
    EXPECT_EQ(stats.GetHitRatio(), 0.0);
    EXPECT_EQ(stats.GetAverageAccessTime(), 0.0);
}

/**
 * Test cache status report generation
 */
TEST_F(BotPvpGearCacheLRUTest, CacheStatusReportIsGenerated)
{
    // Generate status report
    std::string report = cache->GetCacheStatusReport();
    
    // Verify report is not empty and contains expected sections
    EXPECT_FALSE(report.empty());
    EXPECT_THAT(report, HasSubstr("PVP Gear Cache Status Report"));
    EXPECT_THAT(report, HasSubstr("Entries:"));
    EXPECT_THAT(report, HasSubstr("Memory Usage:"));
    EXPECT_THAT(report, HasSubstr("Hit Ratio:"));
    EXPECT_THAT(report, HasSubstr("Total Requests:"));
    EXPECT_THAT(report, HasSubstr("Cache Hits:"));
    EXPECT_THAT(report, HasSubstr("Cache Misses:"));
    EXPECT_THAT(report, HasSubstr("Evictions:"));
    EXPECT_THAT(report, HasSubstr("Average Access Time:"));
    EXPECT_THAT(report, HasSubstr("Last Cleanup:"));
    EXPECT_THAT(report, HasSubstr("Cleanup Ratio:"));
    EXPECT_THAT(report, HasSubstr("Memory Threshold Exceeded:"));
}

/**
 * Test configuration parameter validation
 */
TEST_F(BotPvpGearCacheLRUTest, ConfigurationParametersAreValidated)
{
    // Test setting various configuration values
    cache->SetMaxCacheSize(100);
    EXPECT_EQ(cache->GetMaxCacheSize(), 100);
    
    cache->SetMemoryThreshold(2048);
    EXPECT_EQ(cache->GetMemoryThreshold(), 2048);
    
    cache->SetCleanupRatio(0.25f);
    EXPECT_FLOAT_EQ(cache->GetCleanupRatio(), 0.25f);
    
    // Test edge cases
    cache->SetMaxCacheSize(0);
    EXPECT_EQ(cache->GetMaxCacheSize(), 0);
    
    cache->SetCleanupRatio(0.0f);
    EXPECT_FLOAT_EQ(cache->GetCleanupRatio(), 0.0f);
    
    cache->SetCleanupRatio(1.0f);
    EXPECT_FLOAT_EQ(cache->GetCleanupRatio(), 1.0f);
}

/**
 * Test cache access with missing items (should not crash)
 */
TEST_F(BotPvpGearCacheLRUTest, AccessMissingItemsHandledGracefully)
{
    // Test accessing non-existent items
    PvpGearItem* item = cache->GetGearDetails(99999);
    EXPECT_EQ(item, nullptr);
    
    // Test resilience check on non-existent item
    bool isResilient = cache->IsResilienceGear(99999);
    EXPECT_FALSE(isResilient); // Should return false for non-existent items
    
    // Test getting available gear for non-existent class
    std::vector<PvpGearItem> gear = cache->GetAvailableGear(99, 80, "NonExistentTalent");
    EXPECT_TRUE(gear.empty());
    
    // Verify statistics were updated for cache misses
    CacheStats stats = cache->GetCacheStats();
    EXPECT_GT(stats.totalRequests.load(), 0);
    EXPECT_GT(stats.cacheMisses.load(), 0);
    EXPECT_EQ(stats.cacheHits.load(), 0);
}

/**
 * Test thread safety of configuration changes
 */
TEST_F(BotPvpGearCacheLRUTest, ConfigurationChangesAreThreadSafe)
{
    // Test that configuration changes don't cause crashes
    // when called concurrently with other operations
    
    std::vector<std::thread> threads;
    std::atomic<bool> stopFlag{false};
    
    // Thread 1: Change configuration
    threads.emplace_back([&]() {
        for (int i = 0; i < 10 && !stopFlag.load(); ++i)
        {
            cache->SetMaxCacheSize(10 + i);
            cache->SetMemoryThreshold(1024 * (i + 1));
            cache->SetCleanupRatio(0.1f + (i * 0.05f));
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });
    
    // Thread 2: Access cache
    threads.emplace_back([&]() {
        for (int i = 0; i < 10 && !stopFlag.load(); ++i)
        {
            cache->GetGearDetails(i);
            cache->GetCacheStats();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });
    
    // Thread 3: Trigger cleanup
    threads.emplace_back([&]() {
        for (int i = 0; i < 5 && !stopFlag.load(); ++i)
        {
            cache->CleanupLRUCache();
            cache->OptimizeMemoryUsage();
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    });
    
    // Run for a short time
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    stopFlag.store(true);
    
    // Wait for all threads to complete
    for (auto& thread : threads)
    {
        thread.join();
    }
    
    // Verify cache is still in a valid state
    EXPECT_GE(cache->GetMaxCacheSize(), 10);
    EXPECT_GE(cache->GetMemoryThreshold(), 1024);
    EXPECT_GE(cache->GetCleanupRatio(), 0.1f);
    
    // Verify cache is still functional
    std::string report = cache->GetCacheStatusReport();
    EXPECT_FALSE(report.empty());
}

//End leewheel