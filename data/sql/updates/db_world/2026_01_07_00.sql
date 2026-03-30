
DELETE FROM `smart_scripts` WHERE (`source_type` = 0 AND `entryorguid` = 30134);
INSERT INTO `smart_scripts` (`entryorguid`, `source_type`, `id`, `link`, `event_type`, `event_phase_mask`, `event_chance`, `event_flags`, `event_param1`, `event_param2`, `event_param3`, `event_param4`, `event_param5`, `event_param6`, `action_type`, `action_param1`, `action_param2`, `action_param3`, `action_param4`, `action_param5`, `action_param6`, `target_type`, `target_param1`, `target_param2`, `target_param3`, `target_param4`, `target_x`, `target_y`, `target_z`, `target_o`, `comment`) VALUES
(30134, 0, 0, 13, 27, 0, 100, 512, 0, 0, 0, 0, 0, 0, 53, 2, 30134, 0, 0, 500, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, 'Brann\'s Flying Machine - On Passenger Boarded - Start Waypoint Path 30134'),
(30134, 0, 1, 0, 28, 0, 100, 512, 0, 0, 0, 0, 0, 0, 41, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 'Brann\'s Flying Machine - On Passenger Removed - Despawn Instant'),
(30134, 0, 2, 0, 40, 0, 100, 512, 2, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 19, 30107, 5, 0, 0, 0, 0, 0, 0, 'Brann\'s Flying Machine - On Point 2 of Path Any Reached - Say Line 0'),
(30134, 0, 3, 0, 40, 0, 100, 512, 8, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 19, 30107, 5, 0, 0, 0, 0, 0, 0, 'Brann\'s Flying Machine - On Point 8 of Path Any Reached - Say Line 1'),
(30134, 0, 4, 0, 40, 0, 100, 512, 16, 0, 0, 0, 0, 0, 1, 2, 0, 0, 0, 0, 0, 19, 30107, 5, 0, 0, 0, 0, 0, 0, 'Brann\'s Flying Machine - On Point 16 of Path Any Reached - Say Line 2'),
(30134, 0, 5, 0, 40, 0, 100, 512, 22, 0, 0, 0, 0, 0, 1, 3, 0, 0, 0, 0, 0, 19, 30107, 5, 0, 0, 0, 0, 0, 0, 'Brann\'s Flying Machine - On Point 22 of Path Any Reached - Say Line 3'),
(30134, 0, 6, 0, 40, 0, 100, 512, 37, 0, 0, 0, 0, 0, 1, 4, 0, 0, 0, 0, 0, 19, 30107, 5, 0, 0, 0, 0, 0, 0, 'Brann\'s Flying Machine - On Point 37 of Path Any Reached - Say Line 4'),
(30134, 0, 7, 0, 40, 0, 100, 512, 47, 0, 0, 0, 0, 0, 1, 5, 0, 0, 0, 0, 0, 19, 30107, 5, 0, 0, 0, 0, 0, 0, 'Brann\'s Flying Machine - On Point 47 of Path Any Reached - Say Line 5'),
(30134, 0, 8, 0, 40, 0, 100, 512, 53, 0, 0, 0, 0, 0, 1, 6, 0, 0, 0, 0, 0, 19, 30107, 5, 0, 0, 0, 0, 0, 0, 'Brann\'s Flying Machine - On Point 53 of Path Any Reached - Say Line 6'),
(30134, 0, 9, 0, 40, 0, 100, 512, 57, 0, 0, 0, 0, 0, 1, 7, 0, 0, 0, 0, 0, 19, 30107, 5, 0, 0, 0, 0, 0, 0, 'Brann\'s Flying Machine - On Point 57 of Path Any Reached - Say Line 7'),
(30134, 0, 10, 0, 40, 0, 100, 512, 65, 0, 0, 0, 0, 0, 1, 8, 0, 0, 0, 0, 0, 19, 30107, 5, 0, 0, 0, 0, 0, 0, 'Brann\'s Flying Machine - On Point 65 of Path Any Reached - Say Line 8'),
(30134, 0, 11, 12, 40, 0, 100, 512, 72, 0, 0, 0, 0, 0, 28, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 'Brann\'s Flying Machine - On Point 72 of Path Any Reached - Remove Auras'),
(30134, 0, 12, 0, 61, 0, 100, 512, 0, 0, 0, 0, 0, 0, 11, 56675, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 'Brann\'s Flying Machine - On Point 72 of Path Any Reached - Cast \'Summon Brann Bronzebeard\''),
(30134, 0, 13, 14, 61, 0, 100, 512, 0, 0, 0, 0, 0, 0, 18, 16777216, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 'Brann\'s Flying Machine - On Passenger Boarded - Set Flags Player Controlled'),
(30134, 0, 14, 15, 61, 0, 100, 512, 0, 0, 0, 0, 0, 0, 20, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 'Brann\'s Flying Machine - On Passenger Boarded - Stop Attacking'),
(30134, 0, 15, 0, 61, 0, 100, 512, 0, 0, 0, 0, 0, 0, 44, 7, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 'Brann\'s Flying Machine - On Passenger Boarded - Set PhaseMask 7'),
(30134, 0, 16, 0, 60, 0, 100, 0, 3600, 3600, 3600, 3600, 0, 0, 86, 55089, 1, 19, 30136, 40, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 'Brann\'s Flying Machine - On Update, Condition: Not Currently Boarded - Cross Cast \'Mount Brann`s Flying Machine\'');

DELETE FROM `conditions` WHERE (`SourceTypeOrReferenceId` = 22) AND (`SourceGroup` = 17) AND (`SourceEntry` = 30134) AND (`SourceId` = 0) AND (`ElseGroup` = 0) AND (`ConditionTypeOrReference` = 29) AND (`ConditionTarget` = 1) AND (`ConditionValue1` = 30136) AND (`ConditionValue2` = 5) AND (`ConditionValue3` = 0);
INSERT INTO `conditions` (`SourceTypeOrReferenceId`, `SourceGroup`, `SourceEntry`, `SourceId`, `ElseGroup`, `ConditionTypeOrReference`, `ConditionTarget`, `ConditionValue1`, `ConditionValue2`, `ConditionValue3`, `NegativeCondition`, `ErrorType`, `ErrorTextId`, `ScriptName`, `Comment`) VALUES
(22, 17, 30134, 0, 0, 29, 1, 30136, 5, 0, 1, 0, 0, '', 'Only allow boarding if there are no current boarders for quest Bronzebeard Brothers');

DELETE FROM `smart_scripts` WHERE (`source_type` = 0 AND `entryorguid` = 30136);
INSERT INTO `smart_scripts` (`entryorguid`, `source_type`, `id`, `link`, `event_type`, `event_phase_mask`, `event_chance`, `event_flags`, `event_param1`, `event_param2`, `event_param3`, `event_param4`, `event_param5`, `event_param6`, `action_type`, `action_param1`, `action_param2`, `action_param3`, `action_param4`, `action_param5`, `action_param6`, `target_type`, `target_param1`, `target_param2`, `target_param3`, `target_param4`, `target_x`, `target_y`, `target_z`, `target_o`, `comment`) VALUES
(30136, 0, 0, 0, 6, 0, 100, 0, 0, 0, 0, 0, 0, 0, 203, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 'Stormforged Soldier - On Just Died - Exit Vehicle'),
(30136, 0, 1, 0, 10, 0, 100, 0, 0, 100, 10000, 10000, 0, 0, 49, 0, 0, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, 'Stormforged Soldier - Within 0-100 Range Out of Combat LoS - Start Attacking'),
(30136, 0, 2, 0, 0, 0, 25, 1, 0, 500, 0, 0, 0, 0, 11, 56621, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 'Stormforged Soldier - In Combat - Cast \'Thunder Orb\' Hit (No Repeat)'),
(30136, 0, 3, 0, 0, 0, 100, 0, 3000, 5000, 4000, 6000, 0, 0, 11, 56622, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 'Stormforged Soldier - In Combat - Cast \'Thunder Orb\' Miss');

-- DELETE FROM `npc_spellclick_spells` WHERE `npc_entry` = 30134 AND `spell_id` IN (52391, 55089); -- Breaking Brann, I think
DELETE FROM `conditions` WHERE (`SourceTypeOrReferenceId` = 13) AND (`SourceGroup` = 1) AND (`SourceEntry` = 55089) AND (`SourceId` = 0) AND (`ElseGroup` = 0) AND (`ConditionTypeOrReference` = 31) AND (`ConditionTarget` = 0) AND (`ConditionValue1` = 3) AND (`ConditionValue2` = 30134) AND (`ConditionValue3` = 0);
INSERT INTO `conditions` (`SourceTypeOrReferenceId`, `SourceGroup`, `SourceEntry`, `SourceId`, `ElseGroup`, `ConditionTypeOrReference`, `ConditionTarget`, `ConditionValue1`, `ConditionValue2`, `ConditionValue3`, `NegativeCondition`, `ErrorType`, `ErrorTextId`, `ScriptName`, `Comment`) VALUES
(13, 1, 55089, 0, 0, 31, 0, 3, 30134, 0, 0, 0, 0, '', '55089 \'Mount Brann\'s Flying Machine\' Targets Brann\'s Flying Machine (30134)');

-- DB update 2026_01_07_00 -> 2026_01_07_01
--
UPDATE `quest_template_addon` SET `SpecialFlags` = `SpecialFlags`|1 WHERE (`ID` = 13845);
SET @entry             := 1000003;
SET @CreatureDisplayID := 3280;
SET @name              := 'Arena 3v3 Solo';

DELETE FROM `creature_template` WHERE `entry` = @entry;
INSERT INTO `creature_template` (`entry`, `difficulty_entry_1`, `difficulty_entry_2`, `difficulty_entry_3`, `KillCredit1`, `KillCredit2`, `name`, `subname`, `IconName`, `gossip_menu_id`, `minlevel`, `maxlevel`, `exp`, `faction`, `npcflag`, `speed_walk`, `speed_run`, `speed_swim`, `speed_flight`, `detection_range`, `scale`, `rank`, `dmgschool`, `DamageModifier`, `BaseAttackTime`, `RangeAttackTime`, `BaseVariance`, `RangeVariance`, `unit_class`, `unit_flags`, `unit_flags2`, `dynamicflags`, `family`, `type`, `type_flags`, `lootid`, `pickpocketloot`, `skinloot`, `PetSpellDataId`, `VehicleId`, `mingold`, `maxgold`, `AIName`, `MovementType`, `HoverHeight`, `HealthModifier`, `ManaModifier`, `ArmorModifier`, `ExperienceModifier`, `RacialLeader`, `movementId`, `RegenHealth`, `mechanic_immune_mask`, `spell_school_immune_mask`, `flags_extra`, `ScriptName`, `VerifiedBuild`) VALUES
(@entry, 0, 0, 0, 0, 0, @name, '', '', 0, 80, 80, 2, 35, 1, 1, 1.14286, 1, 1, 20, 1, 1, 0, 1, 1, 1, 1, 1, 1, 131078, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, '', 0, 1, 1, 1, 1, 1, 0, 0, 1, 138936390, 0, 0, 'npc_solo3v3', '12340');

DELETE FROM `creature_template_model` WHERE `CreatureID` = @entry;
INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`) VALUES
(@entry, 0, @CreatureDisplayID, 1, 1, 0);

-- npc text
SET @npc_text_3v3_soloq := "This NPC enables you to join 3v3soloQ unrated arenas, create a 3v3soloQ arena team, and join 3v3soloQ rated arenas.$B$BIf you prefer not to interact with the NPC, you can use the following commands to join or create the arena:$B$B.qsolo rated$B$B.qsolo unrated$B$BNote that if you don’t already have a 3v3soloQ arena team, using .qsolo rated will automatically create one for you.";
DELETE FROM `npc_text` WHERE `ID` = 1000004;
INSERT INTO `npc_text` (`id`, `text0_0`, `text0_1`, `Probability0`) VALUES
(1000004, @npc_text_3v3_soloq, @npc_text_3v3_soloq, 1);

-- Command
DELETE FROM `command` WHERE `name` IN ('qsolo', 'qsolo rated', 'qsolo unrated', 'testqsolo');
INSERT INTO `command` (`name`, `security`, `help`) VALUES
('qsolo', 0, '.qsolo rated/unrated\nJoin arena 3v3soloQ rated or unrated'),
('qsolo rated', 0, 'Syntax .qsolo rated\nJoin arena 3v3soloQ rated'),
('qsolo unrated', 0, 'Syntax .qsolo unrated\nJoin arena 3v3soloQ unrated'),
('testqsolo', 4, '.testqsolo -> join arena 3v3soloQ for testing');

-- DB update 2026_01_07_01 -> 2026_01_07_02
--
DELETE FROM `conditions` WHERE (`SourceTypeOrReferenceId` = 13) AND (`SourceGroup` = 7) AND (`SourceEntry` = 54469) AND (`SourceId` = 0) AND (`ElseGroup` = 0) AND (`ConditionTypeOrReference` = 31) AND (`ConditionTarget` = 0) AND (`ConditionValue1` = 3) AND (`ConditionValue2` = 29333) AND (`ConditionValue3` = 0);
INSERT INTO `conditions` (`SourceTypeOrReferenceId`, `SourceGroup`, `SourceEntry`, `SourceId`, `ElseGroup`, `ConditionTypeOrReference`, `ConditionTarget`, `ConditionValue1`, `ConditionValue2`, `ConditionValue3`, `NegativeCondition`, `ErrorType`, `ErrorTextId`, `ScriptName`, `Comment`) VALUES
(13, 7, 54469, 0, 0, 31, 0, 3, 29333, 0, 0, 0, 0, '', 'Only useful versus Onslaught Gryphon Rider.');

DELETE FROM `conditions` WHERE (`SourceTypeOrReferenceId` = 13) AND (`SourceGroup` = 1) AND (`SourceEntry` = 74541) AND (`SourceId` = 0) AND (`ElseGroup` = 0) AND (`ConditionTypeOrReference` = 31) AND (`ConditionTarget` = 0) AND (`ConditionValue1` = 3) AND (`ConditionValue2` = 29333) AND (`ConditionValue3` = 0);
INSERT INTO `conditions` (`SourceTypeOrReferenceId`, `SourceGroup`, `SourceEntry`, `SourceId`, `ElseGroup`, `ConditionTypeOrReference`, `ConditionTarget`, `ConditionValue1`, `ConditionValue2`, `ConditionValue3`, `NegativeCondition`, `ErrorType`, `ErrorTextId`, `ScriptName`, `Comment`) VALUES
(13, 1, 74541, 0, 0, 31, 0, 3, 29333, 0, 0, 0, 0, '', 'Only useful versus Onslaught Gryphon Rider.');
