/**
 * =============================================================================
 * CS2Fixes
 * Copyright (C) 2023-2026 Source2ZE
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

#include "KeyValues.h"
#include "convar.h"
#include "cs2_sdk/entity/ccsweaponbase.h"
#include "ehandle.h"
#include <vector>
#undef max

extern CConVar<int> g_cvarMapMigrations20260121;

#define SF_DOOR_ONEWAY 16

class CMapMigrations
{
public:
	void PreLevelLoad(uint64 iWorkshopId);
	void ApplyGameSettings(uint64 iWorkshopId);
	void OnRoundPrestart();
	void OnEquipWeapon(CBasePlayerWeapon* pWeapon);
	bool Detour_CEntityIdentity_AcceptInput(CEntityIdentity* pThis, CUtlSymbolLarge* pInputName, CEntityInstance* pActivator, CEntityInstance* pCaller, variant_t* value);
	void RunMigrations(CUtlVector<CEntityKeyValues*>* pVecEntityKeyValues);
	void Migrations_Rendermode(CUtlVector<CEntityKeyValues*>* pVecEntityKeyValues);
	void Migrations_20260121(CUtlVector<CEntityKeyValues*>* pVecEntityKeyValues);
	void Migrations_20260420(CBasePlayerWeapon* pWeapon);
	bool Migrations_20260922(CEntityIdentity* pThis, CUtlSymbolLarge* pInputName, CEntityInstance* pActivator, CEntityInstance* pCaller, variant_t* value);
	bool Migrations20260420Enabled();
	void UpdateMapUpdateTime(uint64 iWorkshopId, time_t timeMapUpdated);

private:
	time_t m_timeMapUpdated = std::numeric_limits<time_t>::max();
	std::unordered_map<uint64, time_t> m_mapUpdateTimes;
	std::vector<CHandle<CBasePlayerWeapon>> m_vecEquippedWeapons;
};

extern CMapMigrations* g_pMapMigrations;