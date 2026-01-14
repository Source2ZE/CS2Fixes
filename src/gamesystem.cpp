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
#include "customio.h"
#include "entities.h"
#include "entity/cgamerules.h"
#include "gameconfig.h"
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
	// This signature directly points to the instruction referencing sm_pFirst
	uintptr_t pAddr = (uintptr_t)g_GameConfig->ResolveSignature("IGameSystem_InitAllSystems_pFirst");

	if (!pAddr)
	{
		Panic("Failed to InitGameSystems, see warnings above.\n");
		return false;
	}

	// the opcode is 3 bytes so we skip those
	pAddr += 3;

	// Grab the offset as 4 bytes
	uint32 offset = *(uint32*)pAddr;

	// Go to the next instruction, which is the starting point of the relative jump
	pAddr += 4;

	// Now grab our pointer
	CBaseGameSystemFactory::sm_pFirst = (CBaseGameSystemFactory**)(pAddr + offset);

	// And insert the game system(s)
	CGameSystem::sm_Factory = new CGameSystemStaticCustomFactory<CGameSystem>("CS2Fixes_GameSystem", &g_GameSystem);

	return true;
}

bool UnregisterGameSystem()
{
	// This signature directly points to the instruction referencing sm_pEventDispatcher
	uintptr_t pAddr = (uintptr_t)g_GameConfig->ResolveSignature("IGameSystem_LoopPostInitAllSystems_pEventDispatcher");

	if (!pAddr)
	{
		Panic("Failed to UnregisterGameSystem, see warnings above.\n");
		return false;
	}

	// the opcode is 3 bytes so we skip those
	pAddr += 3;

	uint32 offset = *(uint32*)pAddr;

	// Go to the next instruction, which is the starting point of the relative jump
	pAddr += 4;

	CGameSystemEventDispatcher** ppDispatchers = (CGameSystemEventDispatcher**)(pAddr + offset);

	pAddr = (uintptr_t)g_GameConfig->ResolveSignature("IGameSystem_LoopDestroyAllSystems_s_GameSystems");

	if (!pAddr)
	{
		Panic("Failed to UnregisterGameSystem, see warnings above.\n");
		return false;
	}

	// Here the opcode is 2 bytes as it's moving a dword, not a qword, but that's the start of a vector object
	pAddr += 2;

	offset = *(uint32*)pAddr;

	pAddr += 4;

	CUtlVector<AddedGameSystem_t>* pGameSystems = (CUtlVector<AddedGameSystem_t>*)(pAddr + offset);

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
	if (!g_pSpawnGroupMgr)
		return;

	CUtlVector<SpawnGroupHandle_t> vecActualSpawnGroups;
	addresses::GetSpawnGroups(g_pSpawnGroupMgr, &vecActualSpawnGroups);

	auto pClients = GetClientList();

	// Ensure clients have no leaked spawngroups every time a new one loads
	// Due to a timing problem for leaked spawngroups with this callback, clients may have one lingering, this is fine since it'll still be taken care of next time a spawngroup loads
	FOR_EACH_VEC(*pClients, i)
	{
		auto pClient = (*pClients)[i];

		if (!pClient || pClient->m_vecLoadedSpawnGroups.Count() == vecActualSpawnGroups.Count())
			continue;

		pClient->m_vecLoadedSpawnGroups = vecActualSpawnGroups;
	}
}