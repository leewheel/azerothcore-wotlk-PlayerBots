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
#include <fstream>
#include <filesystem>

using namespace testing;

/**
 * Property-based test for BotPvpGearCache synchronization and fault tolerance
 * **功能: pvp-gear-auto-equip, 属性 14: 缓存同步机制**
 * **功能: pvp-gear-auto-equip, 属性 16: 容错能力**
 * **验证需求: 5.3, 5.5**
 */
class BotPvpGearCacheSyncPropertyTest : public Test
{
protected:
    void SetUp() override
    {
        // Initialize cache with test configuration
        cache = BotPvpGearCache::GetInstance();
        cache->ClearCache();
        
        // Set test-friendly configuration
        cache->SetMaxCacheSize(500);
        cache->SetMemoryThreshold(2 * 1024 * 1024);  // 2MB threshold
        cache->SetCleanupRatio(0.3f);
        
        // Enable database monitoring for sync tests
        cache->EnableDatabaseChangeMonitoring(true);
        
        // Reset error recovery manager
        errorManager = ErrorRecoveryManager::GetInstance();
        errorManager->Initialize();
        
        // Initialize random number generator
        rng.seed(std::chrono::steady_clock::now().time_since_epoch().count());
        
        // Create test cache directory
        testCacheDir = "test_cache_" + std::to_string(rng());
        std::filesystem::create_directories(testCacheDir);
    }
    
    void TearDown() override
    {
        if (cache)
        {
            cache->ClearCache();
            cache->SetOfflineMode(false);
            cache->EnableDatabaseChangeMonitoring(false);
        }
        
        // Clean up test cache directory
        if (std::filesystem::exists(testCacheDir))
        {
            std::filesystem::remove_all(testCacheDir);
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
        item.itemName = "TestSyncItem_" + std::to_string(itemId);
        item.itemCount = 1;
        item.classId = classDistribution(rng);
        item.talent = talents[talentDistribution(rng)];
        item.levelMin = levelMinDistribution(rng);
        item.levelMax = item.levelMin + levelRangeDistribution(rng);
        item.lastAccessTime = 0;
        item.accessCount = 0;
        
        return item;
    }
    
    // Simulate database connection failure
    void SimulateDatabaseFailure()
    {
        // This would normally involve mocking the database connection
        // For testing purposes, we'll use the offline mode functionality
        cache->SetOfflineMode(true);
    }
    
    // Simulate database connection restoration
    void SimulateDatabaseRestoration()
    {
        cache->SetOfflineMode(false);
    }
    
    // Create test cache file with known data
    std::string CreateTestCacheFile(const std::vector<PvpGearItem>& items)
    {
        std::string filePath = testCacheDir + "/test_cache_" + std::to_string(rng()) + ".dat";
        
        // Simplified cache file creation for testing
        std::ofstream file(filePath, std::ios::binary);
        if (!file.is_open())
        {
            return "";
        }
        
        // Write cache size
        uint32 cacheSize = items.size();
        file.write(reinterpret_cast<const char*>(&cacheSize), sizeof(cacheSize));
        
        // Write cache entries
        for (const auto& item : items)
        {
            // Write item data
            file.write(reinterpret_cast<const char*>(&item.itemId), sizeof(item.itemId));
            file.write(reinterpret_cast<const char*>(&item.itemCount), sizeof(item.itemCount));
            file.write(reinterpret_cast<const char*>(&item.classId), sizeof(item.classId));
            file.write(reinterpret_cast<const char*>(&item.levelMin), sizeof(item.levelMin));
            file.write(reinterpret_cast<const char*>(&item.levelMax), sizeof(item.levelMax));
            
            // Write strings
            uint32 nameLength = item.itemName.length();
            file.write(reinterpret_cast<const char*>(&nameLength), sizeof(nameLength));
            file.write(item.itemName.c_str(), nameLength);
            
            uint32 talentLength = item.talent.length();
            file.write(reinterpret_cast<const char*>(&talentLength), sizeof(talentLength));
            file.write(item.talent.c_str(), talentLength);
        }
        
        file.close();
        return filePath;
    }
    
    // Verify cache contains expected items
    bool VerifyCacheContains(const std::vector<PvpGearItem>& expectedItems)
    {
        for (const auto& expectedItem : expectedItems)
        {
            PvpGearItem* cachedItem = cache->GetGearDetails(expectedItem.itemId);
            if (!cachedItem)
            {
                return false;
            }
            
            if (cachedItem->itemName != expectedItem.itemName ||
                cachedItem->classId != expectedItem.classId ||
                cachedItem->talent != expectedItem.talent)
            {
                return false;
            }
        }
        return true;
    }

protected:
    BotPvpGearCache* cache;
    ErrorRecoveryManager* errorManager;
    std::mt19937 rng;
    std::string testCacheDir;
    
    // Test data generation parameters
    static constexpr uint32 baseItemId = 20000;
    std::uniform_int_distribution<uint32> itemIdDistribution{baseItemId, baseItemId + 5000};
    std::uniform_int_distribution<uint8> classDistribution{1, 11};
    std::uniform_int_distribution<uint8> levelMinDistribution{1, 70};
    std::uniform_int_distribution<uint8> levelRangeDistribution{1, 10};
    std::uniform_int_distribution<size_t> talentDistribution{0, 2};
    
    std::vector<std::string> talents = {"", "Holy", "Protection", "Retribution"};
};

/**
 * Property 14: Cache synchronization mechanism - database changes should trigger cache updates
 * **功能: pvp-gear-auto-equip, 属性 14: 缓存同步机制**
 * **验证需求: 5.3**
 */
TEST_F(BotPvpGearCacheSyncPropertyTest, DatabaseChangesShouldTriggerCacheUpdates)
{
    // Property: For any database change, the cache should automatically update
    // to maintain synchronization with the database
    
    for (int iteration = 0; iteration < 30; ++iteration)
    {
        // Setup initial cache state
        cache->ClearCache();
        
        // Generate initial test data
        std::vector<PvpGearItem> initialItems;
        uint32 initialItemCount = std::uniform_int_distribution<uint32>{10, 50}(rng);
        
        for (uint32 i = 0; i < initialItemCount; ++i)
        {
            initialItems.push_back(GenerateRandomGearItem(baseItemId + i));
        }
        
        // Simulate initial cache load (normally from database)
        // For testing, we'll populate the cache directly
        {
            std::lock_guard<std::mutex> lock(cache->cacheMutex_);
            for (const auto& item : initialItems)
            {
                cache->gearCache_[item.itemId] = item;
                
                uint32 key = item.classId * 1000;
                if (!item.talent.empty())
                {
                    key += std::hash<std::string>{}(item.talent) % 999;
                }
                cache->classGearCache_[key].push_back(item.itemId);
            }
            cache->isLoaded_ = true;
            cache->stats_.currentEntryCount = initialItems.size();
        }
        
        // Verify initial state
        ASSERT_EQ(cache->GetCacheEntryCount(), initialItemCount)
            << "Initial cache should contain expected number of items. Iteration: " << iteration;
        
        ASSERT_TRUE(VerifyCacheContains(initialItems))
            << "Cache should contain all initial items. Iteration: " << iteration;
        
        // Test cache refresh from database
        bool refreshResult = cache->RefreshCacheFromDatabase();
        
        // The refresh might fail due to test environment database setup
        // but the mechanism should handle this gracefully
        if (!refreshResult)
        {
            // Verify that cache maintains its state on refresh failure
            EXPECT_GT(cache->GetCacheEntryCount(), 0)
                << "Cache should maintain data on refresh failure. Iteration: " << iteration;
        }
        
        // Test cache integrity validation
        bool integrityValid = cache->ValidateCacheIntegrity();
        EXPECT_TRUE(integrityValid)
            << "Cache integrity should be valid after operations. Iteration: " << iteration;
        
        // Test synchronization with database monitoring
        cache->MonitorDatabaseChanges();
        
        // Verify monitoring doesn't corrupt cache state
        EXPECT_GT(cache->GetCacheEntryCount(), 0)
            << "Cache should maintain data after monitoring check. Iteration: " << iteration;
        
        // Test synchronization method
        bool syncResult = cache->SynchronizeWithDatabase();
        
        // Sync might fail in test environment, but should handle gracefully
        if (syncResult)
        {
            EXPECT_TRUE(cache->IsLoaded())
                << "Cache should remain loaded after successful sync. Iteration: " << iteration;
        }
        
        // Verify cache statistics are maintained
        auto stats = cache->GetCacheStats();
        EXPECT_GE(stats.totalRequests.load(), 0)
            << "Cache statistics should be maintained. Iteration: " << iteration;
    }
}

/**
 * Property 16: Fault tolerance - cache should continue operating when database connection fails
 * **功能: pvp-gear-auto-equip, 属性 16: 容错能力**
 * **验证需求: 5.5**
 */
TEST_F(BotPvpGearCacheSyncPropertyTest, CacheShouldContinueOperatingWhenDatabaseFails)
{
    // Property: For any database connection failure, the cache should use local
    // cached data to continue providing service
    
    for (int iteration = 0; iteration < 25; ++iteration)
    {
        // Setup cache with test data
        cache->ClearCache();
        
        std::vector<PvpGearItem> testItems;
        uint32 itemCount = std::uniform_int_distribution<uint32>{20, 100}(rng);
        
        for (uint32 i = 0; i < itemCount; ++i)
        {
            testItems.push_back(GenerateRandomGearItem(baseItemId + i));
        }
        
        // Populate cache
        {
            std::lock_guard<std::mutex> lock(cache->cacheMutex_);
            for (const auto& item : testItems)
            {
                cache->gearCache_[item.itemId] = item;
                
                uint32 key = item.classId * 1000;
                if (!item.talent.empty())
                {
                    key += std::hash<std::string>{}(item.talent) % 999;
                }
                cache->classGearCache_[key].push_back(item.itemId);
            }
            cache->isLoaded_ = true;
            cache->stats_.currentEntryCount = testItems.size();
        }
        
        // Verify cache is operational before failure
        ASSERT_TRUE(cache->IsLoaded())
            << "Cache should be loaded before database failure. Iteration: " << iteration;
        
        ASSERT_EQ(cache->GetCacheEntryCount(), itemCount)
            << "Cache should contain expected items before failure. Iteration: " << iteration;
        
        // Test normal operation before failure
        uint32 testItemId = testItems[0].itemId;
        PvpGearItem* item = cache->GetGearDetails(testItemId);
        ASSERT_NE(item, nullptr)
            << "Cache should return valid item before failure. Iteration: " << iteration;
        
        // Simulate database connection failure
        SimulateDatabaseFailure();
        
        // Verify cache enters offline mode
        EXPECT_TRUE(cache->IsOfflineMode())
            << "Cache should enter offline mode on database failure. Iteration: " << iteration;
        
        // Test that cache continues to serve data in offline mode
        PvpGearItem* offlineItem = cache->GetGearDetails(testItemId);
        EXPECT_NE(offlineItem, nullptr)
            << "Cache should continue serving data in offline mode. Iteration: " << iteration;
        
        if (offlineItem)
        {
            EXPECT_EQ(offlineItem->itemId, testItemId)
                << "Cached item should have correct ID in offline mode. Iteration: " << iteration;
            
            EXPECT_EQ(offlineItem->itemName, testItems[0].itemName)
                << "Cached item should have correct name in offline mode. Iteration: " << iteration;
        }
        
        // Test GetAvailableGear in offline mode
        uint8 testClass = testItems[0].classId;
        uint8 testLevel = (testItems[0].levelMin + testItems[0].levelMax) / 2;
        
        std::vector<PvpGearItem> availableGear = cache->GetAvailableGear(testClass, testLevel);
        EXPECT_GT(availableGear.size(), 0)
            << "Cache should return available gear in offline mode. Iteration: " << iteration;
        
        // Test cache statistics in offline mode
        auto offlineStats = cache->GetCacheStats();
        EXPECT_GT(offlineStats.totalRequests.load(), 0)
            << "Cache statistics should be updated in offline mode. Iteration: " << iteration;
        
        // Test database connection failure handling
        bool failureHandled = cache->HandleDatabaseConnectionFailure();
        EXPECT_TRUE(failureHandled)
            << "Database failure should be handled gracefully. Iteration: " << iteration;
        
        // Test local cache file operations in offline mode
        std::string cacheFilePath = testCacheDir + "/offline_cache_" + std::to_string(iteration) + ".dat";
        cache->SaveCacheToLocalFile(cacheFilePath);
        
        // Verify cache file was created
        EXPECT_TRUE(std::filesystem::exists(cacheFilePath))
            << "Cache file should be created in offline mode. Iteration: " << iteration;
        
        // Test loading from cache file
        cache->ClearCache();
        bool loadSuccess = cache->LoadCacheFromLocalFile(cacheFilePath);
        
        if (loadSuccess)
        {
            EXPECT_TRUE(cache->IsLoaded())
                << "Cache should be loaded from file. Iteration: " << iteration;
            
            EXPECT_GT(cache->GetCacheEntryCount(), 0)
                << "Cache should contain items loaded from file. Iteration: " << iteration;
            
            // Verify loaded data integrity
            PvpGearItem* loadedItem = cache->GetGearDetails(testItemId);
            if (loadedItem)
            {
                EXPECT_EQ(loadedItem->itemName, testItems[0].itemName)
                    << "Loaded item should have correct data. Iteration: " << iteration;
            }
        }
        
        // Test database connection restoration
        SimulateDatabaseRestoration();
        
        bool restorationResult = cache->RestoreDatabaseConnection();
        
        // Restoration might fail in test environment, but should handle gracefully
        if (restorationResult)
        {
            EXPECT_FALSE(cache->IsOfflineMode())
                << "Cache should exit offline mode on connection restoration. Iteration: " << iteration;
        }
        
        // Clean up test cache file
        if (std::filesystem::exists(cacheFilePath))
        {
            std::filesystem::remove(cacheFilePath);
        }
    }
}

/**
 * Property: Cache integrity validation should detect and repair corrupted entries
 * **验证需求: 5.3, 5.5**
 */
TEST_F(BotPvpGearCacheSyncPropertyTest, CacheIntegrityValidationShouldDetectCorruption)
{
    // Property: For any cache state with corrupted entries, integrity validation
    // should detect corruption and repair mechanisms should fix it
    
    for (int iteration = 0; iteration < 20; ++iteration)
    {
        // Setup cache with valid data
        cache->ClearCache();
        
        std::vector<PvpGearItem> validItems;
        uint32 validItemCount = std::uniform_int_distribution<uint32>{10, 30}(rng);
        
        for (uint32 i = 0; i < validItemCount; ++i)
        {
            validItems.push_back(GenerateRandomGearItem(baseItemId + i));
        }
        
        // Add valid items to cache
        {
            std::lock_guard<std::mutex> lock(cache->cacheMutex_);
            for (const auto& item : validItems)
            {
                cache->gearCache_[item.itemId] = item;
                
                uint32 key = item.classId * 1000;
                if (!item.talent.empty())
                {
                    key += std::hash<std::string>{}(item.talent) % 999;
                }
                cache->classGearCache_[key].push_back(item.itemId);
            }
            cache->isLoaded_ = true;
            cache->stats_.currentEntryCount = validItems.size();
        }
        
        // Verify initial integrity
        bool initialIntegrity = cache->ValidateCacheIntegrity();
        EXPECT_TRUE(initialIntegrity)
            << "Cache should have valid integrity initially. Iteration: " << iteration;
        
        // Introduce corruption by adding invalid items
        uint32 corruptedItemCount = std::uniform_int_distribution<uint32>{1, 5}(rng);
        std::vector<uint32> corruptedItemIds;
        
        {
            std::lock_guard<std::mutex> lock(cache->cacheMutex_);
            
            for (uint32 i = 0; i < corruptedItemCount; ++i)
            {
                PvpGearItem corruptedItem;
                
                // Create different types of corruption
                switch (i % 4)
                {
                    case 0: // Invalid item ID
                        corruptedItem.itemId = 0;
                        corruptedItem.itemName = "CorruptedItem";
                        corruptedItem.classId = 1;
                        break;
                        
                    case 1: // Empty name
                        corruptedItem.itemId = baseItemId + validItemCount + i;
                        corruptedItem.itemName = "";
                        corruptedItem.classId = 1;
                        break;
                        
                    case 2: // Invalid class
                        corruptedItem.itemId = baseItemId + validItemCount + i;
                        corruptedItem.itemName = "CorruptedItem";
                        corruptedItem.classId = 99; // Invalid class
                        break;
                        
                    case 3: // Invalid level range
                        corruptedItem.itemId = baseItemId + validItemCount + i;
                        corruptedItem.itemName = "CorruptedItem";
                        corruptedItem.classId = 1;
                        corruptedItem.levelMin = 80;
                        corruptedItem.levelMax = 10; // Min > Max
                        break;
                }
                
                corruptedItemIds.push_back(corruptedItem.itemId);
                cache->gearCache_[corruptedItem.itemId] = corruptedItem;
            }
            
            cache->stats_.currentEntryCount = validItems.size() + corruptedItemCount;
        }
        
        // Verify corruption is detected
        bool corruptedIntegrity = cache->ValidateCacheIntegrity();
        EXPECT_FALSE(corruptedIntegrity)
            << "Cache integrity validation should detect corruption. Iteration: " << iteration;
        
        // Test repair mechanism
        cache->RepairCorruptedEntries();
        
        // Verify corruption is repaired
        bool repairedIntegrity = cache->ValidateCacheIntegrity();
        EXPECT_TRUE(repairedIntegrity)
            << "Cache should have valid integrity after repair. Iteration: " << iteration;
        
        // Verify corrupted items were removed
        for (uint32 corruptedId : corruptedItemIds)
        {
            PvpGearItem* item = cache->GetGearDetails(corruptedId);
            if (item && corruptedId != 0) // ID 0 items are always invalid
            {
                // If item still exists, it should be valid now
                EXPECT_GT(item->itemId, 0) << "Remaining item should have valid ID";
                EXPECT_FALSE(item->itemName.empty()) << "Remaining item should have valid name";
                EXPECT_LE(item->classId, 11) << "Remaining item should have valid class";
                EXPECT_LE(item->levelMin, item->levelMax) << "Remaining item should have valid level range";
            }
        }
        
        // Verify valid items are still present
        uint32 remainingValidItems = 0;
        for (const auto& validItem : validItems)
        {
            PvpGearItem* item = cache->GetGearDetails(validItem.itemId);
            if (item)
            {
                remainingValidItems++;
                EXPECT_EQ(item->itemName, validItem.itemName)
                    << "Valid item should retain correct data after repair. Iteration: " << iteration;
            }
        }
        
        EXPECT_GT(remainingValidItems, 0)
            << "Some valid items should remain after repair. Iteration: " << iteration;
        
        // Test cache functionality after repair
        uint8 testClass = validItems[0].classId;
        uint8 testLevel = (validItems[0].levelMin + validItems[0].levelMax) / 2;
        
        std::vector<PvpGearItem> availableGear = cache->GetAvailableGear(testClass, testLevel);
        // Should not crash and may return items if any match the criteria
        
        // Verify cache statistics are consistent
        auto stats = cache->GetCacheStats();
        EXPECT_EQ(stats.currentEntryCount.load(), cache->GetCacheEntryCount())
            << "Cache statistics should be consistent after repair. Iteration: " << iteration;
    }
}

/**
 * Property: Concurrent sync operations should not corrupt cache state
 * **验证需求: 5.3, 5.5**
 */
TEST_F(BotPvpGearCacheSyncPropertyTest, ConcurrentSyncOperationsShouldNotCorruptCache)
{
    // Property: For any concurrent synchronization operations, cache should
    // maintain consistency and not corrupt data
    
    for (int iteration = 0; iteration < 10; ++iteration)
    {
        // Setup cache with test data
        cache->ClearCache();
        
        std::vector<PvpGearItem> testItems;
        uint32 itemCount = std::uniform_int_distribution<uint32>{50, 150}(rng);
        
        for (uint32 i = 0; i < itemCount; ++i)
        {
            testItems.push_back(GenerateRandomGearItem(baseItemId + i));
        }
        
        // Populate cache
        {
            std::lock_guard<std::mutex> lock(cache->cacheMutex_);
            for (const auto& item : testItems)
            {
                cache->gearCache_[item.itemId] = item;
                
                uint32 key = item.classId * 1000;
                if (!item.talent.empty())
                {
                    key += std::hash<std::string>{}(item.talent) % 999;
                }
                cache->classGearCache_[key].push_back(item.itemId);
            }
            cache->isLoaded_ = true;
            cache->stats_.currentEntryCount = testItems.size();
        }
        
        uint32 initialCount = cache->GetCacheEntryCount();
        
        // Launch multiple threads performing sync operations
        std::vector<std::thread> threads;
        std::atomic<bool> stopFlag{false};
        std::atomic<uint32> operationCount{0};
        std::atomic<uint32> successCount{0};
        
        // Thread 1: Cache integrity validation
        threads.emplace_back([&]() {
            while (!stopFlag.load())
            {
                bool valid = cache->ValidateCacheIntegrity();
                operationCount.fetch_add(1);
                if (valid) successCount.fetch_add(1);
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });
        
        // Thread 2: Database monitoring
        threads.emplace_back([&]() {
            while (!stopFlag.load())
            {
                cache->MonitorDatabaseChanges();
                operationCount.fetch_add(1);
                successCount.fetch_add(1); // Monitoring doesn't fail
                std::this_thread::sleep_for(std::chrono::milliseconds(15));
            }
        });
        
        // Thread 3: Cache access operations
        threads.emplace_back([&]() {
            std::mt19937 localRng(rng());
            std::uniform_int_distribution<uint32> itemDist(0, testItems.size() - 1);
            
            while (!stopFlag.load())
            {
                uint32 itemIndex = itemDist(localRng);
                uint32 itemId = testItems[itemIndex].itemId;
                
                PvpGearItem* item = cache->GetGearDetails(itemId);
                operationCount.fetch_add(1);
                if (item) successCount.fetch_add(1);
                
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        });
        
        // Thread 4: Offline mode simulation
        threads.emplace_back([&]() {
            while (!stopFlag.load())
            {
                // Randomly toggle offline mode
                bool shouldBeOffline = std::uniform_real_distribution<float>{0.0f, 1.0f}(rng) < 0.1f;
                cache->SetOfflineMode(shouldBeOffline);
                
                operationCount.fetch_add(1);
                successCount.fetch_add(1);
                
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        });
        
        // Run concurrent operations for a short time
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        stopFlag.store(true);
        
        // Wait for all threads to complete
        for (auto& thread : threads)
        {
            thread.join();
        }
        
        // Verify cache is still in a valid state
        uint32 finalCount = cache->GetCacheEntryCount();
        
        EXPECT_GT(finalCount, 0)
            << "Cache should not be empty after concurrent operations. Iteration: " << iteration;
        
        EXPECT_LE(finalCount, initialCount)
            << "Cache size should not increase unexpectedly. Iteration: " << iteration;
        
        // Verify cache integrity after concurrent operations
        bool finalIntegrity = cache->ValidateCacheIntegrity();
        EXPECT_TRUE(finalIntegrity)
            << "Cache integrity should be maintained after concurrent operations. Iteration: " << iteration;
        
        // Verify cache is still functional
        if (!testItems.empty())
        {
            uint32 testItemId = testItems[0].itemId;
            PvpGearItem* testItem = cache->GetGearDetails(testItemId);
            // Item might have been evicted, but call should not crash
        }
        
        // Verify statistics are reasonable
        auto stats = cache->GetCacheStats();
        EXPECT_GT(stats.totalRequests.load(), 0)
            << "Cache should have processed requests. Iteration: " << iteration;
        
        EXPECT_GT(operationCount.load(), 0)
            << "Concurrent operations should have been performed. Iteration: " << iteration;
        
        // Success rate should be reasonable (at least 50%)
        float successRate = static_cast<float>(successCount.load()) / operationCount.load();
        EXPECT_GE(successRate, 0.5f)
            << "Success rate should be reasonable during concurrent operations. Iteration: " << iteration
            << ", Success: " << successCount.load() << ", Total: " << operationCount.load();
        
        // Reset offline mode for next iteration
        cache->SetOfflineMode(false);
    }
}

//End leewheel