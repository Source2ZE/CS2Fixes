/**
 * =============================================================================
 * CS2Fixes
 * Copyright (C) 2023-2024 Source2ZE
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
#include "utils/entity.h"
#include "playermanager.h"
#include "gamesystem.h"
#include "igameevents.h"

struct LeaderColor
{
	const char* pszColorName;
	Color clColor;
};

extern LeaderColor LeaderColorMap[];
extern const size_t g_nLeaderColorMapSize;
extern CUtlVector<ZEPlayerHandle> g_vecLeaders;
extern int g_iLeaderIndex;

extern bool g_bEnableLeader;

bool Leader_NoLeaders();
void Leader_ApplyLeaderVisuals(CCSPlayerPawn *pPawn);
void Leader_PostEventAbstract_Source1LegacyGameEvent(const uint64 *clients, const void* pData);
void Leader_OnRoundStart(IGameEvent *pEvent);
void Leader_BulletImpact(IGameEvent *pEvent);
void Leader_Precache(IEntityResourceManifest *pResourceManifest);