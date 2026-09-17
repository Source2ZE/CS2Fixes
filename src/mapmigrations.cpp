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

#include "mapmigrations.h"
#include "cs2fixes.h"
#include "entity.h"
#include "entity/cbasetoggle.h"
#include "map_votes.h"
#include "utils.h"
#include "vprof.h"

CMapMigrations* g_pMapMigrations = nullptr;

const time_t g_time20260121 = 1769036239;
const time_t g_time20260420 = 1776725888;

CConVar<int> g_cvarMapMigrations20260121("cs2f_mapmigrations_20260121", FCVAR_NONE, "Current mode for 2026-01-21 CS2 update map migrations. [0 = Force disabled, 1 = Force enabled, 2 = Automatically enabled for maps updated before 2026-01-21 & disabled if updated after]", 2);
CConVar<int> g_cvarMapMigrations20260420("cs2f_mapmigrations_20260420", FCVAR_NONE, "Current mode for 2026-04-20 CS2 update map migrations. [0 = Force disabled, 1 = Force enabled, 2 = Automatically enabled for maps updated before 2026-04-20 & disabled if updated after]", 2);

void CMapMigrations::PreLevelLoad(uint64 iWorkshopId)
{
	m_timeMapUpdated = std::numeric_limits<time_t>::max();

	// Don't run on default maps
	if (iWorkshopId != 0)
	{
		if (m_mapUpdateTimes.contains(iWorkshopId))
		{
			m_timeMapUpdated = m_mapUpdateTimes[iWorkshopId];
		}
		else
		{
			Message("Skipping pre-load map migrations for %llu, update time is not available\n", iWorkshopId);

			// Try to get the update time anyways, for later migrations
			CMapSystemWorkshopDetailsQuery::Create(iWorkshopId);
		}
	}
}

void CMapMigrations::OnRoundPrestart()
{
	m_vecEquippedWeapons.clear();
}

void CMapMigrations::OnEquipWeapon(CBasePlayerWeapon* pWeapon)
{
	if (Migrations20260420Enabled())
		Migrations_20260420(pWeapon);
}

void CMapMigrations::RunMigrations(CUtlVector<CEntityKeyValues*>* pVecEntityKeyValues)
{
	if (g_cvarMapMigrations20260121.Get() > 0)
		Migrations_Rendermode(pVecEntityKeyValues);

	if (g_cvarMapMigrations20260121.Get() == 1 || (g_cvarMapMigrations20260121.Get() == 2 && m_timeMapUpdated < g_time20260121))
		Migrations_20260121(pVecEntityKeyValues);
}

void CMapMigrations::Migrations_Rendermode(CUtlVector<CEntityKeyValues*>* pVecEntityKeyValues)
{
	FOR_EACH_VEC(*pVecEntityKeyValues, i)
	{
		auto pKeyValues = (*pVecEntityKeyValues)[i];

		if (!pKeyValues->HasValue("rendermode"))
			continue;

		int renderMode = V_StringToInt32(pKeyValues->GetString("rendermode"), -1, NULL, NULL, PARSING_FLAG_SKIP_WARNING);

		// Enum-named render modes already migrate correctly
		if (renderMode == -1)
			continue;

		if (renderMode == 4)
			pKeyValues->SetString("rendermode", "kRenderTransAlpha");
		else if (renderMode == 10)
			pKeyValues->SetString("rendermode", "kRenderNone");
		// All other removed render modes, fall back to normal
		else if (renderMode > kRenderNormal)
			pKeyValues->SetString("rendermode", "kRenderNormal");
	}
}

void CMapMigrations::Migrations_20260121(CUtlVector<CEntityKeyValues*>* pVecEntityKeyValues)
{
	FOR_EACH_VEC(*pVecEntityKeyValues, i)
	{
		auto pKeyValues = (*pVecEntityKeyValues)[i];

		if (!V_strcasecmp(pKeyValues->GetString("classname"), "func_door_rotating") && pKeyValues->HasValue("spawnflags"))
		{
			uint32 spawnFlags = pKeyValues->GetUint("spawnflags");
			pKeyValues->SetUint("spawnflags", spawnFlags | SF_DOOR_ONEWAY);
		}
	}
}

void CMapMigrations::Migrations_20260420(CBasePlayerWeapon* pWeapon)
{
	VPROF("CMapMigrations::Migrations_20260420");

	// We only care about map-spawned weapons
	if (!V_strcmp(pWeapon->m_sUniqueHammerID().Get(), ""))
		return;

	// And only their first equip
	for (int i = 0; i < m_vecEquippedWeapons.size(); i++)
		if (m_vecEquippedWeapons[i] == pWeapon->GetHandle())
			return;

	m_vecEquippedWeapons.push_back(pWeapon->GetHandle());
	CBaseEntity* pTarget = nullptr;

	// Entities parented to weapons being held by players were offset by +40 units following the AG2 update
	// Since that doesn't affect in-world weapons, this migration is delayed until weapon equip to prevent breaking strip triggers etc
	// Alternatively, we may want to track weapon children ahead of time via OnEntityParentChanged if this ends up becoming a performance concern
	while ((pTarget = UTIL_FindEntityByClassname(pTarget, "*")))
	{
		CGameSceneNode* pParentSceneNode = pTarget->m_CBodyComponent()->m_pSceneNode()->m_pParent();

		if (pParentSceneNode && pParentSceneNode->m_pOwner() == pWeapon)
		{
			Vector newOrigin = pTarget->GetAbsOrigin();
			const char* pszClass = pTarget->GetClassname();

			newOrigin.z -= 40.0f;
			pTarget->Teleport(&newOrigin, nullptr, nullptr);

			// If child inherits CBaseToggle, there's further bullshit we have to offset
			// There's also some more obscure entities + all of CBaseTrigger, but these seem unnecessary to fixup
			if (!V_strcasecmp(pszClass, "func_button") || !V_strcasecmp(pszClass, "func_physical_button") || !V_strcasecmp(pszClass, "func_rot_button") || !V_strcasecmp(pszClass, "momentary_rot_button") || !V_strcasecmp(pszClass, "func_movelinear") || !V_strcasecmp(pszClass, "func_door") || !V_strcasecmp(pszClass, "func_door_rotating"))
			{
				CBaseToggle* pToggle = (CBaseToggle*)pTarget;

				pToggle->m_vecPosition1().z -= 40.0f;
				pToggle->m_vecPosition2().z -= 40.0f;
			}
		}
	}
}

bool CMapMigrations::Migrations20260420Enabled()
{
	return g_cvarMapMigrations20260420.Get() == 1 || (g_cvarMapMigrations20260420.Get() == 2 && m_timeMapUpdated < g_time20260420);
}

void CMapMigrations::UpdateMapUpdateTime(uint64 iWorkshopId, time_t timeMapUpdated)
{
	m_mapUpdateTimes[iWorkshopId] = timeMapUpdated;

	// If we get triggered through PreLevelLoad
	if (g_pMapVoteSystem->GetCurrentMap() && g_pMapVoteSystem->GetCurrentMap()->GetWorkshopId() == iWorkshopId)
		m_timeMapUpdated = timeMapUpdated;
}