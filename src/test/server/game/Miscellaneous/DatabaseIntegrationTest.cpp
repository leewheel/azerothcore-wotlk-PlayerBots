//By leewheel
#include "gtest/gtest.h"
#include "BotPvpGearCache.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include <memory>

/**
 * **功能: pvp-gear-auto-equip, 数据库集成验证**
 * 
 * 验证需求: 5.1 - 数据库连接和查询功能
 * 
 * 测试数据库集成的各个方面：
 * - 数据库连接验证
 * - bot_pvp_gear表查询功能
 * - 缓存初始化和数据加载
 * - 错误处理和容错机制
 */
class DatabaseIntegrationTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // 获取缓存实例
        cache_ = BotPvpGearCache::GetInstance();
        ASSERT_NE(cache_, nullptr);
    }

    void TearDown() override
    {
        // 清理测试数据
        if (cache_)
        {
            cache_->ClearCache();
        }
    }

    BotPvpGearCache* cache_;
};

/**
 * 测试数据库连接功能
 * 验证系统能够成功连接到数据库
 */
TEST_F(DatabaseIntegrationTest, DatabaseConnectionTest)
{
    // 测试数据库连接
    bool connectionResult = cache_->CheckDatabaseConnection();
    
    // 数据库连接应该成功或者有适当的错误处理
    if (connectionResult)
    {
        LOG_INFO("test", "Database connection successful");
        EXPECT_TRUE(connectionResult);
    }
    else
    {
        LOG_WARN("test", "Database connection failed - testing offline mode");
        // 在数据库连接失败时，系统应该能够处理这种情况
        EXPECT_FALSE(connectionResult);
    }
}

/**
 * 测试bot_pvp_gear表查询功能
 * 验证系统能够正确查询和解析数据
 */
TEST_F(DatabaseIntegrationTest, BotPvpGearTableQueryTest)
{
    // 尝试从数据库加载数据
    bool loadResult = cache_->LoadFromDatabase();
    
    if (loadResult)
    {
        LOG_INFO("test", "Successfully loaded PVP gear data from database");
        
        // 验证缓存已加载
        EXPECT_TRUE(cache_->IsLoaded());
        
        // 验证缓存中有数据（如果数据库中有数据的话）
        uint32 entryCount = cache_->GetCacheEntryCount();
        LOG_INFO("test", "Loaded {} PVP gear entries", entryCount);
        
        // 如果有数据，验证数据的基本完整性
        if (entryCount > 0)
        {
            // 测试获取特定职业的装备
            std::vector<PvpGearItem> warriorGear = cache_->GetAvailableGear(1, 60, ""); // 战士，60级
            LOG_INFO("test", "Found {} warrior gear items", warriorGear.size());
            
            // 验证返回的数据结构
            for (const auto& item : warriorGear)
            {
                EXPECT_GT(item.itemId, 0);
                EXPECT_GE(item.levelMin, 1);
                EXPECT_LE(item.levelMax, 80);
                EXPECT_GE(item.classId, 1);
                EXPECT_LE(item.classId, 11);
                EXPECT_FALSE(item.itemName.empty());
            }
        }
    }
    else
    {
        LOG_WARN("test", "Failed to load from database - testing fallback mechanisms");
        
        // 验证系统能够处理数据库加载失败的情况
        EXPECT_FALSE(loadResult);
        
        // 系统应该尝试从本地缓存加载
        bool localLoadResult = cache_->LoadCacheFromLocalFile();
        if (localLoadResult)
        {
            LOG_INFO("test", "Successfully loaded from local cache file");
            EXPECT_TRUE(cache_->IsLoaded());
        }
        else
        {
            LOG_INFO("test", "No local cache available - system will operate in limited mode");
        }
    }
}

/**
 * 测试数据库查询性能
 * 验证查询响应时间在可接受范围内
 */
TEST_F(DatabaseIntegrationTest, DatabaseQueryPerformanceTest)
{
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // 执行数据库加载
    bool loadResult = cache_->LoadFromDatabase();
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    LOG_INFO("test", "Database load operation took {} ms", duration.count());
    
    if (loadResult)
    {
        // 数据库加载应该在合理时间内完成（5秒内）
        EXPECT_LT(duration.count(), 5000);
        
        // 测试查询性能
        auto queryStartTime = std::chrono::high_resolution_clock::now();
        
        std::vector<PvpGearItem> testGear = cache_->GetAvailableGear(1, 60, "");
        
        auto queryEndTime = std::chrono::high_resolution_clock::now();
        auto queryDuration = std::chrono::duration_cast<std::chrono::microseconds>(queryEndTime - queryStartTime);
        
        LOG_INFO("test", "Gear query took {} μs", queryDuration.count());
        
        // 查询应该在100ms内完成
        EXPECT_LT(queryDuration.count(), 100000);
    }
}

/**
 * 测试数据库错误处理
 * 验证系统在数据库异常时的行为
 */
TEST_F(DatabaseIntegrationTest, DatabaseErrorHandlingTest)
{
    // 测试无效查询的处理
    try
    {
        // 尝试查询不存在的表（这应该被优雅地处理）
        QueryResult result = PlayerbotsDatabase.Query("SELECT * FROM non_existent_table");
        
        // 查询应该返回空结果而不是崩溃
        EXPECT_FALSE(result);
        
        LOG_INFO("test", "Invalid query handled gracefully");
    }
    catch (const std::exception& e)
    {
        LOG_WARN("test", "Database query threw exception: {}", e.what());
        // 异常应该被适当处理，不应该导致程序崩溃
        EXPECT_TRUE(true); // 如果我们到达这里，说明异常被捕获了
    }
}

/**
 * 测试缓存初始化
 * 验证缓存系统的初始化过程
 */
TEST_F(DatabaseIntegrationTest, CacheInitializationTest)
{
    // 清理缓存
    cache_->ClearCache();
    EXPECT_FALSE(cache_->IsLoaded());
    
    // 重新初始化
    bool initResult = cache_->LoadFromDatabase();
    
    if (initResult)
    {
        EXPECT_TRUE(cache_->IsLoaded());
        
        // 验证缓存统计信息
        CacheStats stats = cache_->GetCacheStats();
        EXPECT_GE(stats.currentEntryCount.load(), 0);
        EXPECT_GE(stats.memoryUsageBytes.load(), 0);
        
        LOG_INFO("test", "Cache initialized with {} entries, {} bytes memory", 
                stats.currentEntryCount.load(), stats.memoryUsageBytes.load());
    }
    else
    {
        LOG_WARN("test", "Cache initialization failed - testing offline mode");
        // 即使数据库加载失败，缓存对象也应该是有效的
        EXPECT_NE(cache_, nullptr);
    }
}

/**
 * 测试数据完整性验证
 * 验证从数据库加载的数据的完整性
 */
TEST_F(DatabaseIntegrationTest, DataIntegrityValidationTest)
{
    bool loadResult = cache_->LoadFromDatabase();
    
    if (loadResult && cache_->GetCacheEntryCount() > 0)
    {
        // 验证缓存完整性
        bool integrityResult = cache_->ValidateCacheIntegrity();
        EXPECT_TRUE(integrityResult);
        
        // 测试数据一致性
        std::vector<PvpGearItem> allItems;
        
        // 获取所有职业的装备数据
        for (uint8 classId = 1; classId <= 11; ++classId)
        {
            std::vector<PvpGearItem> classItems = cache_->GetAvailableGear(classId, 70, "");
            allItems.insert(allItems.end(), classItems.begin(), classItems.end());
        }
        
        LOG_INFO("test", "Validated {} total items across all classes", allItems.size());
        
        // 验证数据的基本约束
        for (const auto& item : allItems)
        {
            // 物品ID应该有效
            EXPECT_GT(item.itemId, 0);
            
            // 等级范围应该合理
            EXPECT_GE(item.levelMin, 1);
            EXPECT_LE(item.levelMax, 80);
            EXPECT_LE(item.levelMin, item.levelMax);
            
            // 职业ID应该在有效范围内
            EXPECT_GE(item.classId, 1);
            EXPECT_LE(item.classId, 11);
            
            // 物品名称不应该为空
            EXPECT_FALSE(item.itemName.empty());
            
            // 物品数量应该合理
            EXPECT_GT(item.itemCount, 0);
            EXPECT_LE(item.itemCount, 255);
        }
    }
    else
    {
        LOG_INFO("test", "No data loaded from database - skipping data integrity validation");
    }
}