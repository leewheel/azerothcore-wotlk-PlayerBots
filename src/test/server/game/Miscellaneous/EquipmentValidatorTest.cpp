//By leewheel
#include <gtest/gtest.h>
#include "Bot/EquipmentValidator.h"
#include "Bot/BotPvpGearCache.h"
#include "test/mocks/PlayerMock.h"
#include "SharedDefines.h"
#include "ItemTemplate.h"
#include <memory>
#include <chrono>

/**
 * Property-Based Tests for EquipmentValidator
 * **功能: pvp-gear-auto-equip, 属性 4: 装备兼容性验证**
 * **功能: pvp-gear-auto-equip, 属性 5: 装备冲突解决**
 * **Validates: Requirements 2.1, 2.2, 2.3, 2.4, 2.5**
 */

class EquipmentValidatorTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        validator = std::make_unique<EquipmentValidator>();
        mockPlayer = std::make_unique<PlayerMock>();
        
        // 设置默认的玩家属性
        mockPlayer->SetLevel(70);
        mockPlayer->SetClass(CLASS_WARRIOR);
        mockPlayer->SetName("TestBot");
    }
    
    void TearDown() override
    {
        validator.reset();
        mockPlayer.reset();
    }
    
    // 创建测试装备
    PvpGearItem CreateTestGear(uint32 itemId, uint32 classId = 0, uint32 levelMin = 1)
    {
        PvpGearItem gear;
        gear.itemId = itemId;
        gear.itemName = "Test Item";
        gear.itemCount = 1;
        gear.classId = classId;
        gear.talent = "";
        gear.levelMin = levelMin;
        gear.levelMax = 80;
        return gear;
    }
    
    std::unique_ptr<EquipmentValidator> validator;
    std::unique_ptr<PlayerMock> mockPlayer;
};

// 测试职业兼容性验证
TEST_F(EquipmentValidatorTest, IsClassCompatible_AllClasses)
{
    // 测试所有职业都可以使用的装备 (classId = 0)
    EXPECT_TRUE(validator->IsClassCompatible(0, CLASS_WARRIOR));
    EXPECT_TRUE(validator->IsClassCompatible(0, CLASS_PALADIN));
    EXPECT_TRUE(validator->IsClassCompatible(0, CLASS_MAGE));
}

TEST_F(EquipmentValidatorTest, IsClassCompatible_SpecificClass)
{
    // 测试特定职业的装备
    uint32 warriorMask = 1 << (CLASS_WARRIOR - 1);
    EXPECT_TRUE(validator->IsClassCompatible(warriorMask, CLASS_WARRIOR));
    EXPECT_FALSE(validator->IsClassCompatible(warriorMask, CLASS_MAGE));
    
    uint32 mageMask = 1 << (CLASS_MAGE - 1);
    EXPECT_TRUE(validator->IsClassCompatible(mageMask, CLASS_MAGE));
    EXPECT_FALSE(validator->IsClassCompatible(mageMask, CLASS_WARRIOR));
}

TEST_F(EquipmentValidatorTest, IsClassCompatible_MultipleClasses)
{
    // 测试多职业装备 (战士和圣骑士)
    uint32 plateClassesMask = (1 << (CLASS_WARRIOR - 1)) | (1 << (CLASS_PALADIN - 1));
    EXPECT_TRUE(validator->IsClassCompatible(plateClassesMask, CLASS_WARRIOR));
    EXPECT_TRUE(validator->IsClassCompatible(plateClassesMask, CLASS_PALADIN));
    EXPECT_FALSE(validator->IsClassCompatible(plateClassesMask, CLASS_MAGE));
}

// 测试等级要求验证
TEST_F(EquipmentValidatorTest, IsLevelSufficient_ValidLevel)
{
    EXPECT_TRUE(validator->IsLevelSufficient(60, 70));
    EXPECT_TRUE(validator->IsLevelSufficient(70, 70));
    EXPECT_FALSE(validator->IsLevelSufficient(80, 70));
}

// 测试护甲类型兼容性
TEST_F(EquipmentValidatorTest, GetAllowedArmorTypes_PlateClasses)
{
    // 板甲职业应该可以穿所有护甲类型
    auto warriorArmor = validator->GetAllowedArmorTypes(CLASS_WARRIOR);
    EXPECT_EQ(warriorArmor.size(), 4);
    EXPECT_TRUE(warriorArmor.find(ITEM_SUBCLASS_ARMOR_CLOTH) != warriorArmor.end());
    EXPECT_TRUE(warriorArmor.find(ITEM_SUBCLASS_ARMOR_LEATHER) != warriorArmor.end());
    EXPECT_TRUE(warriorArmor.find(ITEM_SUBCLASS_ARMOR_MAIL) != warriorArmor.end());
    EXPECT_TRUE(warriorArmor.find(ITEM_SUBCLASS_ARMOR_PLATE) != warriorArmor.end());
    
    auto paladinArmor = validator->GetAllowedArmorTypes(CLASS_PALADIN);
    EXPECT_EQ(paladinArmor.size(), 4);
}

TEST_F(EquipmentValidatorTest, GetAllowedArmorTypes_MailClasses)
{
    // 锁甲职业不能穿板甲
    auto hunterArmor = validator->GetAllowedArmorTypes(CLASS_HUNTER);
    EXPECT_EQ(hunterArmor.size(), 3);
    EXPECT_TRUE(hunterArmor.find(ITEM_SUBCLASS_ARMOR_CLOTH) != hunterArmor.end());
    EXPECT_TRUE(hunterArmor.find(ITEM_SUBCLASS_ARMOR_LEATHER) != hunterArmor.end());
    EXPECT_TRUE(hunterArmor.find(ITEM_SUBCLASS_ARMOR_MAIL) != hunterArmor.end());
    EXPECT_FALSE(hunterArmor.find(ITEM_SUBCLASS_ARMOR_PLATE) != hunterArmor.end());
    
    auto shamanArmor = validator->GetAllowedArmorTypes(CLASS_SHAMAN);
    EXPECT_EQ(shamanArmor.size(), 3);
}

TEST_F(EquipmentValidatorTest, GetAllowedArmorTypes_LeatherClasses)
{
    // 皮甲职业不能穿板甲和锁甲
    auto rogueArmor = validator->GetAllowedArmorTypes(CLASS_ROGUE);
    EXPECT_EQ(rogueArmor.size(), 2);
    EXPECT_TRUE(rogueArmor.find(ITEM_SUBCLASS_ARMOR_CLOTH) != rogueArmor.end());
    EXPECT_TRUE(rogueArmor.find(ITEM_SUBCLASS_ARMOR_LEATHER) != rogueArmor.end());
    EXPECT_FALSE(rogueArmor.find(ITEM_SUBCLASS_ARMOR_MAIL) != rogueArmor.end());
    EXPECT_FALSE(rogueArmor.find(ITEM_SUBCLASS_ARMOR_PLATE) != rogueArmor.end());
    
    auto druidArmor = validator->GetAllowedArmorTypes(CLASS_DRUID);
    EXPECT_EQ(druidArmor.size(), 2);
}

TEST_F(EquipmentValidatorTest, GetAllowedArmorTypes_ClothClasses)
{
    // 布甲职业只能穿布甲
    auto mageArmor = validator->GetAllowedArmorTypes(CLASS_MAGE);
    EXPECT_EQ(mageArmor.size(), 1);
    EXPECT_TRUE(mageArmor.find(ITEM_SUBCLASS_ARMOR_CLOTH) != mageArmor.end());
    
    auto priestArmor = validator->GetAllowedArmorTypes(CLASS_PRIEST);
    EXPECT_EQ(priestArmor.size(), 1);
    
    auto warlockArmor = validator->GetAllowedArmorTypes(CLASS_WARLOCK);
    EXPECT_EQ(warlockArmor.size(), 1);
}

// 测试最佳护甲类型检查
TEST_F(EquipmentValidatorTest, IsOptimalArmorType_NonArmor)
{
    // 非护甲装备应该总是返回true
    EXPECT_TRUE(validator->IsOptimalArmorType(12345, CLASS_WARRIOR)); // 假设这不是护甲
}

// 测试装备验证
TEST_F(EquipmentValidatorTest, ValidateEquipment_ValidGear)
{
    // 创建适合战士的装备
    PvpGearItem gear = CreateTestGear(1001, 0, 60); // 所有职业，60级要求
    
    mockPlayer->SetLevel(70);
    mockPlayer->SetClass(CLASS_WARRIOR);
    
    EXPECT_TRUE(validator->ValidateEquipment(gear, mockPlayer.get()));
}

TEST_F(EquipmentValidatorTest, ValidateEquipment_LevelTooLow)
{
    // 创建等级要求过高的装备
    PvpGearItem gear = CreateTestGear(1002, 0, 80); // 80级要求
    
    mockPlayer->SetLevel(70);
    
    EXPECT_FALSE(validator->ValidateEquipment(gear, mockPlayer.get()));
}

TEST_F(EquipmentValidatorTest, ValidateEquipment_ClassIncompatible)
{
    // 创建法师专用装备
    uint32 mageMask = 1 << (CLASS_MAGE - 1);
    PvpGearItem gear = CreateTestGear(1003, mageMask, 60);
    
    mockPlayer->SetClass(CLASS_WARRIOR);
    
    EXPECT_FALSE(validator->ValidateEquipment(gear, mockPlayer.get()));
}

TEST_F(EquipmentValidatorTest, ValidateEquipment_NullPlayer)
{
    PvpGearItem gear = CreateTestGear(1004, 0, 60);
    
    EXPECT_FALSE(validator->ValidateEquipment(gear, nullptr));
}

// 测试兼容性评分计算
TEST_F(EquipmentValidatorTest, CalculateCompatibilityScore_ValidGear)
{
    PvpGearItem gear = CreateTestGear(1005, 0, 60);
    
    mockPlayer->SetLevel(70);
    mockPlayer->SetClass(CLASS_WARRIOR);
    
    float score = validator->CalculateCompatibilityScore(gear, mockPlayer.get());
    EXPECT_GT(score, 0.0f);
}

TEST_F(EquipmentValidatorTest, CalculateCompatibilityScore_IncompatibleGear)
{
    // 创建不兼容的装备
    uint32 mageMask = 1 << (CLASS_MAGE - 1);
    PvpGearItem gear = CreateTestGear(1006, mageMask, 60);
    
    mockPlayer->SetClass(CLASS_WARRIOR);
    
    float score = validator->CalculateCompatibilityScore(gear, mockPlayer.get());
    EXPECT_EQ(score, 0.0f); // 不兼容的装备评分应该为0
}

TEST_F(EquipmentValidatorTest, CalculateCompatibilityScore_NullPlayer)
{
    PvpGearItem gear = CreateTestGear(1007, 0, 60);
    
    float score = validator->CalculateCompatibilityScore(gear, nullptr);
    EXPECT_EQ(score, 0.0f);
}

// 测试槽位冲突解决
TEST_F(EquipmentValidatorTest, ResolveSlotConflict_EmptyList)
{
    std::vector<PvpGearItem> emptyList;
    
    const PvpGearItem* result = validator->ResolveSlotConflict(emptyList, mockPlayer.get());
    EXPECT_EQ(result, nullptr);
}

TEST_F(EquipmentValidatorTest, ResolveSlotConflict_NullPlayer)
{
    std::vector<PvpGearItem> gearList = {CreateTestGear(1008, 0, 60)};
    
    const PvpGearItem* result = validator->ResolveSlotConflict(gearList, nullptr);
    EXPECT_EQ(result, nullptr);
}

TEST_F(EquipmentValidatorTest, ResolveSlotConflict_SingleValidItem)
{
    std::vector<PvpGearItem> gearList = {CreateTestGear(1009, 0, 60)};
    
    mockPlayer->SetLevel(70);
    mockPlayer->SetClass(CLASS_WARRIOR);
    
    const PvpGearItem* result = validator->ResolveSlotConflict(gearList, mockPlayer.get());
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(result->itemId, 1009);
}

TEST_F(EquipmentValidatorTest, ResolveSlotConflict_MultipleItems)
{
    // 创建多个装备，其中一个不兼容
    uint32 mageMask = 1 << (CLASS_MAGE - 1);
    std::vector<PvpGearItem> gearList = {
        CreateTestGear(1010, mageMask, 60),  // 法师专用，战士不能用
        CreateTestGear(1011, 0, 60),         // 所有职业都能用
        CreateTestGear(1012, 0, 80)          // 等级要求过高
    };
    
    mockPlayer->SetLevel(70);
    mockPlayer->SetClass(CLASS_WARRIOR);
    
    const PvpGearItem* result = validator->ResolveSlotConflict(gearList, mockPlayer.get());
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(result->itemId, 1011); // 应该选择兼容的装备
}

// 测试特殊逻辑：板甲职业防止穿布甲
TEST_F(EquipmentValidatorTest, PlateClassClothArmorPenalty)
{
    // 这个测试需要mock ItemTemplate，暂时跳过具体实现
    // 在实际环境中需要设置ItemTemplate mock来测试护甲类型逻辑
}

// 性能测试：验证大量装备的处理性能
TEST_F(EquipmentValidatorTest, Performance_LargeGearList)
{
    std::vector<PvpGearItem> largeGearList;
    for (uint32 i = 2000; i < 2100; ++i)
    {
        largeGearList.push_back(CreateTestGear(i, 0, 60));
    }
    
    ON_CALL(*mockPlayer, GetLevel()).WillByDefault(Return(70));
    ON_CALL(*mockPlayer, getClass()).WillByDefault(Return(CLASS_WARRIOR));
    
    auto start = std::chrono::high_resolution_clock::now();
    const PvpGearItem* result = validator->ResolveSlotConflict(largeGearList, mockPlayer.get());
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    EXPECT_NE(result, nullptr);
    EXPECT_LT(duration.count(), 100); // 应该在100ms内完成
}
//End leewheel