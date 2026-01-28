//By leewheel
#include <gtest/gtest.h>
#include "Bot/EquipmentValidator.h"
#include "test/mocks/PlayerMock.h"
#include "SharedDefines.h"
#include <memory>
#include <random>
#include <chrono>

/**
 * Property-Based Tests for EquipmentValidator
 * **功能: pvp-gear-auto-equip, 属性 4: 装备兼容性验证**
 * **功能: pvp-gear-auto-equip, 属性 5: 装备冲突解决**
 * **Validates: Requirements 2.1, 2.2, 2.3, 2.4, 2.5**
 */

class EquipmentValidatorPropertyTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        validator = std::make_unique<EquipmentValidator>();
        
        // Initialize random number generator
        rng.seed(std::chrono::steady_clock::now().time_since_epoch().count());
    }
    
    void TearDown() override
    {
        validator.reset();
    }
    
    // Generate random bot for property testing
    std::unique_ptr<PlayerMock> GenerateRandomBot()
    {
        std::uniform_int_distribution<uint8> classDist(1, 11);
        std::uniform_int_distribution<uint8> levelDist(1, 80);
        std::uniform_int_distribution<uint64> guidDist(1000, 999999);
        
        uint8 playerClass = classDist(rng);
        // Skip invalid classes
        if (playerClass == 10) playerClass = 11; // Death Knight
        
        uint8 level = levelDist(rng);
        uint64 guid = guidDist(rng);
        
        auto bot = std::make_unique<PlayerMock>();
        bot->SetClass(playerClass);
        bot->SetLevel(level);
        bot->SetGUID(ObjectGuid::Create<HighGuid::Player>(guid));
        bot->SetName("TestBot" + std::to_string(guid));
        
        return bot;
    }
    
    // Generate random gear item for testing
    PvpGearItem GenerateRandomGear()
    {
        std::uniform_int_distribution<uint32> itemIdDist(1000, 99999);
        std::uniform_int_distribution<uint32> classDist(0, 1023); // Class mask
        std::uniform_int_distribution<uint32> levelDist(1, 80);
        
        PvpGearItem gear;
        gear.itemId = itemIdDist(rng);
        gear.itemName = "Random Test Item " + std::to_string(gear.itemId);
        gear.itemCount = 1;
        gear.classId = classDist(rng);
        gear.talent = "";
        gear.levelMin = levelDist(rng);
        gear.levelMax = 80;
        
        return gear;
    }
    
    std::unique_ptr<EquipmentValidator> validator;
    std::mt19937 rng;
};

/**
 * Property Test: Equipment Compatibility Validation - Core Property 4
 * **功能: pvp-gear-auto-equip, 属性 4: 装备兼容性验证**
 * **Validates: Requirements 2.1, 2.2, 2.3, 2.4**
 * 
 * Property 4: For any equipment and bot combination, the system should validate 
 * class, level, and armor type compatibility, rejecting incompatible equipment
 */
TEST_F(EquipmentValidatorPropertyTest, EquipmentCompatibilityValidationProperty)
{
    const int PROPERTY_TEST_ITERATIONS = 100;
    
    for (int iteration = 0; iteration < PROPERTY_TEST_ITERATIONS; ++iteration)
    {
        // Generate random bot and gear
        auto bot = GenerateRandomBot();
        PvpGearItem gear = GenerateRandomGear();
        
        ASSERT_NE(bot.get(), nullptr) << "Iteration " << iteration << ": Bot should not be null";
        
        uint8 botClass = bot->getClass();
        uint8 botLevel = bot->GetLevel();
        
        // Property: ValidateEquipment should be consistent with individual validation methods
        bool overallValid = validator->ValidateEquipment(gear, bot.get());
        bool classValid = validator->IsClassCompatible(gear.classId, botClass);
        bool levelValid = validator->IsLevelSufficient(gear.levelMin, botLevel);
        bool armorValid = validator->IsArmorTypeCompatible(gear.itemId, botClass);
        
        // Property: Overall validation should be true only if all individual validations pass
        bool expectedValid = classValid && levelValid && armorValid;
        EXPECT_EQ(overallValid, expectedValid) << "Iteration " << iteration 
            << ": Overall validation should match individual validations"
            << " (class: " << classValid << ", level: " << levelValid 
            << ", armor: " << armorValid << ", overall: " << overallValid << ")";
        
        // Property: Class compatibility should be deterministic
        bool classValid2 = validator->IsClassCompatible(gear.classId, botClass);
        EXPECT_EQ(classValid, classValid2) << "Iteration " << iteration 
            << ": Class compatibility should be deterministic";
        
        // Property: Level validation should follow logical rules
        if (botLevel >= gear.levelMin)
        {
            EXPECT_TRUE(levelValid) << "Iteration " << iteration 
                << ": Bot level " << (int)botLevel << " should be sufficient for gear requiring " 
                << gear.levelMin;
        }
        else
        {
            EXPECT_FALSE(levelValid) << "Iteration " << iteration 
                << ": Bot level " << (int)botLevel << " should be insufficient for gear requiring " 
                << gear.levelMin;
        }
        
        // Property: Armor type compatibility should respect class restrictions
        auto allowedArmor = validator->GetAllowedArmorTypes(botClass);
        
        // Property: Allowed armor types should be non-empty for valid classes
        if (botClass >= CLASS_WARRIOR && botClass <= CLASS_DRUID)
        {
            EXPECT_FALSE(allowedArmor.empty()) << "Iteration " << iteration 
                << ": Valid class " << (int)botClass << " should have allowed armor types";
        }
        
        // Property: Class-specific armor restrictions should be enforced
        switch (botClass)
        {
            case CLASS_PRIEST:
            case CLASS_MAGE:
            case CLASS_WARLOCK:
                // Cloth classes should only allow cloth
                EXPECT_EQ(allowedArmor.size(), 1) << "Iteration " << iteration 
                    << ": Cloth class " << (int)botClass << " should only allow cloth armor";
                EXPECT_TRUE(allowedArmor.find(ITEM_SUBCLASS_ARMOR_CLOTH) != allowedArmor.end()) 
                    << "Iteration " << iteration << ": Cloth class should allow cloth armor";
                break;
                
            case CLASS_ROGUE:
            case CLASS_DRUID:
                // Leather classes should allow cloth and leather
                EXPECT_EQ(allowedArmor.size(), 2) << "Iteration " << iteration 
                    << ": Leather class " << (int)botClass << " should allow 2 armor types";
                EXPECT_TRUE(allowedArmor.find(ITEM_SUBCLASS_ARMOR_CLOTH) != allowedArmor.end()) 
                    << "Iteration " << iteration << ": Leather class should allow cloth armor";
                EXPECT_TRUE(allowedArmor.find(ITEM_SUBCLASS_ARMOR_LEATHER) != allowedArmor.end()) 
                    << "Iteration " << iteration << ": Leather class should allow leather armor";
                break;
                
            case CLASS_HUNTER:
            case CLASS_SHAMAN:
                // Mail classes should allow cloth, leather, and mail
                EXPECT_EQ(allowedArmor.size(), 3) << "Iteration " << iteration 
                    << ": Mail class " << (int)botClass << " should allow 3 armor types";
                EXPECT_TRUE(allowedArmor.find(ITEM_SUBCLASS_ARMOR_MAIL) != allowedArmor.end()) 
                    << "Iteration " << iteration << ": Mail class should allow mail armor";
                break;
                
            case CLASS_WARRIOR:
            case CLASS_PALADIN:
            case CLASS_DEATH_KNIGHT:
                // Plate classes should allow all armor types
                EXPECT_EQ(allowedArmor.size(), 4) << "Iteration " << iteration 
                    << ": Plate class " << (int)botClass << " should allow all armor types";
                EXPECT_TRUE(allowedArmor.find(ITEM_SUBCLASS_ARMOR_PLATE) != allowedArmor.end()) 
                    << "Iteration " << iteration << ": Plate class should allow plate armor";
                break;
        }
    }
}

/**
 * Property Test: Slot Conflict Resolution - Core Property 5
 * **功能: pvp-gear-auto-equip, 属性 5: 装备冲突解决**
 * **Validates: Requirements 2.5**
 * 
 * Property 5: For any slot conflict situation, the system should select 
 * the highest-scoring equipment for equipping
 */
TEST_F(EquipmentValidatorPropertyTest, SlotConflictResolutionProperty)
{
    const int CONFLICT_TEST_ITERATIONS = 75;
    
    for (int iteration = 0; iteration < CONFLICT_TEST_ITERATIONS; ++iteration)
    {
        // Generate random bot
        auto bot = GenerateRandomBot();
        ASSERT_NE(bot.get(), nullptr) << "Iteration " << iteration << ": Bot should not be null";
        
        // Generate multiple conflicting gear items
        std::vector<PvpGearItem> conflictingGear;
        std::uniform_int_distribution<int> gearCountDist(2, 5);
        int gearCount = gearCountDist(rng);
        
        for (int i = 0; i < gearCount; ++i)
        {
            PvpGearItem gear = GenerateRandomGear();
            // Ensure some gear is compatible
            if (i == 0)
            {
                gear.classId = 0; // Universal gear
                gear.levelMin = 1; // Low level requirement
            }
            conflictingGear.push_back(gear);
        }
        
        // Property: ResolveSlotConflict should return a valid item or null
        const PvpGearItem* selectedGear = validator->ResolveSlotConflict(conflictingGear, bot.get());
        
        if (selectedGear != nullptr)
        {
            // Property: Selected gear should be from the input list
            bool foundInList = false;
            for (const auto& gear : conflictingGear)
            {
                if (gear.itemId == selectedGear->itemId)
                {
                    foundInList = true;
                    break;
                }
            }
            EXPECT_TRUE(foundInList) << "Iteration " << iteration 
                << ": Selected gear should be from the input list";
            
            // Property: Selected gear should be valid for the bot
            bool isValid = validator->ValidateEquipment(*selectedGear, bot.get());
            EXPECT_TRUE(isValid) << "Iteration " << iteration 
                << ": Selected gear should be valid for the bot";
            
            // Property: Selected gear should have the highest compatibility score among valid items
            float selectedScore = validator->CalculateCompatibilityScore(*selectedGear, bot.get());
            
            for (const auto& gear : conflictingGear)
            {
                if (validator->ValidateEquipment(gear, bot.get()))
                {
                    float gearScore = validator->CalculateCompatibilityScore(gear, bot.get());
                    EXPECT_GE(selectedScore, gearScore - 0.001f) << "Iteration " << iteration 
                        << ": Selected gear should have highest or equal score among valid items";
                }
            }
        }
        else
        {
            // Property: If no gear is selected, all gear should be invalid
            bool hasValidGear = false;
            for (const auto& gear : conflictingGear)
            {
                if (validator->ValidateEquipment(gear, bot.get()))
                {
                    hasValidGear = true;
                    break;
                }
            }
            EXPECT_FALSE(hasValidGear) << "Iteration " << iteration 
                << ": If no gear is selected, all gear should be invalid";
        }
        
        // Property: Empty list should return null
        const PvpGearItem* emptyResult = validator->ResolveSlotConflict({}, bot.get());
        EXPECT_EQ(emptyResult, nullptr) << "Iteration " << iteration 
            << ": Empty gear list should return null";
        
        // Property: Null bot should return null
        const PvpGearItem* nullBotResult = validator->ResolveSlotConflict(conflictingGear, nullptr);
        EXPECT_EQ(nullBotResult, nullptr) << "Iteration " << iteration 
            << ": Null bot should return null";
    }
}

/**
 * Property Test: Compatibility Score Consistency
 * **功能: pvp-gear-auto-equip, 属性 4: 装备兼容性验证**
 * **Validates: Requirements 2.1, 2.2, 2.3**
 * 
 * Tests that compatibility scores are consistent and meaningful
 */
TEST_F(EquipmentValidatorPropertyTest, CompatibilityScoreConsistencyProperty)
{
    const int SCORE_TEST_ITERATIONS = 100;
    
    for (int iteration = 0; iteration < SCORE_TEST_ITERATIONS; ++iteration)
    {
        // Generate random bot and gear
        auto bot = GenerateRandomBot();
        PvpGearItem gear = GenerateRandomGear();
        
        ASSERT_NE(bot.get(), nullptr) << "Iteration " << iteration << ": Bot should not be null";
        
        // Property: Compatibility score should be non-negative
        float score = validator->CalculateCompatibilityScore(gear, bot.get());
        EXPECT_GE(score, 0.0f) << "Iteration " << iteration 
            << ": Compatibility score should be non-negative";
        
        // Property: Invalid equipment should have zero score
        bool isValid = validator->ValidateEquipment(gear, bot.get());
        if (!isValid)
        {
            EXPECT_EQ(score, 0.0f) << "Iteration " << iteration 
                << ": Invalid equipment should have zero compatibility score";
        }
        else
        {
            EXPECT_GT(score, 0.0f) << "Iteration " << iteration 
                << ": Valid equipment should have positive compatibility score";
        }
        
        // Property: Score calculation should be deterministic
        float score2 = validator->CalculateCompatibilityScore(gear, bot.get());
        EXPECT_FLOAT_EQ(score, score2) << "Iteration " << iteration 
            << ": Compatibility score should be deterministic";
        
        // Property: Null bot should return zero score
        float nullBotScore = validator->CalculateCompatibilityScore(gear, nullptr);
        EXPECT_EQ(nullBotScore, 0.0f) << "Iteration " << iteration 
            << ": Null bot should result in zero compatibility score";
        
        // Property: Better gear should generally have higher scores
        // Create obviously better gear (universal, low level requirement)
        PvpGearItem betterGear = gear;
        betterGear.classId = 0; // Universal
        betterGear.levelMin = 1; // Low requirement
        
        float betterScore = validator->CalculateCompatibilityScore(betterGear, bot.get());
        
        // If both are valid, better gear should have higher or equal score
        bool betterIsValid = validator->ValidateEquipment(betterGear, bot.get());
        if (isValid && betterIsValid)
        {
            EXPECT_GE(betterScore, score) << "Iteration " << iteration 
                << ": Better gear should have higher or equal compatibility score";
        }
    }
}

/**
 * Property Test: Armor Type Restrictions
 * **功能: pvp-gear-auto-equip, 属性 4: 装备兼容性验证**
 * **Validates: Requirements 2.2**
 * 
 * Tests that armor type restrictions are properly enforced
 */
TEST_F(EquipmentValidatorPropertyTest, ArmorTypeRestrictionsProperty)
{
    const int ARMOR_TEST_ITERATIONS = 50;
    
    // Test all class combinations
    for (uint8 testClass = CLASS_WARRIOR; testClass <= CLASS_DRUID; ++testClass)
    {
        auto bot = std::make_unique<PlayerMock>();
        bot->SetClass(testClass);
        bot->SetLevel(70); // High level to avoid level restrictions
        
        // Property: GetAllowedArmorTypes should return consistent results
        auto allowedTypes1 = validator->GetAllowedArmorTypes(testClass);
        auto allowedTypes2 = validator->GetAllowedArmorTypes(testClass);
        
        EXPECT_EQ(allowedTypes1.size(), allowedTypes2.size()) 
            << "Class " << (int)testClass << ": GetAllowedArmorTypes should be consistent";
        
        for (auto type : allowedTypes1)
        {
            EXPECT_TRUE(allowedTypes2.find(type) != allowedTypes2.end()) 
                << "Class " << (int)testClass << ": Allowed armor types should be consistent";
        }
        
        // Property: Armor type compatibility should match allowed types
        // Note: This test is limited because we don't have actual ItemTemplate data
        // In a real environment, we would test with actual armor items
        
        // Property: Class restrictions should be logical
        switch (testClass)
        {
            case CLASS_PRIEST:
            case CLASS_MAGE:
            case CLASS_WARLOCK:
                // Cloth classes: most restrictive
                EXPECT_LE(allowedTypes1.size(), 1u) 
                    << "Class " << (int)testClass << ": Cloth classes should be most restrictive";
                break;
                
            case CLASS_ROGUE:
            case CLASS_DRUID:
                // Leather classes: moderate restrictions
                EXPECT_LE(allowedTypes1.size(), 2u) 
                    << "Class " << (int)testClass << ": Leather classes should allow up to 2 armor types";
                break;
                
            case CLASS_HUNTER:
            case CLASS_SHAMAN:
                // Mail classes: fewer restrictions
                EXPECT_LE(allowedTypes1.size(), 3u) 
                    << "Class " << (int)testClass << ": Mail classes should allow up to 3 armor types";
                break;
                
            case CLASS_WARRIOR:
            case CLASS_PALADIN:
            case CLASS_DEATH_KNIGHT:
                // Plate classes: least restrictive
                EXPECT_LE(allowedTypes1.size(), 4u) 
                    << "Class " << (int)testClass << ": Plate classes should allow up to 4 armor types";
                break;
        }
    }
}
//End leewheel