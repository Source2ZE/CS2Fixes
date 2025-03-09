/**
 * =============================================================================
 * CS2Fixes
 * Copyright (C) 2023-2025 Source2ZE
 * =============================================================================
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License, version 3.0, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "entity/ccsweaponbase.h"
#include <cstdint>

struct WeaponInfo_t
{
	int16_t m_iItemDefinitionIndex;
	int8_t m_iTeamNum;
	gear_slot_t m_eSlot;
	int32_t m_nPrice;
	int32_t m_nMaxAmount;
	const char* m_pClass;
	const char* m_pName;
	std::vector<std::string> m_vecAliases;

	WeaponInfo_t(const char* classname, int16_t defIndex, int8_t team, gear_slot_t slot, int32_t price = 0, const char* name = nullptr, const std::vector<std::string>& aliases = {}, int32_t maxAmount = 0) :
		m_iItemDefinitionIndex(defIndex), m_iTeamNum(team), m_eSlot(slot), m_nPrice(price),
		m_nMaxAmount(maxAmount), m_pClass(classname), m_pName(name), m_vecAliases(aliases)
	{}
};

const WeaponInfo_t* FindWeaponInfoByClass(const char* pClass);
const WeaponInfo_t* FindWeaponInfoByClassCaseInsensitive(const char* pClass);
const WeaponInfo_t* FindWeaponInfoByAlias(const char* pAlias);

std::vector<std::pair<std::string, std::vector<std::string>>> GenerateWeaponCommands();