-- DB update 2025_12_16_02 -> 2025_12_17_00
--
-- Faerie Fire group
-- SPELL_GROUP_STACK_RULE_EXCLUSIVE_SAME_EFFECT	'Same effects of spells will not stack, yet auras will remain on a target'
UPDATE `spell_group_stack_rules` SET `stack_rule`=3 WHERE `group_id`=1016;
-- DB update 2025_12_17_00 -> 2025_12_17_01
UPDATE `creature_template` SET `unit_flags` = 2147746560 WHERE `entry` IN (34952, 34953);

