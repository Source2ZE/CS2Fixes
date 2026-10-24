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

#include "gamesystem.h"
#include "addresses.h"
#include "common.h"
#include "customio.h"
#include "entities.h"
#include "entity/cgamerules.h"
#include "idlemanager.h"
#include "leader.h"
#include "playermanager.h"
#include "tier0/vprof.h"
#include "zombiereborn.h"

#include "tier0/memdbgon.h"

CGameSystem g_GameSystem;
CBaseGameSystemFactory** CBaseGameSystemFactory::sm_pFirst = nullptr;
CGameSystemStaticCustomFactory<CGameSystem>* CGameSystem::sm_Factory = nullptr;

// This mess is needed to get the pointer to sm_pFirst so we can insert game systems
bool InitGameSystems()
{
	CBaseGameSystemFactory::sm_pFirst = (CBaseGameSystemFactory**)addresses::ResolveRipRelative("IGameSystem_InitAllSystems_pFirst");

	if (!CBaseGameSystemFactory::sm_pFirst)
	{
		Panic("Failed to InitGameSystems, see warnings above.\n");
		return false;
	}

	// And insert the game system(s)
	CGameSystem::sm_Factory = new CGameSystemStaticCustomFactory<CGameSystem>("CS2Fixes_GameSystem", &g_GameSystem);

	return true;
}

bool UnregisterGameSystem()
{
	CGameSystemEventDispatcher** ppDispatchers = (CGameSystemEventDispatcher**)addresses::ResolveRipRelative("IGameSystem_LoopPostInitAllSystems_pEventDispatcher");

	if (!ppDispatchers)
	{
		Panic("Failed to UnregisterGameSystem, see warnings above.\n");
		return false;
	}

	// Here the opcode is 2 bytes as it's moving a dword, not a qword, but that's the start of a vector object
	CUtlVector<AddedGameSystem_t>* pGameSystems = (CUtlVector<AddedGameSystem_t>*)addresses::ResolveRipRelative("IGameSystem_LoopDestroyAllSystems_s_GameSystems", 2);

	if (!pGameSystems)
	{
		Panic("Failed to UnregisterGameSystem, see warnings above.\n");
		return false;
	}

	auto* pDispatcher = *ppDispatchers;

	if (!pDispatcher || !pGameSystems)
	{
		Message("Gamesystems and/or dispatchers is null, server is probably shutting down\n");
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

GS_EVENT_MEMBER(CGameSystem, PostSpawnGroupLoad)
{
	if (!GetNetworkGameServer())
		return;

	auto& spawnGroups = GetNetworkGameServer()->m_SpawnGroups;

	// Ensure clients have no leaked spawngroups every time a new one loads
	// Due to a timing problem for leaked spawngroups with this callback, clients may have one lingering, this is fine since it'll still be taken care of next time a spawngroup loads
	for (auto pClient : *GetClientList())
	{
		if (!pClient || pClient->m_LoadedSpawnGroups.Count() == spawnGroups.Count())
			continue;

		pClient->m_LoadedSpawnGroups.RemoveAll();

		for (const auto& spawnGroup : spawnGroups)
			pClient->m_LoadedSpawnGroups.AddToTail(spawnGroup.key);
	}
}