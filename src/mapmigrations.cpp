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
#include "entity/cbasemodelentity.h"
#include "utils.h"

CMapMigrations* g_pMapMigrations = nullptr;

const time_t g_time20260121 = 1769036239;

CConVar<int> g_cvarMapMigrations20260121("cs2f_mapmigrations_20260121", FCVAR_NONE, "Current mode for 2026-01-21 CS2 update map migrations. [0 = Force disabled, 1 = Force enabled, 2 = Automatically enabled for maps updated before 2026-01-21 & disabled if updated after]", 2);

void CMapMigrations::ApplyGameSettings(KeyValues* pKV)
{
	m_timeMapUpdated = std::numeric_limits<time_t>::max();

	// Don't run on default maps
	if (!pKV->FindKey("launchoptions") || !pKV->FindKey("launchoptions")->FindKey("customgamemode"))
		return;

	CMapMigrationWorkshopDetailsQuery::Create(pKV->FindKey("launchoptions")->GetUint64("customgamemode"));
}

void CMapMigrations::OnRoundPrestart()
{
	m_vecModelEntitiesUsingRendermodeEnum.clear();
}

void CMapMigrations::OnEntitySpawned(CEntityInstance* pEntity, const CEntityKeyValues* pKeyValues)
{
	CBaseEntity* pBaseEntity = (CBaseEntity*)pEntity;

	// Stupid workaround for CEntityKeyValues being inaccessible after entity spawn
	// We need access to this in 2026-01-21 rendermode migrations when called from UpdateMapUpdateTime
	if (pBaseEntity->AsBaseModelEntity() && V_StringToInt32(pKeyValues->GetString("rendermode"), -1, NULL, NULL, PARSING_FLAG_SKIP_WARNING) == -1)
		m_vecModelEntitiesUsingRendermodeEnum.push_back(pBaseEntity->GetHandle());

	RunMigrations(pBaseEntity);
}

void CMapMigrations::RunMigrations(CBaseEntity* pEntity)
{
	if (g_cvarMapMigrations20260121.Get() == 1 || (g_cvarMapMigrations20260121.Get() == 2 && m_timeMapUpdated < g_time20260121))
		Migrations_20260121(pEntity);
}

void CMapMigrations::Migrations_20260121(CBaseEntity* pEntity)
{
	if (!V_strcasecmp(pEntity->GetClassname(), "func_door_rotating"))
	{
		uint32 spawnFlags = pEntity->m_spawnflags();

		if (!(spawnFlags & SF_DOOR_ONEWAY))
			pEntity->m_spawnflags = spawnFlags + SF_DOOR_ONEWAY;
	}

	CBaseModelEntity* pModelEntity = pEntity->AsBaseModelEntity();

	if (pModelEntity)
	{
		// Also need to make sure the entity is using index-based rendermodes, and not enum-based ones (which will already automatically migrate correctly)
		// This differentiation is lost after entity spawn, so we had to check the original keyvalue earlier instead
		for (int i = 0; i < m_vecModelEntitiesUsingRendermodeEnum.size(); i++)
			if (m_vecModelEntitiesUsingRendermodeEnum[i] == pModelEntity->GetHandle())
				return;

		RenderMode_t renderMode = pModelEntity->m_nRenderMode();

		// Legacy kRenderTransAlpha
		if (renderMode == 4)
			pModelEntity->m_nRenderMode = kRenderTransAlpha;
		// Legacy kRenderNone
		else if (renderMode == 10)
			pModelEntity->m_nRenderMode = kRenderNone;
		// All other removed render modes, fall back to normal
		else if (renderMode > kRenderNormal)
			pModelEntity->m_nRenderMode = kRenderNormal;
	}
}

void CMapMigrations::UpdateMapUpdateTime(time_t timeMapUpdated)
{
	m_timeMapUpdated = timeMapUpdated;

	CBaseEntity* pTarget = nullptr;

	// May be called late, so also check any existing entities first
	while ((pTarget = UTIL_FindEntityByClassname(pTarget, "*")))
		RunMigrations(pTarget);
}

std::shared_ptr<CMapMigrationWorkshopDetailsQuery> CMapMigrationWorkshopDetailsQuery::Create(uint64 iWorkshopId)
{
	if (!GetSteamUGC())
	{
		Panic("Map migrations failed to find current map update time: null ISteamUGC\n");
		return nullptr;
	}

	uint64 iWorkshopIDArray[1] = {iWorkshopId};
	UGCQueryHandle_t hQuery = GetSteamUGC()->CreateQueryUGCDetailsRequest(iWorkshopIDArray, 1);

	if (hQuery == k_UGCQueryHandleInvalid)
	{
		Panic("Map migrations failed to find current map update time: failed to query workshop map information for ID %llu\n", iWorkshopId);
		return nullptr;
	}

	GetSteamUGC()->SetAllowCachedResponse(hQuery, 0);
	SteamAPICall_t hCall = GetSteamUGC()->SendQueryUGCRequest(hQuery);

	auto pQuery = std::make_shared<CMapMigrationWorkshopDetailsQuery>(hQuery, iWorkshopId);
	g_pMapMigrations->AddWorkshopDetailsQuery(pQuery);
	pQuery->m_CallResult.Set(hCall, pQuery.get(), &CMapMigrationWorkshopDetailsQuery::OnQueryCompleted);

	return pQuery;
}

void CMapMigrationWorkshopDetailsQuery::OnQueryCompleted(SteamUGCQueryCompleted_t* pCompletedQuery, bool bFailed)
{
	SteamUGCDetails_t details;

	if (bFailed || pCompletedQuery->m_eResult != k_EResultOK || pCompletedQuery->m_unNumResultsReturned < 1 || !GetSteamUGC()->GetQueryUGCResult(pCompletedQuery->m_handle, 0, &details) || details.m_eResult != k_EResultOK)
		Panic("Map migrations failed to find current map update time: failed to query workshop map information for ID %llu\n", m_iWorkshopId);
	else
		g_pMapMigrations->UpdateMapUpdateTime(details.m_rtimeUpdated);

	if (GetSteamUGC())
		GetSteamUGC()->ReleaseQueryUGCRequest(m_hQuery);

	g_pMapMigrations->RemoveWorkshopDetailsQuery(shared_from_this());
}