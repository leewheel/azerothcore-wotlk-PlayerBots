/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license, you may redistribute it
 * and/or modify it under version 3 of the License, or (at your option), any later version.
 */

#include <gtest/gtest.h>
#include <random>
#include <vector>
#include <memory>
#include <unordered_set>
#include <chrono>
#include <algorithm>

#include "SharedDefines.h"
#include "modules/mod-playerbots/src/Bot/InventoryManager.h"
#include "PlayerMock.h"

// Forward declarations for types used in MockPlayer
class Item;
class Bag;
typedef std::vector<ItemPosCount> ItemPosCountVec;

// Mock ItemPosCount structure
struct ItemPosCount
{
    uint16 pos;
    uint32 count;
    ItemPosCount(uint16 _pos, uint32 _count) : pos(_pos), count(_count) {}
};

// Mock InventoryResult enum values if not available
#ifndef EQUIP_ERR_OK
enum InventoryResult
{
    EQUIP_ERR_OK = 0,
    EQUIP_ERR_INVENTORY_FULL = 18,
    // Add other values as needed
};
#endif

/**
 * Property-Based Tests for InventoryManager Bag and Item Management System
 * 
 * **功能: pvp-gear-auto-equip, 属性 6: 物品保护机制**
 * **功能: pvp-gear-auto-equip, 属性 7: 背包空间管理**
 * **功能: pvp-gear-auto-equip, 属性 8: 装备替换存储**
 * 
 * 验证需求: 3.1, 3.2, 3.3, 3.4, 3.5
 */

class InventoryManagerPropertyTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // 初始化随机数生成器
        rng_.seed(std::chrono::steady_clock::now().time_since_epoch().count());
        
        // 创建InventoryManager实例
        inventoryManager_ = std::make_unique<InventoryManager>();
        ASSERT_TRUE(inventoryManager_->Initialize()) << "InventoryManager initialization failed";
        
        // 初始化测试数据
        InitializeTestData();
    }
    
    void TearDown() override
    {
        // 清理测试数据
        testItemTemplates_.clear();
        inventoryManager_.reset();
    }
    
    // 测试用的机器人数据结构
    struct TestBotData
    {
        uint8 playerClass;
        uint8 level;
        uint64 guid;
        std::string name;
        uint32 totalBagSlots;
        uint32 usedBagSlots;
        std::vector<uint32> importantItems;  // 重要物品ID列表
        std::vector<uint32> regularItems;    // 普通物品ID列表
        std::unordered_map<uint8, uint32> equippedItems; // 装备槽位 -> 物品ID
        
        TestBotData() : playerClass(CLASS_WARRIOR), level(60), guid(1000), name("TestBot"), 
                       totalBagSlots(16), usedBagSlots(0) {}
    };
    
    // 生成随机的机器人数据
    TestBotData GenerateRandomBot()
    {
        TestBotData bot;
        
        // 随机职业 (1-11, 跳过无效职业)
        std::uniform_int_distribution<uint8> classDist(CLASS_WARRIOR, CLASS_DRUID);
        bot.playerClass = classDist(rng_);
        
        // 随机等级 (1-80)
        std::uniform_int_distribution<uint8> levelDist(1, 80);
        bot.level = levelDist(rng_);
        
        // 随机GUID
        std::uniform_int_distribution<uint64> guidDist(1000, 999999);
        bot.guid = guidDist(rng_);
        bot.name = "TestBot" + std::to_string(bot.guid);
        
        // 随机背包配置
        std::uniform_int_distribution<uint32> bagSlotsDist(16, 64);
        bot.totalBagSlots = bagSlotsDist(rng_);
        
        std::uniform_int_distribution<uint32> usedSlotsDist(0, bot.totalBagSlots);
        bot.usedBagSlots = usedSlotsDist(rng_);
        
        // 生成重要物品
        GenerateImportantItems(bot);
        
        // 生成普通物品
        GenerateRegularItems(bot);
        
        // 生成装备
        GenerateEquippedItems(bot);
        
        return bot;
    }
    
    // 测试用的物品数据结构
    struct TestItemData
    {
        uint32 itemId;
        std::string itemName;
        ImportantItemType importantType;
        bool isImportant;
        uint32 priority;
        uint32 stackSize;
        uint32 itemLevel;
        uint32 quality;
        uint8 inventoryType;
        
        TestItemData() : itemId(0), itemName(""), importantType(IMPORTANT_ITEM_MAX), 
                        isImportant(false), priority(10), stackSize(1), itemLevel(1), 
                        quality(ITEM_QUALITY_NORMAL), inventoryType(INVTYPE_NON_EQUIP) {}
    };
    
    // 生成随机的物品数据
    TestItemData GenerateRandomItem(bool forceImportant = false)
    {
        TestItemData item;
        
        if (forceImportant || (std::uniform_int_distribution<int>(0, 3)(rng_) == 0))
        {
            // 生成重要物品
            item = GenerateImportantItem();
        }
        else
        {
            // 生成普通物品
            item = GenerateRegularItem();
        }
        
        return item;
    }
    
    // 模拟Player类的简化版本
    class MockPlayer : public PlayerMock
    {
    public:
        MockPlayer(const TestBotData& data) : PlayerMock(), botData_(data), bagSpaceInfo_() 
        {
            // 设置基础属性
            SetClass(botData_.playerClass);
            SetLevel(botData_.level);
            SetGUID(ObjectGuid::Create<HighGuid::Player>(botData_.guid));
            SetName(botData_.name);
            
            // 初始化背包空间信息
            bagSpaceInfo_.totalSlots = botData_.totalBagSlots;
            bagSpaceInfo_.usedSlots = botData_.usedBagSlots;
            bagSpaceInfo_.freeSlots = botData_.totalBagSlots - botData_.usedBagSlots;
        }
        
        uint8 getClass() const override { return botData_.playerClass; }
        uint8 GetLevel() const override { return botData_.level; }
        ObjectGuid GetGUID() const override { return ObjectGuid::Create<HighGuid::Player>(botData_.guid); }
        std::string GetName() const override { return botData_.name; }
        
        // 背包相关方法
        const BagSpaceInfo& GetBagSpaceInfo() const { return bagSpaceInfo_; }
        void SetBagSpaceInfo(const BagSpaceInfo& info) { bagSpaceInfo_ = info; }
        
        // 物品相关方法
        const std::vector<uint32>& GetImportantItems() const { return botData_.importantItems; }
        const std::vector<uint32>& GetRegularItems() const { return botData_.regularItems; }
        const std::unordered_map<uint8, uint32>& GetEquippedItems() const { return botData_.equippedItems; }
        
        // 模拟物品查找
        bool HasItem(uint32 itemId) const
        {
            for (uint32 id : botData_.importantItems)
                if (id == itemId) return true;
            for (uint32 id : botData_.regularItems)
                if (id == itemId) return true;
            return false;
        }
        
        // 模拟装备查找
        bool HasEquippedItem(uint8 slot) const
        {
            return botData_.equippedItems.find(slot) != botData_.equippedItems.end();
        }
        
        uint32 GetEquippedItemId(uint8 slot) const
        {
            auto it = botData_.equippedItems.find(slot);
            return it != botData_.equippedItems.end() ? it->second : 0;
        }
        
        // 模拟背包空间操作
        bool AddItem(uint32 itemId)
        {
            if (bagSpaceInfo_.freeSlots > 0)
            {
                botData_.regularItems.push_back(itemId);
                bagSpaceInfo_.usedSlots++;
                bagSpaceInfo_.freeSlots--;
                return true;
            }
            return false;
        }
        
        bool RemoveItem(uint32 itemId)
        {
            auto it = std::find(botData_.regularItems.begin(), botData_.regularItems.end(), itemId);
            if (it != botData_.regularItems.end())
            {
                botData_.regularItems.erase(it);
                bagSpaceInfo_.usedSlots--;
                bagSpaceInfo_.freeSlots++;
                return true;
            }
            return false;
        }
        
        // 模拟装备操作
        bool EquipItem(uint32 itemId, uint8 slot)
        {
            botData_.equippedItems[slot] = itemId;
            return true;
        }
        
        bool UnequipItem(uint8 slot)
        {
            auto it = botData_.equippedItems.find(slot);
            if (it != botData_.equippedItems.end())
            {
                botData_.equippedItems.erase(it);
                return true;
            }
            return false;
        }
        
        // 模拟种族获取（用于装备验证）
        uint8 getRace() const { return RACE_HUMAN; } // 默认人类
        
        // 模拟背包和物品位置查询（返回nullptr表示空槽位或不存在）
        Item* GetItemByPos(uint8 bag, uint8 slot) const override
        {
            // 简化实现：总是返回nullptr，表示槽位为空
            // 实际测试中，InventoryManager会通过其他方法验证物品存在性
            return nullptr;
        }
        
        // 模拟背包获取
        Bag* GetBagByPos(uint8 bag) const
        {
            // 简化实现：返回nullptr表示没有背包
            return nullptr;
        }
        
        // 添加更多必要的Player接口方法以支持InventoryManager
        // 这些方法在实际测试中可能被调用，提供基本的模拟实现
        
        // 模拟物品创建和存储
        InventoryResult CanStoreNewItem(uint8 bag, uint8 slot, ItemPosCountVec& dest, uint32 item, uint32 count) const
        {
            if (bagSpaceInfo_.freeSlots > 0)
            {
                return EQUIP_ERR_OK;
            }
            return EQUIP_ERR_INVENTORY_FULL;
        }
        
        Item* StoreNewItem(const ItemPosCountVec& dest, uint32 item, bool update)
        {
            // 简化实现：假设物品创建成功
            return reinterpret_cast<Item*>(0x1); // 返回非空指针表示成功
        }
        
        // 模拟装备验证
        InventoryResult CanEquipItem(uint8 slot, uint16& dest, Item* pItem, bool swap) const
        {
            return EQUIP_ERR_OK; // 简化：总是允许装备
        }
        
        void EquipItem(uint16 pos, Item* pItem, bool update)
        {
            // 简化实现：不做实际操作
        }
        
        // 模拟物品销毁
        void DestroyItemCount(uint32 item, uint32 count, bool update)
        {
            // 简化实现：从列表中移除物品
            RemoveItem(item);
        }
        
        // 模拟物品存储检查
        InventoryResult CanStoreItem(uint8 bag, uint8 slot, ItemPosCountVec& dest, Item* pItem, bool swap) const
        {
            if (bagSpaceInfo_.freeSlots > 0)
            {
                return EQUIP_ERR_OK;
            }
            return EQUIP_ERR_INVENTORY_FULL;
        }
        
        void StoreItem(const ItemPosCountVec& dest, Item* pItem, bool update)
        {
            // 简化实现：不做实际操作
        }
        
        void RemoveItem(uint8 bag, uint8 slot, bool update)
        {
            // 简化实现：不做实际操作
        }
        
        // 模拟物品交换
        void SwapItem(uint16 src, uint16 dst)
        {
            // 简化实现：不做实际操作
        }
        
    private:
        TestBotData botData_;
        BagSpaceInfo bagSpaceInfo_;
    };
    
    // 创建模拟Player对象
    std::unique_ptr<MockPlayer> CreateMockPlayer(const TestBotData& botData)
    {
        return std::make_unique<MockPlayer>(botData);
    }
    
private:
    void InitializeTestData()
    {
        // 初始化重要物品ID
        importantItemIds_ = {
            6948,  // 炉石
            17031, // 符文布
            17032, // 符文线
            17033, // 符文皮
            17034, // 附魔材料
            17035, // 任务物品1
            17036, // 任务物品2
            17037, // 钥匙1
            17038, // 钥匙2
            17039  // 法术材料
        };
        
        // 初始化普通物品ID
        regularItemIds_ = {
            2589,  // 亚麻布
            2592,  // 毛料
            4306,  // 丝绸
            14047, // 魔纹布
            4338,  // 玛瑙
            1529,  // 翡翠
            3864,  // 星红宝石
            7910,  // 星蓝宝石
            12800, // 奥术水晶
            12655  // 附魔粉尘
        };
        
        // 初始化装备物品ID
        equipmentItemIds_ = {
            1728,  // 皮甲头盔
            2042,  // 皮甲胸甲
            1852,  // 皮甲护腿
            2947,  // 皮甲靴子
            3430,  // 布甲长袍
            4324,  // 布甲帽子
            5739,  // 锁甲胸甲
            6087,  // 锁甲头盔
            7919,  // 板甲胸甲
            8152   // 板甲头盔
        };
    }
    
    void GenerateImportantItems(TestBotData& bot)
    {
        std::uniform_int_distribution<int> countDist(1, 5);
        int importantCount = countDist(rng_);
        
        std::uniform_int_distribution<size_t> itemDist(0, importantItemIds_.size() - 1);
        
        for (int i = 0; i < importantCount; ++i)
        {
            size_t index = itemDist(rng_);
            uint32 itemId = importantItemIds_[index];
            
            // 避免重复
            if (std::find(bot.importantItems.begin(), bot.importantItems.end(), itemId) == bot.importantItems.end())
            {
                bot.importantItems.push_back(itemId);
            }
        }
    }
    
    void GenerateRegularItems(TestBotData& bot)
    {
        uint32 maxRegularItems = bot.totalBagSlots - bot.importantItems.size() - 5; // 留一些空间
        if (maxRegularItems > bot.usedBagSlots) maxRegularItems = bot.usedBagSlots;
        
        std::uniform_int_distribution<uint32> countDist(0, maxRegularItems);
        uint32 regularCount = countDist(rng_);
        
        std::uniform_int_distribution<size_t> itemDist(0, regularItemIds_.size() - 1);
        
        for (uint32 i = 0; i < regularCount; ++i)
        {
            size_t index = itemDist(rng_);
            uint32 itemId = regularItemIds_[index];
            bot.regularItems.push_back(itemId);
        }
    }
    
    void GenerateEquippedItems(TestBotData& bot)
    {
        std::uniform_int_distribution<int> equipCountDist(0, 8);
        int equipCount = equipCountDist(rng_);
        
        std::uniform_int_distribution<uint8> slotDist(EQUIPMENT_SLOT_START, EQUIPMENT_SLOT_END - 1);
        std::uniform_int_distribution<size_t> itemDist(0, equipmentItemIds_.size() - 1);
        
        for (int i = 0; i < equipCount; ++i)
        {
            uint8 slot = slotDist(rng_);
            size_t index = itemDist(rng_);
            uint32 itemId = equipmentItemIds_[index];
            
            bot.equippedItems[slot] = itemId;
        }
    }
    
    TestItemData GenerateImportantItem()
    {
        TestItemData item;
        
        std::uniform_int_distribution<size_t> itemDist(0, importantItemIds_.size() - 1);
        size_t index = itemDist(rng_);
        item.itemId = importantItemIds_[index];
        item.itemName = "Important Item " + std::to_string(item.itemId);
        item.isImportant = true;
        item.priority = 100; // 高优先级
        
        // 根据物品ID确定重要物品类型
        if (item.itemId == 6948)
        {
            item.importantType = IMPORTANT_ITEM_HEARTHSTONE;
        }
        else if (item.itemId >= 17035 && item.itemId <= 17036)
        {
            item.importantType = IMPORTANT_ITEM_QUEST;
        }
        else if (item.itemId >= 17037 && item.itemId <= 17038)
        {
            item.importantType = IMPORTANT_ITEM_KEY;
        }
        else if (item.itemId == 17039)
        {
            item.importantType = IMPORTANT_ITEM_REAGENT;
        }
        else
        {
            item.importantType = IMPORTANT_ITEM_SPECIAL;
        }
        
        return item;
    }
    
    TestItemData GenerateRegularItem()
    {
        TestItemData item;
        
        std::uniform_int_distribution<size_t> itemDist(0, regularItemIds_.size() - 1);
        size_t index = itemDist(rng_);
        item.itemId = regularItemIds_[index];
        item.itemName = "Regular Item " + std::to_string(item.itemId);
        item.isImportant = false;
        
        std::uniform_int_distribution<uint32> priorityDist(10, 60);
        item.priority = priorityDist(rng_);
        
        return item;
    }
    
protected:
    std::mt19937 rng_;
    std::unique_ptr<InventoryManager> inventoryManager_;
    std::unordered_map<uint32, TestItemData> testItemTemplates_;
    
    // 测试数据
    std::vector<uint32> importantItemIds_;
    std::vector<uint32> regularItemIds_;
    std::vector<uint32> equipmentItemIds_;
    
    // 常量定义
    static constexpr int PROPERTY_TEST_ITERATIONS = 100;
    static constexpr uint32 IMPORTANT_ITEM_PRIORITY = 100;
    static constexpr uint32 REGULAR_ITEM_PRIORITY = 40;
    static constexpr uint32 TRASH_ITEM_PRIORITY = 10;
};

/**
 * **功能: pvp-gear-auto-equip, 属性 6: 物品保护机制**
 * **验证需求: 3.1, 3.2, 3.3**
 * 
 * 属性：对于任何装备操作，系统应该保留背包中的所有重要物品（炉石、任务物品等），不被移除或替换
 */
TEST_F(InventoryManagerPropertyTest, ImportantItemProtectionMechanism)
{
    for (int iteration = 0; iteration < PROPERTY_TEST_ITERATIONS; ++iteration)
    {
        // 生成随机测试数据
        TestBotData botData = GenerateRandomBot();
        auto mockPlayer = CreateMockPlayer(botData);
        
        ASSERT_NE(mockPlayer.get(), nullptr) << "Iteration " << iteration << ": Mock player should not be null";
        
        // 记录初始的重要物品
        std::vector<uint32> initialImportantItems = mockPlayer->GetImportantItems();
        
        // 执行物品保护操作
        bool protectionResult = inventoryManager_->ProtectImportantItems(reinterpret_cast<Player*>(mockPlayer.get()));
        
        // 属性验证：保护操作应该成功
        EXPECT_TRUE(protectionResult) << "Iteration " << iteration 
            << ": Important item protection should succeed";
        
        // 属性验证：所有重要物品应该仍然存在
        for (uint32 importantItemId : initialImportantItems)
        {
            bool stillExists = mockPlayer->HasItem(importantItemId);
            EXPECT_TRUE(stillExists) << "Iteration " << iteration 
                << ": Important item " << importantItemId << " should still exist after protection";
        }
        
        // 属性验证：重要物品的优先级应该是最高的
        for (uint32 importantItemId : initialImportantItems)
        {
            uint32 priority = inventoryManager_->GetItemPriority(importantItemId);
            EXPECT_GE(priority, IMPORTANT_ITEM_PRIORITY) << "Iteration " << iteration 
                << ": Important item " << importantItemId << " should have high priority (" << priority << ")";
        }
        
        // 属性验证：重要物品应该被正确识别
        for (uint32 importantItemId : initialImportantItems)
        {
            ImportantItemType type;
            bool isImportant = inventoryManager_->IsImportantItem(importantItemId, &type);
            EXPECT_TRUE(isImportant) << "Iteration " << iteration 
                << ": Item " << importantItemId << " should be recognized as important";
            
            // 验证重要物品类型的合理性
            EXPECT_LT(type, IMPORTANT_ITEM_MAX) << "Iteration " << iteration 
                << ": Important item type should be valid for item " << importantItemId;
                
            // 属性验证：重要物品不应该被意外移除
            bool canRemove = inventoryManager_->RemoveItemFromBag(
                reinterpret_cast<Player*>(mockPlayer.get()), importantItemId, 1);
            EXPECT_FALSE(canRemove) << "Iteration " << iteration 
                << ": Important item " << importantItemId << " should not be removable";
        }
        
        // 属性验证：炉石应该被特别保护
        if (std::find(initialImportantItems.begin(), initialImportantItems.end(), 6948) != initialImportantItems.end())
        {
            ImportantItemType hearthstoneType;
            bool isHearthstone = inventoryManager_->IsImportantItem(6948, &hearthstoneType);
            EXPECT_TRUE(isHearthstone) << "Iteration " << iteration 
                << ": Hearthstone should be recognized as important";
            EXPECT_EQ(hearthstoneType, IMPORTANT_ITEM_HEARTHSTONE) << "Iteration " << iteration 
                << ": Hearthstone should have correct type";
        }
        
        // 属性验证：任务物品应该被保护
        for (uint32 itemId = 17035; itemId <= 17036; ++itemId)
        {
            if (std::find(initialImportantItems.begin(), initialImportantItems.end(), itemId) != initialImportantItems.end())
            {
                ImportantItemType questType;
                bool isQuest = inventoryManager_->IsImportantItem(itemId, &questType);
                EXPECT_TRUE(isQuest) << "Iteration " << iteration 
                    << ": Quest item " << itemId << " should be recognized as important";
                EXPECT_EQ(questType, IMPORTANT_ITEM_QUEST) << "Iteration " << iteration 
                    << ": Quest item should have correct type";
            }
        }
        
        // 属性验证：钥匙物品应该被保护
        for (uint32 itemId = 17037; itemId <= 17038; ++itemId)
        {
            if (std::find(initialImportantItems.begin(), initialImportantItems.end(), itemId) != initialImportantItems.end())
            {
                ImportantItemType keyType;
                bool isKey = inventoryManager_->IsImportantItem(itemId, &keyType);
                EXPECT_TRUE(isKey) << "Iteration " << iteration 
                    << ": Key item " << itemId << " should be recognized as important";
                EXPECT_EQ(keyType, IMPORTANT_ITEM_KEY) << "Iteration " << iteration 
                    << ": Key item should have correct type";
            }
        }
        
        // 属性验证：法术材料应该被保护
        if (std::find(initialImportantItems.begin(), initialImportantItems.end(), 17039) != initialImportantItems.end())
        {
            ImportantItemType reagentType;
            bool isReagent = inventoryManager_->IsImportantItem(17039, &reagentType);
            EXPECT_TRUE(isReagent) << "Iteration " << iteration 
                << ": Reagent item should be recognized as important";
            EXPECT_EQ(reagentType, IMPORTANT_ITEM_REAGENT) << "Iteration " << iteration 
                << ": Reagent item should have correct type";
        }
    }
}

/**
 * **功能: pvp-gear-auto-equip, 属性 7: 背包空间管理**
 * **验证需求: 3.4**
 * 
 * 属性：对于任何背包空间不足的情况，系统应该优先保留重要物品而非装备新的PVP装备
 */
TEST_F(InventoryManagerPropertyTest, BagSpaceManagementPriority)
{
    for (int iteration = 0; iteration < PROPERTY_TEST_ITERATIONS; ++iteration)
    {
        // 生成随机测试数据，确保背包空间紧张
        TestBotData botData = GenerateRandomBot();
        
        // 模拟背包空间不足的情况
        botData.usedBagSlots = botData.totalBagSlots - 2; // 只剩2个空位
        
        auto mockPlayer = CreateMockPlayer(botData);
        ASSERT_NE(mockPlayer.get(), nullptr) << "Iteration " << iteration << ": Mock player should not be null";
        
        // 获取初始背包空间信息
        BagSpaceInfo initialSpaceInfo = inventoryManager_->GetBagSpaceInfo(reinterpret_cast<Player*>(mockPlayer.get()));
        
        // 属性验证：背包空间信息应该准确
        EXPECT_EQ(initialSpaceInfo.totalSlots, botData.totalBagSlots) << "Iteration " << iteration 
            << ": Total bag slots should match";
        EXPECT_EQ(initialSpaceInfo.usedSlots, botData.usedBagSlots) << "Iteration " << iteration 
            << ": Used bag slots should match";
        EXPECT_EQ(initialSpaceInfo.freeSlots, botData.totalBagSlots - botData.usedBagSlots) << "Iteration " << iteration 
            << ": Free bag slots should be calculated correctly";
        
        // 记录初始的重要物品
        std::vector<uint32> initialImportantItems = mockPlayer->GetImportantItems();
        
        // 测试背包空间检查
        uint32 requiredSlots = 5; // 需要5个槽位
        bool hasSufficientSpace = inventoryManager_->HasSufficientBagSpace(
            reinterpret_cast<Player*>(mockPlayer.get()), requiredSlots);
        
        // 属性验证：空间不足时应该返回false
        if (initialSpaceInfo.freeSlots < requiredSlots)
        {
            EXPECT_FALSE(hasSufficientSpace) << "Iteration " << iteration 
                << ": Should detect insufficient bag space (free: " << initialSpaceInfo.freeSlots 
                << ", required: " << requiredSlots << ")";
        }
        else
        {
            EXPECT_TRUE(hasSufficientSpace) << "Iteration " << iteration 
                << ": Should detect sufficient bag space (free: " << initialSpaceInfo.freeSlots 
                << ", required: " << requiredSlots << ")";
        }
        
        // 测试背包空间优化
        bool optimizationResult = inventoryManager_->OptimizeBagSpace(
            reinterpret_cast<Player*>(mockPlayer.get()), requiredSlots);
        
        if (!hasSufficientSpace)
        {
            // 属性验证：优化后应该有足够空间或保护了重要物品
            BagSpaceInfo optimizedSpaceInfo = inventoryManager_->GetBagSpaceInfo(
                reinterpret_cast<Player*>(mockPlayer.get()));
            
            if (optimizationResult)
            {
                EXPECT_GE(optimizedSpaceInfo.freeSlots, requiredSlots) << "Iteration " << iteration 
                    << ": Optimization should provide sufficient space";
            }
            
            // 属性验证：重要物品应该被保留
            for (uint32 importantItemId : initialImportantItems)
            {
                bool stillExists = mockPlayer->HasItem(importantItemId);
                EXPECT_TRUE(stillExists) << "Iteration " << iteration 
                    << ": Important item " << importantItemId << " should be preserved during optimization";
            }
        }
        
        // 属性验证：背包使用率计算应该正确
        float usageRatio = initialSpaceInfo.GetUsageRatio();
        float expectedRatio = static_cast<float>(initialSpaceInfo.usedSlots) / initialSpaceInfo.totalSlots;
        EXPECT_FLOAT_EQ(usageRatio, expectedRatio) << "Iteration " << iteration 
            << ": Bag usage ratio should be calculated correctly";
        
        // 属性验证：HasSpace方法应该正确工作
        bool hasSpace1 = initialSpaceInfo.HasSpace(1);
        bool hasSpace5 = initialSpaceInfo.HasSpace(5);
        bool hasSpaceAll = initialSpaceInfo.HasSpace(initialSpaceInfo.totalSlots);
        
        EXPECT_EQ(hasSpace1, initialSpaceInfo.freeSlots >= 1) << "Iteration " << iteration 
            << ": HasSpace(1) should work correctly";
        EXPECT_EQ(hasSpace5, initialSpaceInfo.freeSlots >= 5) << "Iteration " << iteration 
            << ": HasSpace(5) should work correctly";
        EXPECT_FALSE(hasSpaceAll) << "Iteration " << iteration 
            << ": HasSpace(totalSlots) should be false when bag is not empty";
        
        // 属性验证：优先级系统应该正确工作
        for (uint32 importantItemId : initialImportantItems)
        {
            uint32 importantPriority = inventoryManager_->GetItemPriority(importantItemId);
            
            // 检查普通物品的优先级应该更低
            for (uint32 regularItemId : mockPlayer->GetRegularItems())
            {
                uint32 regularPriority = inventoryManager_->GetItemPriority(regularItemId);
                EXPECT_GT(importantPriority, regularPriority) << "Iteration " << iteration 
                    << ": Important item " << importantItemId << " (priority: " << importantPriority 
                    << ") should have higher priority than regular item " << regularItemId 
                    << " (priority: " << regularPriority << ")";
            }
        }
        
        // 属性验证：边界条件 - 完全满的背包
        if (initialSpaceInfo.freeSlots == 0)
        {
            bool hasSpaceForOne = inventoryManager_->HasSufficientBagSpace(
                reinterpret_cast<Player*>(mockPlayer.get()), 1);
            EXPECT_FALSE(hasSpaceForOne) << "Iteration " << iteration 
                << ": Full bag should not have space for any items";
                
            // 优化应该通过移除低优先级物品来创建空间
            bool canOptimize = inventoryManager_->OptimizeBagSpace(
                reinterpret_cast<Player*>(mockPlayer.get()), 1);
            if (canOptimize)
            {
                BagSpaceInfo afterOptimization = inventoryManager_->GetBagSpaceInfo(
                    reinterpret_cast<Player*>(mockPlayer.get()));
                EXPECT_GT(afterOptimization.freeSlots, 0u) << "Iteration " << iteration 
                    << ": Optimization should create at least one free slot";
            }
        }
        
        // 属性验证：边界条件 - 空背包
        if (initialSpaceInfo.usedSlots == 0)
        {
            bool hasSpaceForAll = inventoryManager_->HasSufficientBagSpace(
                reinterpret_cast<Player*>(mockPlayer.get()), initialSpaceInfo.totalSlots);
            EXPECT_TRUE(hasSpaceForAll) << "Iteration " << iteration 
                << ": Empty bag should have space for all slots";
        }
    }
}

/**
 * **功能: pvp-gear-auto-equip, 属性 8: 装备替换存储**
 * **验证需求: 3.5**
 * 
 * 属性：对于任何装备更换操作，被替换的装备应该正确存储到背包中
 */
TEST_F(InventoryManagerPropertyTest, EquipmentReplacementStorage)
{
    for (int iteration = 0; iteration < PROPERTY_TEST_ITERATIONS; ++iteration)
    {
        // 生成随机测试数据
        TestBotData botData = GenerateRandomBot();
        
        // 确保有一些装备和背包空间
        botData.totalBagSlots = 32;
        botData.usedBagSlots = 20; // 留一些空间
        
        auto mockPlayer = CreateMockPlayer(botData);
        ASSERT_NE(mockPlayer.get(), nullptr) << "Iteration " << iteration << ": Mock player should not be null";
        
        // 保存当前装备配置
        EquipmentSnapshot snapshot;
        bool saveResult = inventoryManager_->SaveCurrentEquipment(
            reinterpret_cast<Player*>(mockPlayer.get()), snapshot);
        
        // 属性验证：保存操作应该成功
        EXPECT_TRUE(saveResult) << "Iteration " << iteration 
            << ": Equipment snapshot save should succeed";
        
        // 属性验证：快照应该有效
        EXPECT_TRUE(snapshot.isValid) << "Iteration " << iteration 
            << ": Equipment snapshot should be valid";
        
        // 属性验证：快照应该包含当前装备
        const auto& equippedItems = mockPlayer->GetEquippedItems();
        EXPECT_EQ(snapshot.GetEquipmentCount(), equippedItems.size()) << "Iteration " << iteration 
            << ": Snapshot should contain all equipped items";
        
        for (const auto& [slot, itemId] : equippedItems)
        {
            auto it = snapshot.equippedItems.find(slot);
            EXPECT_NE(it, snapshot.equippedItems.end()) << "Iteration " << iteration 
                << ": Snapshot should contain item in slot " << (int)slot;
            
            if (it != snapshot.equippedItems.end())
            {
                EXPECT_EQ(it->second, itemId) << "Iteration " << iteration 
                    << ": Snapshot should have correct item ID for slot " << (int)slot;
            }
        }
        
        // 属性验证：快照时间戳应该合理
        uint64 currentTime = time(nullptr);
        EXPECT_LE(snapshot.timestamp, currentTime) << "Iteration " << iteration 
            << ": Snapshot timestamp should not be in the future";
        EXPECT_GT(snapshot.timestamp, currentTime - 60) << "Iteration " << iteration 
            << ": Snapshot timestamp should be recent";
        
        // 模拟装备更换操作
        std::uniform_int_distribution<uint8> slotDist(EQUIPMENT_SLOT_START, EQUIPMENT_SLOT_END - 1);
        uint8 testSlot = slotDist(rng_);
        
        std::uniform_int_distribution<size_t> itemDist(0, equipmentItemIds_.size() - 1);
        uint32 newItemId = equipmentItemIds_[itemDist(rng_)];
        
        // 记录原装备
        uint32 originalItemId = mockPlayer->GetEquippedItemId(testSlot);
        
        // 执行装备操作
        bool equipResult = inventoryManager_->EquipItem(
            reinterpret_cast<Player*>(mockPlayer.get()), newItemId, testSlot);
        
        if (equipResult)
        {
            // 属性验证：新装备应该被装备
            uint32 currentItemId = mockPlayer->GetEquippedItemId(testSlot);
            EXPECT_EQ(currentItemId, newItemId) << "Iteration " << iteration 
                << ": New item should be equipped in slot " << (int)testSlot;
            
            // 属性验证：如果有原装备，应该被存储到背包
            if (originalItemId != 0)
            {
                bool originalInBag = mockPlayer->HasItem(originalItemId);
                EXPECT_TRUE(originalInBag) << "Iteration " << iteration 
                    << ": Original item " << originalItemId << " should be stored in bag after replacement";
            }
        }
        
        // 测试装备恢复
        bool restoreResult = inventoryManager_->RestoreEquipment(
            reinterpret_cast<Player*>(mockPlayer.get()), snapshot);
        
        if (restoreResult)
        {
            // 属性验证：装备应该被恢复到快照状态
            for (const auto& [slot, itemId] : snapshot.equippedItems)
            {
                uint32 restoredItemId = mockPlayer->GetEquippedItemId(slot);
                EXPECT_EQ(restoredItemId, itemId) << "Iteration " << iteration 
                    << ": Item in slot " << (int)slot << " should be restored to " << itemId;
            }
            
            // 属性验证：恢复后的装备数量应该与快照一致
            uint32 restoredCount = 0;
            for (const auto& [slot, itemId] : snapshot.equippedItems)
            {
                if (mockPlayer->GetEquippedItemId(slot) == itemId)
                {
                    restoredCount++;
                }
            }
            EXPECT_EQ(restoredCount, snapshot.GetEquipmentCount()) << "Iteration " << iteration 
                << ": All equipment should be restored correctly";
        }
        
        // 属性验证：多次保存和恢复应该保持一致性
        EquipmentSnapshot secondSnapshot;
        bool secondSaveResult = inventoryManager_->SaveCurrentEquipment(
            reinterpret_cast<Player*>(mockPlayer.get()), secondSnapshot);
        
        if (secondSaveResult && restoreResult)
        {
            // 如果恢复成功，第二次快照应该与第一次快照相同
            EXPECT_EQ(secondSnapshot.GetEquipmentCount(), snapshot.GetEquipmentCount()) 
                << "Iteration " << iteration << ": Snapshot consistency should be maintained";
                
            for (const auto& [slot, itemId] : snapshot.equippedItems)
            {
                auto it = secondSnapshot.equippedItems.find(slot);
                if (it != secondSnapshot.equippedItems.end())
                {
                    EXPECT_EQ(it->second, itemId) << "Iteration " << iteration 
                        << ": Equipment in slot " << (int)slot << " should be consistent across snapshots";
                }
            }
        }
        
        // 属性验证：空快照应该被正确处理
        EquipmentSnapshot emptySnapshot;
        bool emptyRestoreResult = inventoryManager_->RestoreEquipment(
            reinterpret_cast<Player*>(mockPlayer.get()), emptySnapshot);
        EXPECT_FALSE(emptyRestoreResult) << "Iteration " << iteration 
            << ": Empty snapshot restore should fail";
        
        // 属性验证：快照方法应该正确工作
        EXPECT_FALSE(emptySnapshot.isValid) << "Iteration " << iteration 
            << ": Empty snapshot should be invalid";
        EXPECT_TRUE(emptySnapshot.IsEmpty()) << "Iteration " << iteration 
            << ": Empty snapshot should report as empty";
        EXPECT_EQ(emptySnapshot.GetEquipmentCount(), 0u) << "Iteration " << iteration 
            << ": Empty snapshot should have zero equipment count";
        
        // 测试快照重置
        snapshot.Reset();
        EXPECT_FALSE(snapshot.isValid) << "Iteration " << iteration 
            << ": Reset snapshot should be invalid";
        EXPECT_TRUE(snapshot.IsEmpty()) << "Iteration " << iteration 
            << ": Reset snapshot should be empty";
        EXPECT_EQ(snapshot.GetEquipmentCount(), 0u) << "Iteration " << iteration 
            << ": Reset snapshot should have zero equipment count";
    }
}

/**
 * 装备操作验证测试
 * 验证装备操作的安全性和正确性
 */
TEST_F(InventoryManagerPropertyTest, EquipmentOperationValidation)
{
    for (int iteration = 0; iteration < PROPERTY_TEST_ITERATIONS; ++iteration)
    {
        // 生成随机测试数据
        TestBotData botData = GenerateRandomBot();
        auto mockPlayer = CreateMockPlayer(botData);
        
        ASSERT_NE(mockPlayer.get(), nullptr) << "Iteration " << iteration << ": Mock player should not be null";
        
        // 测试装备操作验证
        std::uniform_int_distribution<uint8> slotDist(EQUIPMENT_SLOT_START, EQUIPMENT_SLOT_END - 1);
        std::uniform_int_distribution<size_t> itemDist(0, equipmentItemIds_.size() - 1);
        
        uint8 testSlot = slotDist(rng_);
        uint32 testItemId = equipmentItemIds_[itemDist(rng_)];
        
        // 属性验证：装备操作验证应该是确定性的
        bool validation1 = inventoryManager_->ValidateEquipOperation(
            reinterpret_cast<Player*>(mockPlayer.get()), testItemId, testSlot);
        bool validation2 = inventoryManager_->ValidateEquipOperation(
            reinterpret_cast<Player*>(mockPlayer.get()), testItemId, testSlot);
        
        EXPECT_EQ(validation1, validation2) << "Iteration " << iteration 
            << ": Equipment operation validation should be deterministic";
        
        // 属性验证：无效槽位应该被拒绝
        bool invalidSlotValidation = inventoryManager_->ValidateEquipOperation(
            reinterpret_cast<Player*>(mockPlayer.get()), testItemId, EQUIPMENT_SLOT_END);
        EXPECT_FALSE(invalidSlotValidation) << "Iteration " << iteration 
            << ": Invalid slot should be rejected";
        
        // 属性验证：空指针应该被正确处理
        bool nullPlayerValidation = inventoryManager_->ValidateEquipOperation(
            nullptr, testItemId, testSlot);
        EXPECT_FALSE(nullPlayerValidation) << "Iteration " << iteration 
            << ": Null player should be rejected";
        
        // 测试装备槽位兼容性
        bool canEquip = inventoryManager_->CanEquipItemToSlot(
            reinterpret_cast<Player*>(mockPlayer.get()), testItemId, testSlot);
        
        // 属性验证：兼容性检查应该是确定性的
        bool canEquip2 = inventoryManager_->CanEquipItemToSlot(
            reinterpret_cast<Player*>(mockPlayer.get()), testItemId, testSlot);
        EXPECT_EQ(canEquip, canEquip2) << "Iteration " << iteration 
            << ": Equipment slot compatibility should be deterministic";
        
        // 属性验证：空指针应该返回false
        bool nullCanEquip = inventoryManager_->CanEquipItemToSlot(nullptr, testItemId, testSlot);
        EXPECT_FALSE(nullCanEquip) << "Iteration " << iteration 
            << ": Null player should not be able to equip items";
        
        // 属性验证：无效槽位应该返回false
        bool invalidSlotCanEquip = inventoryManager_->CanEquipItemToSlot(
            reinterpret_cast<Player*>(mockPlayer.get()), testItemId, EQUIPMENT_SLOT_END);
        EXPECT_FALSE(invalidSlotCanEquip) << "Iteration " << iteration 
            << ": Invalid slot should not allow equipment";
    }
}

/**
 * **功能: pvp-gear-auto-equip, 综合属性测试: 物品保护、空间管理与装备存储的协同工作**
 * **验证需求: 3.1, 3.2, 3.3, 3.4, 3.5**
 * 
 * 综合属性：在任何复杂的装备操作场景中，系统应该同时满足物品保护、空间管理和装备存储的所有要求
 */
TEST_F(InventoryManagerPropertyTest, ComprehensiveInventoryManagementProperty)
{
    for (int iteration = 0; iteration < PROPERTY_TEST_ITERATIONS; ++iteration)
    {
        // 生成复杂的测试场景
        TestBotData botData = GenerateRandomBot();
        
        // 创建紧张的背包空间情况
        botData.totalBagSlots = 20;
        botData.usedBagSlots = 18; // 只剩2个空位
        
        auto mockPlayer = CreateMockPlayer(botData);
        ASSERT_NE(mockPlayer.get(), nullptr) << "Iteration " << iteration << ": Mock player should not be null";
        
        // 记录初始状态
        std::vector<uint32> initialImportantItems = mockPlayer->GetImportantItems();
        std::vector<uint32> initialRegularItems = mockPlayer->GetRegularItems();
        BagSpaceInfo initialSpaceInfo = inventoryManager_->GetBagSpaceInfo(
            reinterpret_cast<Player*>(mockPlayer.get()));
        
        // 保存初始装备快照
        EquipmentSnapshot initialSnapshot;
        bool snapshotSaved = inventoryManager_->SaveCurrentEquipment(
            reinterpret_cast<Player*>(mockPlayer.get()), initialSnapshot);
        
        // 综合属性验证1：物品保护在空间紧张时仍然有效
        bool protectionResult = inventoryManager_->ProtectImportantItems(
            reinterpret_cast<Player*>(mockPlayer.get()));
        EXPECT_TRUE(protectionResult) << "Iteration " << iteration 
            << ": Important item protection should work even with limited space";
        
        // 验证重要物品仍然存在
        for (uint32 importantItemId : initialImportantItems)
        {
            bool stillExists = mockPlayer->HasItem(importantItemId);
            EXPECT_TRUE(stillExists) << "Iteration " << iteration 
                << ": Important item " << importantItemId << " should be protected";
        }
        
        // 综合属性验证2：空间优化不应该影响重要物品
        uint32 requiredSlots = 3;
        bool optimizationResult = inventoryManager_->OptimizeBagSpace(
            reinterpret_cast<Player*>(mockPlayer.get()), requiredSlots);
        
        // 验证重要物品在优化后仍然存在
        for (uint32 importantItemId : initialImportantItems)
        {
            bool stillExists = mockPlayer->HasItem(importantItemId);
            EXPECT_TRUE(stillExists) << "Iteration " << iteration 
                << ": Important item " << importantItemId << " should survive space optimization";
        }
        
        // 综合属性验证3：装备操作应该考虑空间限制和物品保护
        std::uniform_int_distribution<uint8> slotDist(EQUIPMENT_SLOT_START, EQUIPMENT_SLOT_END - 1);
        std::uniform_int_distribution<size_t> itemDist(0, equipmentItemIds_.size() - 1);
        
        uint8 testSlot = slotDist(rng_);
        uint32 newItemId = equipmentItemIds_[itemDist(rng_)];
        
        // 记录原装备
        uint32 originalItemId = mockPlayer->GetEquippedItemId(testSlot);
        
        // 尝试装备新物品
        bool equipResult = inventoryManager_->EquipItem(
            reinterpret_cast<Player*>(mockPlayer.get()), newItemId, testSlot);
        
        if (equipResult)
        {
            // 验证新装备已装备
            uint32 currentItemId = mockPlayer->GetEquippedItemId(testSlot);
            EXPECT_EQ(currentItemId, newItemId) << "Iteration " << iteration 
                << ": New equipment should be equipped";
            
            // 验证原装备被正确存储（如果有的话）
            if (originalItemId != 0)
            {
                bool originalInBag = mockPlayer->HasItem(originalItemId);
                EXPECT_TRUE(originalInBag) << "Iteration " << iteration 
                    << ": Original equipment should be stored in bag";
            }
            
            // 验证重要物品仍然受保护
            for (uint32 importantItemId : initialImportantItems)
            {
                bool stillExists = mockPlayer->HasItem(importantItemId);
                EXPECT_TRUE(stillExists) << "Iteration " << iteration 
                    << ": Important item " << importantItemId << " should remain protected after equipment change";
            }
        }
        
        // 综合属性验证4：装备恢复应该在空间限制下正常工作
        if (snapshotSaved && initialSnapshot.isValid)
        {
            bool restoreResult = inventoryManager_->RestoreEquipment(
                reinterpret_cast<Player*>(mockPlayer.get()), initialSnapshot);
            
            if (restoreResult)
            {
                // 验证装备已恢复
                for (const auto& [slot, itemId] : initialSnapshot.equippedItems)
                {
                    uint32 restoredItemId = mockPlayer->GetEquippedItemId(slot);
                    EXPECT_EQ(restoredItemId, itemId) << "Iteration " << iteration 
                        << ": Equipment should be restored correctly";
                }
                
                // 验证重要物品在恢复过程中仍然受保护
                for (uint32 importantItemId : initialImportantItems)
                {
                    bool stillExists = mockPlayer->HasItem(importantItemId);
                    EXPECT_TRUE(stillExists) << "Iteration " << iteration 
                        << ": Important item " << importantItemId << " should remain protected during restoration";
                }
            }
        }
        
        // 综合属性验证5：系统状态的一致性
        BagSpaceInfo finalSpaceInfo = inventoryManager_->GetBagSpaceInfo(
            reinterpret_cast<Player*>(mockPlayer.get()));
        
        // 背包总容量不应该改变
        EXPECT_EQ(finalSpaceInfo.totalSlots, initialSpaceInfo.totalSlots) << "Iteration " << iteration 
            << ": Total bag capacity should remain constant";
        
        // 使用率计算应该正确
        float finalUsageRatio = finalSpaceInfo.GetUsageRatio();
        EXPECT_GE(finalUsageRatio, 0.0f) << "Iteration " << iteration 
            << ": Usage ratio should be non-negative";
        EXPECT_LE(finalUsageRatio, 1.0f) << "Iteration " << iteration 
            << ": Usage ratio should not exceed 100%";
        
        // 综合属性验证6：错误处理的鲁棒性
        // 测试空指针处理
        bool nullProtection = inventoryManager_->ProtectImportantItems(nullptr);
        EXPECT_FALSE(nullProtection) << "Iteration " << iteration 
            << ": Null player should be handled gracefully";
        
        bool nullSpaceCheck = inventoryManager_->HasSufficientBagSpace(nullptr, 1);
        EXPECT_FALSE(nullSpaceCheck) << "Iteration " << iteration 
            << ": Null player space check should return false";
        
        // 测试无效参数处理
        bool zeroSlotCheck = inventoryManager_->HasSufficientBagSpace(
            reinterpret_cast<Player*>(mockPlayer.get()), 0);
        EXPECT_TRUE(zeroSlotCheck) << "Iteration " << iteration 
            << ": Zero slot requirement should always be satisfied";
        
        bool excessiveSlotCheck = inventoryManager_->HasSufficientBagSpace(
            reinterpret_cast<Player*>(mockPlayer.get()), 1000);
        EXPECT_FALSE(excessiveSlotCheck) << "Iteration " << iteration 
            << ": Excessive slot requirement should be rejected";
    }
}

/**
 * 边界条件和异常处理测试
 * 验证系统在极端情况下的鲁棒性
 */
TEST_F(InventoryManagerPropertyTest, BoundaryConditionsAndExceptionHandling)
{
    for (int iteration = 0; iteration < PROPERTY_TEST_ITERATIONS; ++iteration)
    {
        // 测试边界条件：完全空的背包
        TestBotData emptyBotData = GenerateRandomBot();
        emptyBotData.totalBagSlots = 16;
        emptyBotData.usedBagSlots = 0;
        emptyBotData.importantItems.clear();
        emptyBotData.regularItems.clear();
        emptyBotData.equippedItems.clear();
        
        auto emptyMockPlayer = CreateMockPlayer(emptyBotData);
        
        // 属性验证：空背包应该有最大可用空间
        BagSpaceInfo emptySpaceInfo = inventoryManager_->GetBagSpaceInfo(
            reinterpret_cast<Player*>(emptyMockPlayer.get()));
        EXPECT_EQ(emptySpaceInfo.freeSlots, emptySpaceInfo.totalSlots) << "Iteration " << iteration 
            << ": Empty bag should have all slots free";
        EXPECT_EQ(emptySpaceInfo.usedSlots, 0u) << "Iteration " << iteration 
            << ": Empty bag should have no used slots";
        EXPECT_FLOAT_EQ(emptySpaceInfo.GetUsageRatio(), 0.0f) << "Iteration " << iteration 
            << ": Empty bag should have 0% usage ratio";
        
        // 测试边界条件：完全满的背包
        TestBotData fullBotData = GenerateRandomBot();
        fullBotData.totalBagSlots = 16;
        fullBotData.usedBagSlots = 16;
        
        auto fullMockPlayer = CreateMockPlayer(fullBotData);
        
        // 属性验证：满背包应该没有可用空间
        BagSpaceInfo fullSpaceInfo = inventoryManager_->GetBagSpaceInfo(
            reinterpret_cast<Player*>(fullMockPlayer.get()));
        EXPECT_EQ(fullSpaceInfo.freeSlots, 0u) << "Iteration " << iteration 
            << ": Full bag should have no free slots";
        EXPECT_EQ(fullSpaceInfo.usedSlots, fullSpaceInfo.totalSlots) << "Iteration " << iteration 
            << ": Full bag should have all slots used";
        EXPECT_FLOAT_EQ(fullSpaceInfo.GetUsageRatio(), 1.0f) << "Iteration " << iteration 
            << ": Full bag should have 100% usage ratio";
        
        // 测试异常处理：无效物品ID
        uint32 invalidItemId = 0;
        bool invalidItemProtection = inventoryManager_->IsImportantItem(invalidItemId);
        EXPECT_FALSE(invalidItemProtection) << "Iteration " << iteration 
            << ": Invalid item ID should not be considered important";
        
        uint32 invalidItemPriority = inventoryManager_->GetItemPriority(invalidItemId);
        EXPECT_GT(invalidItemPriority, 0u) << "Iteration " << iteration 
            << ": Invalid item should have some default priority";
        
        // 测试异常处理：极大的物品ID
        uint32 extremeItemId = UINT32_MAX;
        bool extremeItemProtection = inventoryManager_->IsImportantItem(extremeItemId);
        // 不应该崩溃，返回值可以是任何合理的布尔值
        
        // 测试异常处理：无效装备槽位
        uint8 invalidSlot = EQUIPMENT_SLOT_END + 10;
        bool invalidSlotValidation = inventoryManager_->ValidateEquipOperation(
            reinterpret_cast<Player*>(emptyMockPlayer.get()), 1000, invalidSlot);
        EXPECT_FALSE(invalidSlotValidation) << "Iteration " << iteration 
            << ": Invalid equipment slot should be rejected";
        
        // 测试快照的边界条件
        EquipmentSnapshot boundarySnapshot;
        
        // 空快照的属性
        EXPECT_FALSE(boundarySnapshot.isValid) << "Iteration " << iteration 
            << ": Default snapshot should be invalid";
        EXPECT_TRUE(boundarySnapshot.IsEmpty()) << "Iteration " << iteration 
            << ": Default snapshot should be empty";
        EXPECT_EQ(boundarySnapshot.GetEquipmentCount(), 0u) << "Iteration " << iteration 
            << ": Default snapshot should have zero equipment count";
        
        // 测试快照时间戳边界
        boundarySnapshot.timestamp = 0;
        boundarySnapshot.isValid = true;
        uint64 currentTime = time(nullptr);
        EXPECT_LT(boundarySnapshot.timestamp, currentTime) << "Iteration " << iteration 
            << ": Zero timestamp should be in the past";
        
        // 测试快照重置后的状态
        boundarySnapshot.equippedItems[EQUIPMENT_SLOT_HEAD] = 1000;
        boundarySnapshot.Reset();
        EXPECT_FALSE(boundarySnapshot.isValid) << "Iteration " << iteration 
            << ": Reset snapshot should be invalid";
        EXPECT_TRUE(boundarySnapshot.IsEmpty()) << "Iteration " << iteration 
            << ": Reset snapshot should be empty";
        EXPECT_EQ(boundarySnapshot.timestamp, 0u) << "Iteration " << iteration 
            << ": Reset snapshot should have zero timestamp";
    }
}
        bool validation1 = inventoryManager_->ValidateEquipOperation(
            reinterpret_cast<Player*>(mockPlayer.get()), testItemId, testSlot);
        bool validation2 = inventoryManager_->ValidateEquipOperation(
            reinterpret_cast<Player*>(mockPlayer.get()), testItemId, testSlot);
        
        EXPECT_EQ(validation1, validation2) << "Iteration " << iteration 
            << ": Equipment operation validation should be deterministic";
        
        // 属性验证：无效槽位应该被拒绝
        bool invalidSlotValidation = inventoryManager_->ValidateEquipOperation(
            reinterpret_cast<Player*>(mockPlayer.get()), testItemId, EQUIPMENT_SLOT_END);
        EXPECT_FALSE(invalidSlotValidation) << "Iteration " << iteration 
            << ": Invalid slot should be rejected";
        
        // 属性验证：空指针应该被正确处理
        bool nullPlayerValidation = inventoryManager_->ValidateEquipOperation(
            nullptr, testItemId, testSlot);
        EXPECT_FALSE(nullPlayerValidation) << "Iteration " << iteration 
            << ": Null player should be rejected";
        
        // 测试装备槽位兼容性
        bool canEquip = inventoryManager_->CanEquipItemToSlot(
            reinterpret_cast<Player*>(mockPlayer.get()), testItemId, testSlot);
        
        // 属性验证：兼容性检查应该是确定性的
        bool canEquip2 = inventoryManager_->CanEquipItemToSlot(
            reinterpret_cast<Player*>(mockPlayer.get()), testItemId, testSlot);
        EXPECT_EQ(canEquip, canEquip2) << "Iteration " << iteration 
            << ": Equipment slot compatibility should be deterministic";
        
        // 属性验证：空指针应该返回false
        bool nullCanEquip = inventoryManager_->CanEquipItemToSlot(nullptr, testItemId, testSlot);
        EXPECT_FALSE(nullCanEquip) << "Iteration " << iteration 
            << ": Null player should not be able to equip items";
        
        // 属性验证：无效槽位应该返回false
        bool invalidSlotCanEquip = inventoryManager_->CanEquipItemToSlot(
            reinterpret_cast<Player*>(mockPlayer.get()), testItemId, EQUIPMENT_SLOT_END);
        EXPECT_FALSE(invalidSlotCanEquip) << "Iteration " << iteration 
            << ": Invalid slot should not allow equipment";
    }
}

/**
 * 物品创建和移除测试
 * 验证背包物品管理的正确性
 */
TEST_F(InventoryManagerPropertyTest, ItemCreationAndRemoval)
{
    for (int iteration = 0; iteration < PROPERTY_TEST_ITERATIONS; ++iteration)
    {
        // 生成随机测试数据
        TestBotData botData = GenerateRandomBot();
        
        // 确保有足够的背包空间
        botData.totalBagSlots = 32;
        botData.usedBagSlots = 10;
        
        auto mockPlayer = CreateMockPlayer(botData);
        ASSERT_NE(mockPlayer.get(), nullptr) << "Iteration " << iteration << ": Mock player should not be null";
        
        // 测试物品创建
        std::uniform_int_distribution<size_t> itemDist(0, regularItemIds_.size() - 1);
        uint32 testItemId = regularItemIds_[itemDist(rng_)];
        
        BagSpaceInfo initialSpace = inventoryManager_->GetBagSpaceInfo(
            reinterpret_cast<Player*>(mockPlayer.get()));
        
        bool createResult = inventoryManager_->CreateItemInBag(
            reinterpret_cast<Player*>(mockPlayer.get()), testItemId, 1);
        
        if (initialSpace.freeSlots > 0)
        {
            // 属性验证：有空间时创建应该成功
            EXPECT_TRUE(createResult) << "Iteration " << iteration 
                << ": Item creation should succeed when bag has space";
            
            // 属性验证：物品应该能被找到
            bool itemExists = mockPlayer->HasItem(testItemId);
            EXPECT_TRUE(itemExists) << "Iteration " << iteration 
                << ": Created item should exist in bag";
        }
        
        // 测试物品查找
        bool foundItem = (inventoryManager_->FindItemInBags(
            reinterpret_cast<Player*>(mockPlayer.get()), testItemId) != nullptr);
        
        if (createResult)
        {
            EXPECT_TRUE(foundItem) << "Iteration " << iteration 
                << ": Created item should be findable";
        }
        
        // 测试重要物品保护
        if (inventoryManager_->IsImportantItem(testItemId))
        {
            bool removeResult = inventoryManager_->RemoveItemFromBag(
                reinterpret_cast<Player*>(mockPlayer.get()), testItemId, 1);
            EXPECT_FALSE(removeResult) << "Iteration " << iteration 
                << ": Important items should not be removable";
        }
        else if (foundItem)
        {
            // 测试普通物品移除
            bool removeResult = inventoryManager_->RemoveItemFromBag(
                reinterpret_cast<Player*>(mockPlayer.get()), testItemId, 1);
            EXPECT_TRUE(removeResult) << "Iteration " << iteration 
                << ": Regular items should be removable";
            
            // 属性验证：移除后物品应该不存在
            bool itemExistsAfterRemoval = mockPlayer->HasItem(testItemId);
            EXPECT_FALSE(itemExistsAfterRemoval) << "Iteration " << iteration 
                << ": Item should not exist after removal";
        }
        
        // 属性验证：空指针应该被正确处理
        bool nullCreateResult = inventoryManager_->CreateItemInBag(nullptr, testItemId, 1);
        EXPECT_FALSE(nullCreateResult) << "Iteration " << iteration 
            << ": Null player should fail item creation";
        
        bool nullRemoveResult = inventoryManager_->RemoveItemFromBag(nullptr, testItemId, 1);
        EXPECT_FALSE(nullRemoveResult) << "Iteration " << iteration 
            << ": Null player should fail item removal";
        
        // 属性验证：零数量应该被正确处理
        bool zeroCreateResult = inventoryManager_->CreateItemInBag(
            reinterpret_cast<Player*>(mockPlayer.get()), testItemId, 0);
        EXPECT_FALSE(zeroCreateResult) << "Iteration " << iteration 
            << ": Zero count should fail item creation";
        
        bool zeroRemoveResult = inventoryManager_->RemoveItemFromBag(
            reinterpret_cast<Player*>(mockPlayer.get()), testItemId, 0);
        EXPECT_FALSE(zeroRemoveResult) << "Iteration " << iteration 
            << ": Zero count should fail item removal";
    }
}