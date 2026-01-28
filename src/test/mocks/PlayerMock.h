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

#ifndef AZEROTHCORE_PLAYERMOCK_H
#define AZEROTHCORE_PLAYERMOCK_H

#include "Common.h"
#include "ObjectGuid.h"
#include "SharedDefines.h"
#include <string>

// Forward declarations
class Item;

/**
 * Mock Player class for testing purposes
 * Provides minimal implementation of Player interface needed for PvpGearManager tests
 */
class PlayerMock
{
public:
    PlayerMock() 
        : m_level(1)
        , m_class(CLASS_WARRIOR)
        , m_guid(ObjectGuid::Empty)
        , m_name("MockPlayer")
        , m_inBattleground(false)
    {
    }

    virtual ~PlayerMock() = default;

    // Level management
    uint8 GetLevel() const { return m_level; }
    void SetLevel(uint8 level) { m_level = level; }

    // Class management
    uint8 getClass() const { return m_class; }
    void SetClass(uint8 playerClass) { m_class = playerClass; }

    // GUID management
    ObjectGuid GetGUID() const { return m_guid; }
    void SetGUID(ObjectGuid guid) { m_guid = guid; }

    // Name management
    std::string GetName() const { return m_name; }
    void SetName(const std::string& name) { m_name = name; }

    // Battleground status
    bool InBattleground() const { return m_inBattleground; }
    void SetInBattleground(bool inBG) { m_inBattleground = inBG; }
    
    // Arena status (simplified)
    bool InArena() const { return m_inBattleground; } // Simplified for testing

    // Item management (simplified for testing)
    Item* GetItemByPos(uint8 bag, uint8 slot) const 
    { 
        // Return nullptr for mock - actual implementation would return real items
        return nullptr; 
    }

    Item* GetItemByPos(uint16 pos) const 
    { 
        // Return nullptr for mock - actual implementation would return real items
        return nullptr; 
    }

private:
    uint8 m_level;
    uint8 m_class;
    ObjectGuid m_guid;
    std::string m_name;
    bool m_inBattleground;
};

#endif // AZEROTHCORE_PLAYERMOCK_H