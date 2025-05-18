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

#include "gamesystem.h"
#include "addresses.h"
#include "adminsystem.h"
#include "common.h"
#include "entities.h"
#include "entity/cgamerules.h"
#include "gameconfig.h"
#include "idlemanager.h"
#include "leader.h"
#include "playermanager.h"
#include "tier0/vprof.h"
#include "zombiereborn.h"

#include "tier0/memdbgon.h"

extern CGlobalVars* GetGlobals();
extern CGameConfig* g_GameConfig;
extern CCSGameRules* g_pGameRules;

CBaseGameSystemFactory** CBaseGameSystemFactory::sm_pFirst = nullptr;

CGameSystem g_GameSystem;
CGameSystemStaticCustomFactory<CGameSystem>* CGameSystem::sm_Factory = nullptr;

// This mess is needed to get the pointer to sm_pFirst so we can insert game systems
bool InitGameSystems()
{
	// This signature directly points to the instruction referencing sm_pFirst, and the opcode is 3 bytes so we skip those
	uint8* ptr = (uint8*)g_GameConfig->ResolveSignature("IGameSystem_InitAllSystems_pFirst") + 3;

	if (!ptr)
	{
		Panic("Failed to InitGameSystems, see warnings above.\n");
		return false;
	}

	// Grab the offset as 4 bytes
	uint32 offset = *(uint32*)ptr;

	// Go to the next instruction, which is the starting point of the relative jump
	ptr += 4;

	// Now grab our pointer
	CBaseGameSystemFactory::sm_pFirst = (CBaseGameSystemFactory**)(ptr + offset);

	// And insert the game system(s)
	CGameSystem::sm_Factory = new CGameSystemStaticCustomFactory<CGameSystem>("CS2Fixes_GameSystem", &g_GameSystem);

	return true;
}

bool UnregisterGameSystem()
{
	// This signature directly points to the instruction referencing sm_pEventDispatcher, and the opcode is 3 bytes so we skip those
	uint8* ptr = (uint8*)g_GameConfig->ResolveSignature("IGameSystem_LoopPostInitAllSystems_pEventDispatcher") + 3;

	if (!ptr)
	{
		Panic("Failed to UnregisterGameSystem, see warnings above.\n");
		return false;
	}

	uint32 offset = *(uint32*)ptr;

	// Go to the next instruction, which is the starting point of the relative jump
	ptr += 4;

	CGameSystemEventDispatcher** ppDispatchers = (CGameSystemEventDispatcher**)(ptr + offset);

	// Here the opcode is 2 bytes as it's moving a dword, not a qword, but that's the start of a vector object
	ptr = (uint8*)g_GameConfig->ResolveSignature("IGameSystem_LoopDestroyAllSystems_s_GameSystems") + 2;

	if (!ptr)
	{
		Panic("Failed to UnregisterGameSystem, see warnings above.\n");
		return false;
	}

	offset = *(uint32*)ptr;

	ptr += 4;

	CUtlVector<AddedGameSystem_t>* pGameSystems = (CUtlVector<AddedGameSystem_t>*)(ptr + offset);

	auto* pDispatcher = *ppDispatchers;

	if (!pDispatcher || !pGameSystems)
	{
		Panic("Gamesystems and/or dispatchers is null, server is probably shutting down\n");
		return false;
	}

	auto& funcListeners = *pDispatcher->m_funcListeners;
	auto& gameSystems = *pGameSystems;

	FOR_EACH_VEC_BACK(gameSystems, i)
	{
		if (&g_GameSystem == gameSystems[i].m_pGameSystem)
		{
			gameSystems.FastRemove(i);
			break;
		}
	}

	FOR_EACH_VEC_BACK(funcListeners, i)
	{
		auto& vecListeners = funcListeners[i];

		FOR_EACH_VEC_BACK(vecListeners, j)
		{
			if (&g_GameSystem == vecListeners[j])
			{
				vecListeners.FastRemove(j);

				break;
			}
		}

		if (!vecListeners.Count())
			funcListeners.FastRemove(i);
	}

	CGameSystem::sm_Factory->DestroyGameSystem(&g_GameSystem);
	CGameSystem::sm_Factory->Destroy();

	return true;
}

extern CConVar<CUtlString> g_cvarBurnParticle;

GS_EVENT_MEMBER(CGameSystem, BuildGameSessionManifest)
{
	Message("CGameSystem::BuildGameSessionManifest\n");

	IEntityResourceManifest* pResourceManifest = msg->m_pResourceManifest;

	// This takes any resource type, model or not
	// Any resource adding MUST be done here, the resource manifest is not long-lived
	// pResourceManifest->AddResource("characters/models/my_character_model.vmdl");

	ZR_Precache(pResourceManifest);
	PrecacheBeaconParticle(pResourceManifest);
	Leader_Precache(pResourceManifest);

	pResourceManifest->AddResource(g_cvarBurnParticle.Get().String());
}

// Called every frame before entities think
GS_EVENT_MEMBER(CGameSystem, ServerPreEntityThink)
{
	VPROF_BUDGET("CGameSystem::ServerPreEntityThink", "CS2FixesPerFrame")
	g_playerManager->FlashLightThink();
	g_pIdleSystem->UpdateIdleTimes();

	if (GetGlobals())
		EntityHandler_OnGameFramePre(GetGlobals()->m_bInSimulation, GetGlobals()->tickcount);
}

// Called every frame after entities think
GS_EVENT_MEMBER(CGameSystem, ServerPostEntityThink)
{
	VPROF_BUDGET("CGameSystem::ServerPostEntityThink", "CS2FixesPerFrame")
	g_playerManager->UpdatePlayerStates();
}

GS_EVENT_MEMBER(CGameSystem, GameShutdown)
{
	g_pGameRules = nullptr;
}
