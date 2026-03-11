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
#include "ehandle.h"
#include "entitysystem.h"
#include "steam/isteamugc.h"
#include <vector>
#undef max

extern CConVar<int> g_cvarMapMigrations20260121;

#define SF_DOOR_ONEWAY 16

class CMapMigrationWorkshopDetailsQuery : public std::enable_shared_from_this<CMapMigrationWorkshopDetailsQuery>
{
public:
	CMapMigrationWorkshopDetailsQuery(UGCQueryHandle_t hQuery, uint64 iWorkshopId) :
		m_hQuery(hQuery), m_iWorkshopId(iWorkshopId)
	{}

	static std::shared_ptr<CMapMigrationWorkshopDetailsQuery> Create(uint64 iWorkshopId);

private:
	void OnQueryCompleted(SteamUGCQueryCompleted_t* pCompletedQuery, bool bFailed);

	UGCQueryHandle_t m_hQuery;
	CCallResult<CMapMigrationWorkshopDetailsQuery, SteamUGCQueryCompleted_t> m_CallResult;
	uint64 m_iWorkshopId;
};

class CMapMigrations
{
public:
	void ApplyGameSettings(KeyValues* pKV);
	void OnRoundPrestart();
	void OnEntitySpawned(CEntityInstance* pEntity, const CEntityKeyValues* pKeyValues);
	void RunMigrations(CBaseEntity* pEntity);
	void Migrations_20260121(CBaseEntity* pEntity);
	void UpdateMapUpdateTime(time_t timeMapUpdated);
	void AddWorkshopDetailsQuery(std::shared_ptr<CMapMigrationWorkshopDetailsQuery> pQuery) { m_vecWorkshopDetailsQueries.push_back(pQuery); }
	void RemoveWorkshopDetailsQuery(std::shared_ptr<CMapMigrationWorkshopDetailsQuery> pQuery) { m_vecWorkshopDetailsQueries.erase(std::remove(m_vecWorkshopDetailsQueries.begin(), m_vecWorkshopDetailsQueries.end(), pQuery), m_vecWorkshopDetailsQueries.end()); }

private:
	time_t m_timeMapUpdated = std::numeric_limits<time_t>::max();
	std::vector<std::shared_ptr<CMapMigrationWorkshopDetailsQuery>> m_vecWorkshopDetailsQueries;
	std::vector<CHandle<CBaseEntity>> m_vecModelEntitiesUsingRendermodeEnum;
};

extern CMapMigrations* g_pMapMigrations;