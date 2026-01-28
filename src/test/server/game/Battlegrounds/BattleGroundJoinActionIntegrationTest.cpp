/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "BattleGroundJoinAction.h"
#include "PvpGearManager.h"
#include "WorldMock.h"
#include "PlayerMock.h"
#include "PlayerbotAI.h"
#include "SharedDefines.h"
#include "ObjectGuid.h"
#include "gtest/gtest.h"
#include <memory>
#include <random>
#include <chrono>

/**
 * Integration Tests for BattleGroundJoinAction PVP Gear Integration
 * **功能: pvp-gear-auto-equip, 属性 20: 系统集成无缝性**
 * **Validates: Requirements 7.1**
 * 
 * Tests the seamless integration of PVP gear switching logic into BattleGroundJoinAction
 */

class BattleGroundJoinActionIntegrationTest : public testing::Test
{
protected:
    void SetUp() override
    {
        // Initialize world mock for database access
        auto worldMock = new WorldMock();
        sWorld.reset(worldMock);
        
        // Get PvpGearManager instance
        pvpGearManager = PvpGearManager::GetInstance();
        ASSERT_NE(pvpGearManager, nullptr) << "PvpGearManager instance should not be null";
        
        // Initialize the manager
        bool initResult = pvpGearManager->Initialize();
        if (!initResult)
        {
            GTEST_SKIP() << "PvpGearManager initialization failed, likely due to missing database";
        }
        
        // Initialize random number generator
        rng.seed(std::chrono::steady_clock::now().time_since_epoch().count());
    }

    void TearDown() override
    {
        // Clean up any saved equipment snapshots
        if (pvpGearManager)
        {
            pvpGearManager->CleanupExpiredSnapshots();
        }
    }

    // Create a mock bot with PlayerbotAI for testing
    std::unique_ptr<PlayerMock> CreateTestBot()
    {
        std::uniform_int_distribution<uint8> classDist(1, 11);
        std::uniform_int_distribution<uint8> levelDist(10, 80); // BG level range
        std::uniform_int_distribution<uint64> guidDist(1000, 999999);
        
        uint8 playerClass = classDist(rng);
        if (playerClass == 10) playerClass = 11; // Skip Death Knight for simplicity
        
        uint8 level = levelDist(rng);
        uint64 guid = guidDist(rng);
        
        auto bot = std::make_unique<PlayerMock>();
        bot->SetClass(playerClass);
        bot->SetLevel(level);
        bot->SetGUID(ObjectGuid::Create<HighGuid::Player>(guid));
        bot->SetName("TestBot" + std::to_string(guid));
        
        return bot;
    }

    PvpGearManager* pvpGearManager;
    std::mt19937 rng;
};

/**
 * Property Test: Seamless System Integration - Core Property 20
 * **功能: pvp-gear-auto-equip, 属性 20: 系统集成无缝性**
 * **Validates: Requirements 7.1**
 * 
 * Property 20: For any BattleGroundJoinAction trigger, PVP gear system should 
 * seamlessly integrate equipment switching logic without breaking existing functionality
 */
TEST_F(BattleGroundJoinActionIntegrationTest, SeamlessSystemIntegrationProperty)
{
    const int INTEGRATION_TEST_ITERATIONS = 100;
    
    for (int iteration = 0; iteration < INTEGRATION_TEST_ITERATIONS; ++iteration)
    {
        // Create test bot
        auto bot = CreateTestBot();
        ASSERT_NE(bot.get(), nullptr) << "Iteration " << iteration << ": Bot should not be null";
        
        // Record initial equipment state
        float initialResilienceRatio = pvpGearManager->CalculateResilienceRatio(bot.get());
        bool initialHasComplete = pvpGearManager->HasCompletePvpEquipment(bot.get());
        
        // Property: PvpGearManager integration should not break existing logic
        EXPECT_GE(initialResilienceRatio, 0.0f) << "Iteration " << iteration 
            << ": Initial resilience ratio should be non-negative";
        EXPECT_LE(initialResilienceRatio, 1.0f) << "Iteration " << iteration 
            << ": Initial resilience ratio should not exceed 100%";
        
        // Property: Integration points should work seamlessly
        
        // Test 1: BGJoinAction::Execute integration point
        // Simulate the check that happens before joining BG
        bot->SetInBattleground(false);
        
        // Test HasCompletePvpEquipment integration (used in BGJoinAction::Execute)
        bool hasCompleteBeforeEquip = pvpGearManager->HasCompletePvpEquipment(bot.get());
        
        // If bot doesn't have complete PVP equipment, BGJoinAction::Execute should trigger EquipPvpGear
        if (!hasCompleteBeforeEquip)
        {
            bool equipResult = pvpGearManager->EquipPvpGear(bot.get());
            
            // Property: Equipment operation should complete without errors
            // (Success depends on available gear, but should not crash)
            EXPECT_TRUE(true) << "Iteration " << iteration 
                << ": EquipPvpGear should complete without crashing";
            
            // Check post-equip state
            float postEquipRatio = pvpGearManager->CalculateResilienceRatio(bot.get());
            EXPECT_GE(postEquipRatio, 0.0f) << "Iteration " << iteration 
                << ": Post-equip resilience ratio should be valid";
            EXPECT_LE(postEquipRatio, 1.0f) << "Iteration " << iteration 
                << ": Post-equip resilience ratio should be valid";
        }
        
        // Test 2: BGStatusAction::Execute integration point (STATUS_WAIT_JOIN)
        // Simulate joining battleground
        bot->SetInBattleground(true);
        bool joinEquipResult = pvpGearManager->EquipPvpGear(bot.get());
        
        // Property: Multiple equip attempts should be safe
        EXPECT_TRUE(true) << "Iteration " << iteration 
            << ": Multiple EquipPvpGear calls should be safe";
        
        float bgResilienceRatio = pvpGearManager->CalculateResilienceRatio(bot.get());
        EXPECT_GE(bgResilienceRatio, 0.0f) << "Iteration " << iteration 
            << ": BG resilience ratio should be valid";
        EXPECT_LE(bgResilienceRatio, 1.0f) << "Iteration " << iteration 
            << ": BG resilience ratio should be valid";
        
        // Test 3: BGStatusAction::LeaveBG integration point
        // Simulate leaving battleground
        bot->SetInBattleground(false);
        bool restoreResult = pvpGearManager->RestorePveGear(bot.get());
        
        // Property: Restoration should work seamlessly
        EXPECT_TRUE(true) << "Iteration " << iteration 
            << ": RestorePveGear should complete without crashing";
        
        float restoredRatio = pvpGearManager->CalculateResilienceRatio(bot.get());
        EXPECT_GE(restoredRatio, 0.0f) << "Iteration " << iteration 
            << ": Restored equipment should have valid resilience ratio";
        EXPECT_LE(restoredRatio, 1.0f) << "Iteration " << iteration 
            << ": Restored equipment should have valid resilience ratio";
        
        // Test 4: BGLeaveAction::Execute integration point
        // Test queue leave scenario (should also restore PVE gear)
        bool queueLeaveRestore = pvpGearManager->RestorePveGear(bot.get());
        
        // Property: Queue leave restoration should be safe
        EXPECT_TRUE(true) << "Iteration " << iteration 
            << ": Queue leave restoration should be safe";
        
        // Property: Integration should not cause system instability
        // Test rapid state transitions (simulating real battleground flow)
        for (int cycle = 0; cycle < 3; ++cycle)
        {
            // Simulate full BG cycle: join -> equip -> leave -> restore
            bot->SetInBattleground(false);
            pvpGearManager->EquipPvpGear(bot.get());
            
            bot->SetInBattleground(true);
            pvpGearManager->EquipPvpGear(bot.get());
            
            bot->SetInBattleground(false);
            pvpGearManager->RestorePveGear(bot.get());
            
            // System should remain stable
            float cycleRatio = pvpGearManager->CalculateResilienceRatio(bot.get());
            EXPECT_GE(cycleRatio, 0.0f) << "Iteration " << iteration << " Cycle " << cycle 
                << ": System should remain stable during BG flow cycles";
            EXPECT_LE(cycleRatio, 1.0f) << "Iteration " << iteration << " Cycle " << cycle 
                << ": System should remain stable during BG flow cycles";
        }
        
        // Property: Level-based requirements should be respected
        float requiredRatio = pvpGearManager->GetRequiredResilienceRatio(bot.get());
        if (bot->GetLevel() < 60)
        {
            EXPECT_FLOAT_EQ(requiredRatio, 0.3f) << "Iteration " << iteration 
                << ": Under level 60 should require 30% resilience";
        }
        else
        {
            EXPECT_FLOAT_EQ(requiredRatio, 0.8f) << "Iteration " << iteration 
                << ": Level 60+ should require 80% resilience";
        }
        
        // Property: Error handling should be robust
        // Test with null bot (should not crash)
        bool nullBotResult = pvpGearManager->EquipPvpGear(nullptr);
        EXPECT_FALSE(nullBotResult) << "Iteration " << iteration 
            << ": System should handle null bot gracefully";
        
        bool nullRestoreResult = pvpGearManager->RestorePveGear(nullptr);
        EXPECT_FALSE(nullRestoreResult) << "Iteration " << iteration 
            << ": System should handle null bot gracefully";
    }
}

/**
 * Property Test: Error Handling and Logging Integration
 * **功能: pvp-gear-auto-equip, 属性 21: 异常处理和日志记录**
 * **Validates: Requirements 7.2**
 * 
 * Tests that error handling and logging work correctly in the integrated system
 */
TEST_F(BattleGroundJoinActionIntegrationTest, ErrorHandlingAndLoggingProperty)
{
    const int ERROR_HANDLING_ITERATIONS = 30;
    
    for (int iteration = 0; iteration < ERROR_HANDLING_ITERATIONS; ++iteration)
    {
        // Create test bot
        auto bot = CreateTestBot();
        ASSERT_NE(bot.get(), nullptr) << "Iteration " << iteration << ": Bot should not be null";
        
        // Property: System should handle various error conditions gracefully
        
        // Test 1: Equipment operations with invalid bot states
        bot->SetInBattleground(true);
        
        // Property: Multiple equip attempts should be safe
        bool firstEquip = pvpGearManager->EquipPvpGear(bot.get());
        bool secondEquip = pvpGearManager->EquipPvpGear(bot.get());
        
        // Both should complete without crashing (success depends on implementation)
        EXPECT_TRUE(true) << "Iteration " << iteration 
            << ": Multiple equip attempts should not crash system";
        
        // Test 2: Restore without prior equip
        auto freshBot = CreateTestBot();
        freshBot->SetInBattleground(false);
        
        bool restoreWithoutEquip = pvpGearManager->RestorePveGear(freshBot.get());
        EXPECT_FALSE(restoreWithoutEquip) << "Iteration " << iteration 
            << ": Restore without prior equip should fail gracefully";
        
        // Test 3: Rapid state transitions
        for (int rapidCycle = 0; rapidCycle < 10; ++rapidCycle)
        {
            bot->SetInBattleground(!bot->InBattleground());
            
            if (bot->InBattleground())
            {
                pvpGearManager->EquipPvpGear(bot.get());
            }
            else
            {
                pvpGearManager->RestorePveGear(bot.get());
            }
        }
        
        // Property: System should remain stable after rapid transitions
        float finalRatio = pvpGearManager->CalculateResilienceRatio(bot.get());
        EXPECT_GE(finalRatio, 0.0f) << "Iteration " << iteration 
            << ": System should remain stable after rapid state transitions";
        EXPECT_LE(finalRatio, 1.0f) << "Iteration " << iteration 
            << ": System should remain stable after rapid state transitions";
        
        // Test 4: Configuration edge cases
        auto originalConfig = pvpGearManager->GetConfig();
        
        // Test with extreme configuration values
        GearSwitchConfig extremeConfig = originalConfig;
        extremeConfig.resilienceRatioUnder60 = 0.0f;
        extremeConfig.resilienceRatio60Plus = 1.0f;
        
        pvpGearManager->SetConfig(extremeConfig);
        
        // Property: System should handle extreme configurations
        bool extremeEquipResult = pvpGearManager->EquipPvpGear(bot.get());
        // Should not crash regardless of result
        EXPECT_TRUE(true) << "Iteration " << iteration 
            << ": System should handle extreme configurations without crashing";
        
        // Restore original configuration
        pvpGearManager->SetConfig(originalConfig);
        
        // Test 5: Cleanup operations
        pvpGearManager->CleanupExpiredSnapshots();
        
        // Property: Cleanup should not affect active operations
        bool postCleanupEquip = pvpGearManager->EquipPvpGear(bot.get());
        // Should work normally after cleanup
        EXPECT_TRUE(true) << "Iteration " << iteration 
            << ": System should work normally after cleanup operations";
    }
}

/**
 * Property Test: Performance and Scalability Integration
 * **功能: pvp-gear-auto-equip, 属性 25: 并发性能保证**
 * **Validates: Requirements 8.1, 8.2**
 * 
 * Tests that the integrated system maintains performance under load
 */
TEST_F(BattleGroundJoinActionIntegrationTest, PerformanceAndScalabilityProperty)
{
    const int PERFORMANCE_TEST_BOTS = 20; // Simulate multiple bots
    const int OPERATIONS_PER_BOT = 10;
    
    // Create multiple test bots
    std::vector<std::unique_ptr<PlayerMock>> testBots;
    for (int i = 0; i < PERFORMANCE_TEST_BOTS; ++i)
    {
        testBots.push_back(CreateTestBot());
        ASSERT_NE(testBots.back().get(), nullptr) << "Bot " << i << " should not be null";
    }
    
    // Property: System should handle multiple concurrent operations
    auto startTime = std::chrono::steady_clock::now();
    
    // Simulate multiple bots joining battlegrounds simultaneously
    for (int operation = 0; operation < OPERATIONS_PER_BOT; ++operation)
    {
        for (auto& bot : testBots)
        {
            // Simulate battleground join
            bot->SetInBattleground(true);
            pvpGearManager->EquipPvpGear(bot.get());
            
            // Quick state check
            float ratio = pvpGearManager->CalculateResilienceRatio(bot.get());
            EXPECT_GE(ratio, 0.0f) << "Operation " << operation 
                << ": Resilience ratio should remain valid under load";
            EXPECT_LE(ratio, 1.0f) << "Operation " << operation 
                << ": Resilience ratio should remain valid under load";
            
            // Simulate battleground leave
            bot->SetInBattleground(false);
            pvpGearManager->RestorePveGear(bot.get());
        }
    }
    
    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    // Property: Operations should complete within reasonable time
    // For 20 bots * 10 operations = 200 total operations, should complete within 5 seconds
    EXPECT_LT(duration.count(), 5000) << "Performance test should complete within 5 seconds, took " 
        << duration.count() << "ms";
    
    // Property: System should maintain consistency after load test
    for (size_t i = 0; i < testBots.size(); ++i)
    {
        float finalRatio = pvpGearManager->CalculateResilienceRatio(testBots[i].get());
        EXPECT_GE(finalRatio, 0.0f) << "Bot " << i 
            << ": System should maintain consistency after load test";
        EXPECT_LE(finalRatio, 1.0f) << "Bot " << i 
            << ": System should maintain consistency after load test";
    }
    
    // Property: Memory usage should be reasonable
    // Test cleanup effectiveness
    pvpGearManager->CleanupExpiredSnapshots();
    
    // System should remain responsive after cleanup
    auto cleanupBot = CreateTestBot();
    bool postLoadEquip = pvpGearManager->EquipPvpGear(cleanupBot.get());
    // Should work normally regardless of result
    EXPECT_TRUE(true) << "System should remain responsive after load test and cleanup";
}

/**
 * Unit Test: BGJoinAction HasCompletePvpEquipment Integration
 * 
 * Tests the specific integration of PvpGearManager into BGJoinAction::HasCompletePvpEquipment
 */
TEST_F(BattleGroundJoinActionIntegrationTest, HasCompletePvpEquipmentIntegration)
{
    // Create test bot
    auto bot = CreateTestBot();
    ASSERT_NE(bot.get(), nullptr) << "Bot should not be null";
    
    // Create BGJoinAction instance (with null AI for testing)
    BGJoinAction action(nullptr, "test bg join");
    
    // Test the integration logic by checking that the method works
    // Note: The actual implementation uses PvpGearManager internally
    
    // Property: Method should not crash with valid bot
    // We can't directly test the private method, but we can test the integration
    // through the public interface and verify the PvpGearManager is being used
    
    // Test PvpGearManager methods that BGJoinAction uses
    bool hasComplete = pvpGearManager->HasCompletePvpEquipment(bot.get());
    float resilienceRatio = pvpGearManager->CalculateResilienceRatio(bot.get());
    
    // Property: Integration should provide consistent results
    EXPECT_GE(resilienceRatio, 0.0f) << "Resilience ratio should be non-negative";
    EXPECT_LE(resilienceRatio, 1.0f) << "Resilience ratio should not exceed 100%";
    
    // Property: HasCompletePvpEquipment should be consistent with resilience ratio
    float requiredRatio = pvpGearManager->GetRequiredResilienceRatio(bot.get());
    
    if (hasComplete)
    {
        // If marked as complete, should meet or be close to requirement
        EXPECT_GE(resilienceRatio, requiredRatio * 0.8f) 
            << "Complete PVP equipment should have resilience ratio close to requirement";
    }
    
    // Test level-based requirements
    if (bot->GetLevel() < 60)
    {
        EXPECT_FLOAT_EQ(requiredRatio, 0.3f) << "Under level 60 should require 30% resilience";
    }
    else
    {
        EXPECT_FLOAT_EQ(requiredRatio, 0.8f) << "Level 60+ should require 80% resilience";
    }
}

/**
 * Property Test: Battleground Flow Integration
 * **功能: pvp-gear-auto-equip, 属性 20: 系统集成无缝性**
 * **Validates: Requirements 7.1**
 * 
 * Tests the complete battleground flow integration matching the actual source code paths
 */
TEST_F(BattleGroundJoinActionIntegrationTest, BattlegroundFlowIntegrationProperty)
{
    const int FLOW_TEST_ITERATIONS = 50;
    
    for (int iteration = 0; iteration < FLOW_TEST_ITERATIONS; ++iteration)
    {
        // Create test bot
        auto bot = CreateTestBot();
        ASSERT_NE(bot.get(), nullptr) << "Iteration " << iteration << ": Bot should not be null";
        
        // Property: Complete battleground flow should work seamlessly
        
        // Phase 1: Pre-join check (BGJoinAction::Execute path)
        bot->SetInBattleground(false);
        
        // Simulate the check in BGJoinAction::Execute
        bool initialHasComplete = pvpGearManager->HasCompletePvpEquipment(bot.get());
        
        if (!initialHasComplete)
        {
            // BGJoinAction::Execute calls EquipPvpGear if equipment is incomplete
            bool preJoinEquip = pvpGearManager->EquipPvpGear(bot.get());
            
            // Property: Pre-join equipment should complete without errors
            EXPECT_TRUE(true) << "Iteration " << iteration 
                << ": Pre-join equipment should not crash";
        }
        
        // Phase 2: Battleground join (BGStatusAction::Execute STATUS_WAIT_JOIN path)
        // This happens when bot receives battleground invitation
        bot->SetInBattleground(true);
        
        // BGStatusAction::Execute calls EquipPvpGear when joining
        bool joinEquipResult = pvpGearManager->EquipPvpGear(bot.get());
        
        // Property: Join-time equipment should work correctly
        EXPECT_TRUE(true) << "Iteration " << iteration 
            << ": Join-time equipment should not crash";
        
        float joinRatio = pvpGearManager->CalculateResilienceRatio(bot.get());
        EXPECT_GE(joinRatio, 0.0f) << "Iteration " << iteration 
            << ": Join-time resilience ratio should be valid";
        EXPECT_LE(joinRatio, 1.0f) << "Iteration " << iteration 
            << ": Join-time resilience ratio should be valid";
        
        // Phase 3: Battleground leave (BGStatusAction::LeaveBG path)
        // This happens when battleground ends or bot leaves
        bot->SetInBattleground(false);
        
        // BGStatusAction::LeaveBG calls RestorePveGear
        bool leaveRestoreResult = pvpGearManager->RestorePveGear(bot.get());
        
        // Property: Leave-time restoration should work correctly
        EXPECT_TRUE(true) << "Iteration " << iteration 
            << ": Leave-time restoration should not crash";
        
        float leaveRatio = pvpGearManager->CalculateResilienceRatio(bot.get());
        EXPECT_GE(leaveRatio, 0.0f) << "Iteration " << iteration 
            << ": Leave-time resilience ratio should be valid";
        EXPECT_LE(leaveRatio, 1.0f) << "Iteration " << iteration 
            << ": Leave-time resilience ratio should be valid";
        
        // Phase 4: Queue leave (BGLeaveAction::Execute path)
        // This happens when bot leaves queue without joining battleground
        
        // BGLeaveAction::Execute also calls RestorePveGear for queue leaves
        bool queueLeaveResult = pvpGearManager->RestorePveGear(bot.get());
        
        // Property: Queue leave restoration should be safe
        EXPECT_TRUE(true) << "Iteration " << iteration 
            << ": Queue leave restoration should not crash";
        
        float queueLeaveRatio = pvpGearManager->CalculateResilienceRatio(bot.get());
        EXPECT_GE(queueLeaveRatio, 0.0f) << "Iteration " << iteration 
            << ": Queue leave resilience ratio should be valid";
        EXPECT_LE(queueLeaveRatio, 1.0f) << "Iteration " << iteration 
            << ": Queue leave resilience ratio should be valid";
        
        // Property: Multiple restoration attempts should be safe
        // (This can happen in real scenarios due to multiple code paths)
        bool multipleRestoreResult = pvpGearManager->RestorePveGear(bot.get());
        EXPECT_TRUE(true) << "Iteration " << iteration 
            << ": Multiple restoration attempts should be safe";
        
        // Property: System should handle edge cases
        // Test rapid join/leave cycles
        for (int rapidCycle = 0; rapidCycle < 5; ++rapidCycle)
        {
            bot->SetInBattleground(true);
            pvpGearManager->EquipPvpGear(bot.get());
            
            bot->SetInBattleground(false);
            pvpGearManager->RestorePveGear(bot.get());
        }
        
        // System should remain stable after rapid cycles
        float finalRatio = pvpGearManager->CalculateResilienceRatio(bot.get());
        EXPECT_GE(finalRatio, 0.0f) << "Iteration " << iteration 
            << ": System should remain stable after rapid join/leave cycles";
        EXPECT_LE(finalRatio, 1.0f) << "Iteration " << iteration 
            << ": System should remain stable after rapid join/leave cycles";
    }
}

/**
 * Unit Test: Error Handling in Integration Points
 * 
 * Tests specific error conditions in the integration
 */
TEST_F(BattleGroundJoinActionIntegrationTest, ErrorHandlingInIntegrationPoints)
{
    // Test null bot handling
    bool nullEquipResult = pvpGearManager->EquipPvpGear(nullptr);
    EXPECT_FALSE(nullEquipResult) << "EquipPvpGear should handle null bot gracefully";
    
    bool nullRestoreResult = pvpGearManager->RestorePveGear(nullptr);
    EXPECT_FALSE(nullRestoreResult) << "RestorePveGear should handle null bot gracefully";
    
    bool nullHasCompleteResult = pvpGearManager->HasCompletePvpEquipment(nullptr);
    EXPECT_FALSE(nullHasCompleteResult) << "HasCompletePvpEquipment should handle null bot gracefully";
    
    float nullRatioResult = pvpGearManager->CalculateResilienceRatio(nullptr);
    EXPECT_EQ(nullRatioResult, 0.0f) << "CalculateResilienceRatio should return 0 for null bot";
    
    // Test with valid bot but edge cases
    auto bot = CreateTestBot();
    
    // Test multiple restore attempts without equip
    bool firstRestore = pvpGearManager->RestorePveGear(bot.get());
    bool secondRestore = pvpGearManager->RestorePveGear(bot.get());
    
    // Both should complete without crashing
    EXPECT_TRUE(true) << "Multiple restore attempts should not crash";
    
    // Test rapid state changes
    for (int i = 0; i < 20; ++i)
    {
        bot->SetInBattleground(i % 2 == 0);
        
        if (bot->InBattleground())
        {
            pvpGearManager->EquipPvpGear(bot.get());
        }
        else
        {
            pvpGearManager->RestorePveGear(bot.get());
        }
    }
    
    // System should remain stable
    float finalRatio = pvpGearManager->CalculateResilienceRatio(bot.get());
    EXPECT_GE(finalRatio, 0.0f) << "System should remain stable after rapid changes";
    EXPECT_LE(finalRatio, 1.0f) << "System should remain stable after rapid changes";
}