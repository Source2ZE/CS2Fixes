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
#include "adminsystem.h"
#include "gamesystem.h"
#include "igameevents.h"
#include "playermanager.h"
#include "utils/entity.h"

// Inside IsAdminFlagSet, checks for ADMFLAG_GENERIC.
// Inside command permissions, checks for if leader system is enabled and player is leader
// OR if player has ADMFLAG_GENERIC
#define FLAG_LEADER (ADMFLAG_GENERIC | 1 << 31)

struct ColorPreset
{
	std::string strChatColor;
	Color color;
	bool bIsChatColor;
};

extern std::map<std::string, ColorPreset> mapColorPresets;
extern CUtlVector<ZEPlayerHandle> g_vecLeaders;

extern CConVar<bool> g_cvarEnableLeader;
extern CConVar<bool> g_cvarLeaderActionsHumanOnly;
extern CConVar<CUtlString> g_cvarMarkParticlePath;

void Leader_ApplyLeaderVisuals(CCSPlayerPawn* pPawn);
void Leader_PostEventAbstract_Source1LegacyGameEvent(const uint64* clients, const CNetMessage* pData);
void Leader_OnRoundStart(IGameEvent* pEvent);
void Leader_BulletImpact(IGameEvent* pEvent);
void Leader_Precache(IEntityResourceManifest* pResourceManifest);