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

#include "cs_usercmd.pb.h"
#include "networkbasetypes.pb.h"
#include "usercmd.pb.h"

#include "addresses.h"
#include "adminsystem.h"
#include "buttonwatch.h"
#include "commands.h"
#include "common.h"
#include "cs2fixes.h"
#include "cs_gameevents.pb.h"
#include "ctimer.h"
#include "customio.h"
#include "entities.h"
#include "entity/cbasemodelentity.h"
#include "entity/ccsplayercontroller.h"
#include "entity/ccsplayerpawn.h"
#include "entity/ccsweaponbase.h"
#include "entity/cenvhudhint.h"
#include "entity/cgamerules.h"
#include "entity/cpointviewcontrol.h"
#include "entity/ctakedamageinfo.h"
#include "entity/ctriggerpush.h"
#include "entity/services.h"
#include "entitylistener.h"
#include "entwatch.h"
#include "eventlistener.h"
#include "gameconfig.h"
#include "gameevents.pb.h"
#include "hookmanager.h"
#include "idlemanager.h"
#include "igameevents.h"
#include "irecipientfilter.h"
#include "leader.h"
#include "map_votes.h"
#include "mapmigrations.h"
#include "module.h"
#include "networksystem/inetworkserializer.h"
#include "networkstringtabledefs.h"
#include "panoramavote.h"
#include "patches.h"
#include "playermanager.h"
#include "serversideclient.h"
#include "te.pb.h"
#include "tier0/vprof.h"
#include "usermessages.pb.h"
#include "votemanager.h"
#include "zombiereborn.h"
#include <entity.h>

#include "tier0/memdbgon.h"

CHookManager* g_pHookManager = nullptr;

double g_flUniversalTime = 0.0;
float g_flLastTickedTime = 0.0f;
bool g_bHasTicked = false;

CConVar<CUtlString> g_cvarMotdUrl("cs2f_motd_url", FCVAR_NONE, "Server MOTD URL, shows up as a \"Server Website\" button in scoreboard", "");
CConVar<bool> g_cvarBlockParticleMsgs("cs2f_block_particle_msgs", FCVAR_NONE, "Whether to block CUserMsg_ParticleManager messages to fix lag/crashes, experimental", false);
CConVar<bool> g_cvarDropMapWeapons("cs2f_drop_map_weapons", FCVAR_NONE, "Whether to force drop map-spawned weapons on death", false);
CConVar<bool> g_cvarFixPhysicsPlayerShuffle("cs2f_shuffle_player_physics_sim", FCVAR_NONE, "Whether to enable shuffle player list in physics simulate", false);

class CUserCmd
{
public:
	[[maybe_unused]] char pad0[0x10];
	CSGOUserCmdPB cmd;
	[[maybe_unused]] char pad1[0x38];
#ifdef PLATFORM_WINDOWS
	[[maybe_unused]] char pad2[0x8];
#endif
};

CConVar<bool> g_cvarBlockMolotovSelfDmg("cs2f_block_molotov_self_dmg", FCVAR_NONE, "Whether to block self-damage from molotovs", false);
CConVar<bool> g_cvarBlockAllDamage("cs2f_block_all_dmg", FCVAR_NONE, "Whether to block all damage to players", false);
CConVar<bool> g_cvarFixBlockDamage("cs2f_fix_block_dmg", FCVAR_NONE, "Whether to fix block-damage on players", false);
CConVar<bool> g_cvarUseOldPush("cs2f_use_old_push", FCVAR_NONE, "Whether to use the old CSGO trigger_push behavior", false);
CConVar<bool> g_cvarLogPushes("cs2f_log_pushes", FCVAR_NONE, "Whether to log pushes (cs2f_use_old_push must be enabled)", false);
CConVar<bool> g_cvarEnableTriggerTimer("cs2f_trigger_timer_enable", FCVAR_NONE, "Whether to process countdown messages said by Console (e.g. Hold for 10 seconds) and append the round time where the countdown resolves", false);
CConVar<bool> g_cvarDisableSetModel("cs2f_disable_setmodel", FCVAR_NONE, "Whether to disable SetModel usage from maps (custom input, cs_script function)", false);
CConVar<bool> g_cvarBlockNavLookup("cs2f_block_nav_lookup", FCVAR_NONE, "Whether to block navigation mesh lookup, improves server performance but breaks bot navigation", false);
CConVar<bool> g_cvarDisableSubtickMovement("cs2f_disable_subtick_move", FCVAR_NONE, "Whether to disable subtick movement", false);
CConVar<bool> g_cvarDisableSubtickShooting("cs2f_disable_subtick_shooting", FCVAR_NONE, "Whether to disable subtick shooting, experimental (WARNING: add \"log_flags Shooting + DoNotEcho\" to your cfg to prevent console spam on every shot fired)", false);
CConVar<bool> g_cvarPreventUsingPlayers("cs2f_prevent_using_players", FCVAR_NONE, "Whether to prevent +use from hitting players (0=can use players, 1=cannot use players)", false);
CConVar<bool> g_cvarFixGameBans("cs2f_fix_game_bans", FCVAR_NONE, "Whether to fix CS2 game bans spreading to all new joining players", false);

template <typename RETURN, typename... ARGS>
static void SetupDetour(CGameConfig* gameConfig, KHook::Function<RETURN, ARGS...>* hook, const char* name)
{
	auto pfnFunc = reinterpret_cast<RETURN (*)(ARGS...)>(gameConfig->ResolveSignature(name));

	if (!pfnFunc || !hook)
	{
		g_bRequiredInitLoaded = false;
		return;
	}

	hook->Configure(pfnFunc);
	Message("Detoured %s at 0x%p\n", name, pfnFunc);
}

template <typename CLASS, typename RETURN, typename... ARGS>
static void SetupDetour(CGameConfig* gameConfig, KHook::Member<CLASS, RETURN, ARGS...>* hook, const char* name)
{
	void* pfnFunc = gameConfig->ResolveSignature(name);

	if (!pfnFunc || !hook)
	{
		g_bRequiredInitLoaded = false;
		return;
	}

	hook->Configure(pfnFunc);
	Message("Detoured %s at 0x%p\n", name, pfnFunc);
}

// makeVirtual — mirrors Kenzzer's declareHook() but returns a heap-allocated
// pointer bound to CHookManager as context. Deduces CLASS/RETURN/ARGS from the
// callback signature so callers never need explicit template params.

// With function MFP (vtable index known at compile time): pre + post
template<typename CLASS, typename RETURN, typename... ARGS>
static KHook::Virtual<CLASS, RETURN, ARGS...>* makeVirtual(
	CHookManager* ctx, RETURN (CLASS::*fn)(ARGS...),
	KHook::Return<RETURN> (CHookManager::*pre)(CLASS*, ARGS...),
	KHook::Return<RETURN> (CHookManager::*post)(CLASS*, ARGS...))
{ return new KHook::Virtual<CLASS, RETURN, ARGS...>(fn, ctx, pre, post); }

// With function MFP: nullptr pre
template<typename CLASS, typename RETURN, typename... ARGS>
static KHook::Virtual<CLASS, RETURN, ARGS...>* makeVirtual(
	CHookManager* ctx, RETURN (CLASS::*fn)(ARGS...),
	std::nullptr_t,
	KHook::Return<RETURN> (CHookManager::*post)(CLASS*, ARGS...))
{ return new KHook::Virtual<CLASS, RETURN, ARGS...>(fn, ctx, nullptr, post); }

// With function MFP: nullptr post
template<typename CLASS, typename RETURN, typename... ARGS>
static KHook::Virtual<CLASS, RETURN, ARGS...>* makeVirtual(
	CHookManager* ctx, RETURN (CLASS::*fn)(ARGS...),
	KHook::Return<RETURN> (CHookManager::*pre)(CLASS*, ARGS...),
	std::nullptr_t)
{ return new KHook::Virtual<CLASS, RETURN, ARGS...>(fn, ctx, pre, nullptr); }

// No-function hooks (vtable index set later via Configure()): pre + post
template<typename CLASS, typename RETURN, typename... ARGS>
static KHook::Virtual<CLASS, RETURN, ARGS...>* makeVirtual(
	CHookManager* ctx,
	KHook::Return<RETURN> (CHookManager::*pre)(CLASS*, ARGS...),
	KHook::Return<RETURN> (CHookManager::*post)(CLASS*, ARGS...))
{ return new KHook::Virtual<CLASS, RETURN, ARGS...>(nullptr, ctx, pre, post); }

// No-function: nullptr pre
template<typename CLASS, typename RETURN, typename... ARGS>
static KHook::Virtual<CLASS, RETURN, ARGS...>* makeVirtual(
	CHookManager* ctx,
	std::nullptr_t,
	KHook::Return<RETURN> (CHookManager::*post)(CLASS*, ARGS...))
{ return new KHook::Virtual<CLASS, RETURN, ARGS...>(nullptr, ctx, nullptr, post); }

// No-function: nullptr post
template<typename CLASS, typename RETURN, typename... ARGS>
static KHook::Virtual<CLASS, RETURN, ARGS...>* makeVirtual(
	CHookManager* ctx,
	KHook::Return<RETURN> (CHookManager::*pre)(CLASS*, ARGS...),
	std::nullptr_t)
{ return new KHook::Virtual<CLASS, RETURN, ARGS...>(nullptr, ctx, pre, nullptr); }

CHookManager::CHookManager(CGameConfig* pGameConfig) :
	m_hTakeDamageOld(new KHook::Member(this, &CHookManager::Hook_TakeDamageOld, &CHookManager::Hook_TakeDamageOld_Post)),
	m_hTriggerPushTouch(new KHook::Member(this, &CHookManager::Hook_TriggerPushTouch, nullptr)),
	m_hIsHearingClient(new KHook::Function(this, &CHookManager::Hook_IsHearingClient, nullptr)),
	m_hSayTextFilter(new KHook::Function(this, &CHookManager::Hook_SayTextFilter, nullptr)),
	m_hSayText2Filter(new KHook::Function(this, &CHookManager::Hook_SayText2Filter, nullptr)),
	m_hCanUse(new KHook::Member(this, &CHookManager::Hook_CanUse, nullptr)),
	m_hEquipWeapon(new KHook::Member(this, &CHookManager::Hook_EquipWeapon, nullptr)),
	m_hAcceptInput(new KHook::Member(this, &CHookManager::Hook_AcceptInput, nullptr)),
	m_hGetNearestNavArea(new KHook::Member(this, &CHookManager::Hook_GetNearestNavArea, nullptr)),
	m_hProcessMovement(new KHook::Member(this, &CHookManager::Hook_ProcessMovement, &CHookManager::Hook_ProcessMovement_Post)),
	m_hProcessUsercmds(new KHook::Member(this, &CHookManager::Hook_ProcessUsercmds, nullptr)),
	m_hInputTriggerForAllPlayers(new KHook::Member(this, &CHookManager::Hook_InputTriggerForAllPlayers, nullptr)),
	m_hInputTriggerForActivatedPlayer(new KHook::Member(this, &CHookManager::Hook_InputTriggerForActivatedPlayer, nullptr)),
	m_hGravityTouch(new KHook::Member(this, &CHookManager::Hook_GravityTouch, nullptr)),
	m_hGetFreeClient(new KHook::Function(this, &CHookManager::Hook_GetFreeClient, nullptr)),
	m_hGetMaxSpeed(new KHook::Member(this, &CHookManager::Hook_GetMaxSpeed, nullptr)),
	m_hFindUseEntity(new KHook::Member(this, &CHookManager::Hook_FindUseEntity, &CHookManager::Hook_FindUseEntity_Post)),
	m_hTraceFunc(new KHook::Function(this, &CHookManager::Hook_TraceFunc, nullptr)),
	m_hTraceShape(new KHook::Function(this, &CHookManager::Hook_TraceShape, nullptr)),
	m_hFireOutputInternal(new KHook::Member(this, &CHookManager::Hook_FireOutputInternal, nullptr)),
	m_hGetEyePosition(new KHook::Member(this, &CHookManager::Hook_GetEyePosition, nullptr)),
	m_hGetEyeAngles(new KHook::Member(this, &CHookManager::Hook_GetEyeAngles, nullptr)),
	m_hInputTestActivator(new KHook::Member(this, &CHookManager::Hook_InputTestActivator, nullptr)),
	m_hCheckSteamBan(new KHook::Function(this, nullptr, &CHookManager::Hook_CheckSteamBan_Post)),
	m_hCanAcquire(new KHook::Member(this, &CHookManager::Hook_CanAcquire, nullptr)),
	m_hScriptSetModel(new KHook::Function(this, &CHookManager::Hook_ScriptSetModel, &CHookManager::Hook_ScriptSetModel_Post)),
	m_hSetModel(new KHook::Member(this, &CHookManager::Hook_SetModel, nullptr)),
	m_hGoToIntermission(new KHook::Member(this, &CHookManager::Hook_GoToIntermission, nullptr)),
	m_hGameFrame(makeVirtual(this, &IServerGameDLL::GameFrame, nullptr, &CHookManager::Hook_GameFrame_Post)),
	m_hGameServerSteamAPIActivated(makeVirtual(this, &IServerGameDLL::GameServerSteamAPIActivated, &CHookManager::Hook_GameServerSteamAPIActivated, nullptr)),
	m_hApplyGameSettings(makeVirtual(this, &IServerGameDLL::ApplyGameSettings, &CHookManager::Hook_ApplyGameSettings, nullptr)),
	m_hClientActive(makeVirtual(this, &IServerGameClients::ClientActive, nullptr, &CHookManager::Hook_ClientActive_Post)),
	m_hClientDisconnect(makeVirtual(this, &IServerGameClients::ClientDisconnect, nullptr, &CHookManager::Hook_ClientDisconnect_Post)),
	m_hClientPutInServer(makeVirtual(this, &IServerGameClients::ClientPutInServer, nullptr, &CHookManager::Hook_ClientPutInServer_Post)),
	m_hClientSettingsChanged(makeVirtual(this, &IServerGameClients::ClientSettingsChanged, &CHookManager::Hook_ClientSettingsChanged, nullptr)),
	m_hOnClientConnected(makeVirtual(this, &IServerGameClients::OnClientConnected, &CHookManager::Hook_OnClientConnected, nullptr)),
	m_hClientConnect(makeVirtual(this, &IServerGameClients::ClientConnect, &CHookManager::Hook_ClientConnect, nullptr)),
	m_hClientCommand(makeVirtual(this, &IServerGameClients::ClientCommand, &CHookManager::Hook_ClientCommand, nullptr)),
	m_hPostEventAbstract(makeVirtual(this, &IGameEventSystem::PostEventAbstract, &CHookManager::Hook_PostEventAbstract, nullptr)),
	m_hStartupServer(makeVirtual(this, &INetworkServerService::StartupServer, nullptr, &CHookManager::Hook_StartupServer_Post)),
	m_hCheckTransmit(makeVirtual(this, &ISource2GameEntities::CheckTransmit, nullptr, &CHookManager::Hook_CheckTransmit_Post)),
	m_hDispatchConCommand(makeVirtual(this, &ICvar::DispatchConCommand, &CHookManager::Hook_DispatchConCommand, nullptr)),
	m_hLoadEventsFromFile(makeVirtual(this, &IGameEventManager2::LoadEventsFromFile, &CHookManager::Hook_LoadEventsFromFile, nullptr)),
	m_hSpawn(makeVirtual(this, &CEntitySystem::Spawn, nullptr, &CHookManager::Hook_Spawn_Post)),
	m_hSetGameSpawnGroupMgr(makeVirtual(this, &INetworkGameServer::SetGameSpawnGroupMgr, &CHookManager::Hook_SetGameSpawnGroupMgr, nullptr)),
	m_hCreateWorkshopMapGroup(makeVirtual(this, &CHookManager::Hook_CreateWorkshopMapGroup, nullptr)),
	m_hGetTouchingList(makeVirtual(this, nullptr, &CHookManager::Hook_GetTouchingList_Post)),
	m_hCheckMovingGround(makeVirtual(this, &CHookManager::Hook_CheckMovingGround, nullptr)),
	m_hDropWeapon(makeVirtual(this, nullptr, &CHookManager::Hook_DropWeapon_Post)),
	m_hPlayerEquipUse(makeVirtual(this, &CHookManager::Hook_PlayerEquipUse, nullptr)),
	m_hPlayerEquipPrecache(makeVirtual(this, nullptr, &CHookManager::Hook_PlayerEquipPrecache_Post)),
	m_hTriggerGravityPrecache(makeVirtual(this, nullptr, &CHookManager::Hook_TriggerGravityPrecache_Post)),
	m_hTriggerGravityEndTouch(makeVirtual(this, nullptr, &CHookManager::Hook_TriggerGravityEndTouch_Post)),
	m_hOnTakeDamageAlive(makeVirtual(this, &CHookManager::Hook_OnTakeDamage_Alive, nullptr)),
	m_hPlayerPawnTeleport(makeVirtual(this, &CHookManager::Hook_CCSPlayerPawn_Teleport, nullptr))
{
	CreateHooks(pGameConfig);
}

CHookManager::~CHookManager()
{
	RemoveHooks();
}

void CHookManager::CreateHooks(CGameConfig* gameConfig)
{
	SetupDetour(gameConfig, m_hTakeDamageOld, "CBaseEntity_TakeDamageOld");
	SetupDetour(gameConfig, m_hTriggerPushTouch, "TriggerPush_Touch");
	SetupDetour(gameConfig, m_hIsHearingClient, "IsHearingClient");
	SetupDetour(gameConfig, m_hSayTextFilter, "UTIL_SayTextFilter");
	SetupDetour(gameConfig, m_hSayText2Filter, "UTIL_SayText2Filter");
	SetupDetour(gameConfig, m_hCanUse, "CCSPlayer_WeaponServices_CanUse");
	SetupDetour(gameConfig, m_hEquipWeapon, "CCSPlayer_WeaponServices_EquipWeapon");
	SetupDetour(gameConfig, m_hAcceptInput, "CEntityIdentity_AcceptInput");
	SetupDetour(gameConfig, m_hGetNearestNavArea, "CNavMesh_GetNearestNavArea");
	SetupDetour(gameConfig, m_hProcessMovement, "ProcessMovement");
	SetupDetour(gameConfig, m_hProcessUsercmds, "ProcessUsercmds");
	SetupDetour(gameConfig, m_hInputTriggerForAllPlayers, "CGamePlayerEquip_InputTriggerForAllPlayers");
	SetupDetour(gameConfig, m_hInputTriggerForActivatedPlayer, "CGamePlayerEquip_InputTriggerForActivatedPlayer");
	SetupDetour(gameConfig, m_hGravityTouch, "CTriggerGravity_GravityTouch");
	SetupDetour(gameConfig, m_hGetFreeClient, "GetFreeClient");
#ifdef __linux__
	// Inlined by MSVC as of 2025-07-28 CS2 update
	// TODO: Find some alternative that supports Windows
	SetupDetour(gameConfig, m_hGetMaxSpeed, "CCSPlayerPawn_GetMaxSpeed");
#endif
	SetupDetour(gameConfig, m_hFindUseEntity, "FindUseEntity");
	SetupDetour(gameConfig, m_hTraceFunc, "TraceFunc");
	SetupDetour(gameConfig, m_hTraceShape, "TraceShape");
	SetupDetour(gameConfig, m_hFireOutputInternal, "CEntityIOOutput_FireOutputInternal");
	SetupDetour(gameConfig, m_hGetEyePosition, "CBasePlayerPawn_GetEyePosition");
	SetupDetour(gameConfig, m_hGetEyeAngles, "CBasePlayerPawn_GetEyeAngles");
	SetupDetour(gameConfig, m_hInputTestActivator, "CBaseFilter_InputTestActivator");
	SetupDetour(gameConfig, m_hCheckSteamBan, "GameSystem_Think_CheckSteamBan");
	SetupDetour(gameConfig, m_hCanAcquire, "CCSPlayer_ItemServices_CanAcquire");
	SetupDetour(gameConfig, m_hScriptSetModel, "CS_Script_SetModel");
	SetupDetour(gameConfig, m_hSetModel, "CBaseModelEntity_SetModel");
	SetupDetour(gameConfig, m_hGoToIntermission, "CCSGameRules_GoToIntermission");

	m_hGameFrame->Add(g_pSource2Server);
	m_hGameServerSteamAPIActivated->Add(g_pSource2Server);
	m_hApplyGameSettings->Add(g_pSource2Server);
	m_hClientActive->Add(g_pSource2GameClients);
	m_hClientDisconnect->Add(g_pSource2GameClients);
	m_hClientPutInServer->Add(g_pSource2GameClients);
	m_hClientSettingsChanged->Add(g_pSource2GameClients);
	m_hOnClientConnected->Add(g_pSource2GameClients);
	m_hClientConnect->Add(g_pSource2GameClients);
	m_hClientCommand->Add(g_pSource2GameClients);
	m_hPostEventAbstract->Add(g_gameEventSystem);
	m_hStartupServer->Add(g_pNetworkServerService);
	m_hCheckTransmit->Add(g_pSource2GameEntities);
	m_hDispatchConCommand->Add(g_pCVar);

	m_pCGameEventManagerVTable = (IGameEventManager2*)modules::server->FindVirtualTable("CGameEventManager");
	if (!m_pCGameEventManagerVTable)
	{
		Panic("Failed to find CGameEventManager vtable\n");
		g_bRequiredInitLoaded = false;
	}
	else
	{
		m_hLoadEventsFromFile->AddGlobal((IGameEventManager2*)&m_pCGameEventManagerVTable);
	}

	m_pCEntitySystemVTable = (CEntitySystem*)modules::server->FindVirtualTable("CGameEntitySystem");
	if (!m_pCEntitySystemVTable)
	{
		Panic("Failed to find CGameEntitySystem vtable\n");
		g_bRequiredInitLoaded = false;
	}
	else
	{
		m_hSpawn->AddGlobal((CEntitySystem*)&m_pCEntitySystemVTable);
	}

	int offset = gameConfig->GetOffset("IGameTypes_CreateWorkshopMapGroup");
	if (offset == -1)
	{
		Panic("Failed to find IGameTypes_CreateWorkshopMapGroup\n");
		g_bRequiredInitLoaded = false;
	}
	else
	{
		m_hCreateWorkshopMapGroup->Configure(offset);
		m_hCreateWorkshopMapGroup->Add(g_pGameTypes);
	}

	m_pCVPhys2WorldVTable = (CVPhys2World*)modules::vphysics2->FindVirtualTable("CVPhys2World");
	if (!m_pCVPhys2WorldVTable)
	{
		Panic("Failed to find CVPhys2World vtable\n");
		g_bRequiredInitLoaded = false;
	}
	else
	{
		offset = gameConfig->GetOffset("CVPhys2World::GetTouchingList");
		if (offset == -1)
		{
			Panic("Failed to find offset for CVPhys2World::GetTouchingList\n");
			g_bRequiredInitLoaded = false;
		}
		else
		{
			m_hGetTouchingList->Configure(offset);
			m_hGetTouchingList->AddGlobal((CVPhys2World*)&m_pCVPhys2WorldVTable);
		}
	}

	m_pCCSPlayer_MovementServicesVTable = (CCSPlayer_MovementServices*)modules::server->FindVirtualTable("CCSPlayer_MovementServices");
	if (!m_pCCSPlayer_MovementServicesVTable)
	{
		Panic("Failed to find CCSPlayer_MovementServices vtable\n");
		g_bRequiredInitLoaded = false;
	}
	else
	{
		offset = gameConfig->GetOffset("CCSPlayer_MovementServices::CheckMovingGround");
		if (offset == -1)
		{
			Panic("Failed to find offset for CCSPlayer_MovementServices::CheckMovingGround\n");
			g_bRequiredInitLoaded = false;
		}
		else
		{
			m_hCheckMovingGround->Configure(offset);
			m_hCheckMovingGround->AddGlobal((CCSPlayer_MovementServices*)&m_pCCSPlayer_MovementServicesVTable);
		}
	}

	m_pCCSPlayer_WeaponServicesVTable = (CCSPlayer_WeaponServices*)modules::server->FindVirtualTable("CCSPlayer_WeaponServices");
	if (!m_pCCSPlayer_WeaponServicesVTable)
	{
		Panic("Failed to find CCSPlayer_WeaponServices vtable\n");
		g_bRequiredInitLoaded = false;
	}
	else
	{
		offset = gameConfig->GetOffset("CCSPlayer_WeaponServices::DropWeapon");
		if (offset == -1)
		{
			Panic("Failed to find offset for CCSPlayer_WeaponServices::DropWeapon\n");
			g_bRequiredInitLoaded = false;
		}
		else
		{
			m_hDropWeapon->Configure(offset);
			m_hDropWeapon->AddGlobal((CCSPlayer_WeaponServices*)&m_pCCSPlayer_WeaponServicesVTable);
		}
	}

	m_pCGamePlayerEquipVTable = (CGamePlayerEquip*)modules::server->FindVirtualTable("CGamePlayerEquip");
	if (!m_pCGamePlayerEquipVTable)
	{
		Panic("Failed to find CGamePlayerEquip vtable\n");
		g_bRequiredInitLoaded = false;
	}
	else
	{
		offset = gameConfig->GetOffset("CBaseEntity::Use");
		if (offset == -1)
		{
			Panic("Failed to find offset for CBaseEntity::Use\n");
			g_bRequiredInitLoaded = false;
		}
		else
		{
			m_hPlayerEquipUse->Configure(offset);
			m_hPlayerEquipUse->AddGlobal((CGamePlayerEquip*)&m_pCGamePlayerEquipVTable);
		}

		offset = gameConfig->GetOffset("CBaseEntity::Precache");
		if (offset == -1)
		{
			Panic("Failed to find offset for CBaseEntity::Precache\n");
			g_bRequiredInitLoaded = false;
		}
		else
		{
			m_hPlayerEquipPrecache->Configure(offset);
			m_hPlayerEquipPrecache->AddGlobal((CGamePlayerEquip*)&m_pCGamePlayerEquipVTable);
		}
	}

	m_pTriggerGravityVTable = (CTriggerGravity*)modules::server->FindVirtualTable("CTriggerGravity");
	if (!m_pTriggerGravityVTable)
	{
		Panic("Failed to find CTriggerGravity vtable\n");
		g_bRequiredInitLoaded = false;
	}
	else
	{
		// Reuse CBaseEntity::Precache offset found above
		offset = gameConfig->GetOffset("CBaseEntity::Precache");
		if (offset != -1)
		{
			m_hTriggerGravityPrecache->Configure(offset);
			m_hTriggerGravityPrecache->AddGlobal((CTriggerGravity*)&m_pTriggerGravityVTable);
		}

		offset = gameConfig->GetOffset("CBaseEntity::EndTouch");
		if (offset == -1)
		{
			Panic("Failed to find offset for CBaseEntity::EndTouch\n");
			g_bRequiredInitLoaded = false;
		}
		else
		{
			m_hTriggerGravityEndTouch->Configure(offset);
			m_hTriggerGravityEndTouch->AddGlobal((CTriggerGravity*)&m_pTriggerGravityVTable);
		}
	}

	m_pCCSPlayerPawnVTable = (CCSPlayerPawn*)modules::server->FindVirtualTable("CCSPlayerPawn");
	if (!m_pCCSPlayerPawnVTable)
	{
		Panic("Failed to find CCSPlayerPawn vtable\n");
		g_bRequiredInitLoaded = false;
	}
	else
	{
		offset = gameConfig->GetOffset("CCSPlayerPawn::OnTakeDamage_Alive");
		if (offset == -1)
		{
			Panic("Failed to find offset for CCSPlayerPawn::OnTakeDamage_Alive\n");
			g_bRequiredInitLoaded = false;
		}
		else
		{
			m_hOnTakeDamageAlive->Configure(offset);
			m_hOnTakeDamageAlive->AddGlobal((CCSPlayerPawn*)&m_pCCSPlayerPawnVTable);
		}

		offset = gameConfig->GetOffset("Teleport");
		if (offset == -1)
		{
			Panic("Failed to find offset for Teleport\n");
			g_bRequiredInitLoaded = false;
		}
		else
		{
			m_hPlayerPawnTeleport->Configure(offset);
			m_hPlayerPawnTeleport->AddGlobal((CCSPlayerPawn*)&m_pCCSPlayerPawnVTable);
		}
	}
}

void CHookManager::RemoveHooks()
{
	// Remove virtual hooks
	m_hGameFrame->Remove(g_pSource2Server);
	m_hGameServerSteamAPIActivated->Remove(g_pSource2Server);
	m_hApplyGameSettings->Remove(g_pSource2Server);
	m_hClientActive->Remove(g_pSource2GameClients);
	m_hClientDisconnect->Remove(g_pSource2GameClients);
	m_hClientPutInServer->Remove(g_pSource2GameClients);
	m_hClientSettingsChanged->Remove(g_pSource2GameClients);
	m_hOnClientConnected->Remove(g_pSource2GameClients);
	m_hClientConnect->Remove(g_pSource2GameClients);
	m_hClientCommand->Remove(g_pSource2GameClients);
	m_hPostEventAbstract->Remove(g_gameEventSystem);
	m_hStartupServer->Remove(g_pNetworkServerService);
	m_hCheckTransmit->Remove(g_pSource2GameEntities);
	m_hDispatchConCommand->Remove(g_pCVar);
	m_hSetGameSpawnGroupMgr->Remove(GetNetworkGameServer());

	if (m_pCGameEventManagerVTable)
		m_hLoadEventsFromFile->RemoveGlobal((IGameEventManager2*)&m_pCGameEventManagerVTable);
	if (m_pCEntitySystemVTable)
		m_hSpawn->RemoveGlobal((CEntitySystem*)&m_pCEntitySystemVTable);
	m_hCreateWorkshopMapGroup->Remove(g_pGameTypes);
	if (m_pCVPhys2WorldVTable)
		m_hGetTouchingList->RemoveGlobal((CVPhys2World*)&m_pCVPhys2WorldVTable);
	if (m_pCCSPlayer_MovementServicesVTable)
		m_hCheckMovingGround->RemoveGlobal((CCSPlayer_MovementServices*)&m_pCCSPlayer_MovementServicesVTable);
	if (m_pCCSPlayer_WeaponServicesVTable)
		m_hDropWeapon->RemoveGlobal((CCSPlayer_WeaponServices*)&m_pCCSPlayer_WeaponServicesVTable);
	if (m_pCGamePlayerEquipVTable)
	{
		m_hPlayerEquipUse->RemoveGlobal((CGamePlayerEquip*)&m_pCGamePlayerEquipVTable);
		m_hPlayerEquipPrecache->RemoveGlobal((CGamePlayerEquip*)&m_pCGamePlayerEquipVTable);
	}
	if (m_pTriggerGravityVTable)
	{
		m_hTriggerGravityPrecache->RemoveGlobal((CTriggerGravity*)&m_pTriggerGravityVTable);
		m_hTriggerGravityEndTouch->RemoveGlobal((CTriggerGravity*)&m_pTriggerGravityVTable);
	}
	if (m_pCCSPlayerPawnVTable)
	{
		m_hOnTakeDamageAlive->RemoveGlobal((CCSPlayerPawn*)&m_pCCSPlayerPawnVTable);
		m_hPlayerPawnTeleport->RemoveGlobal((CCSPlayerPawn*)&m_pCCSPlayerPawnVTable);
	}

	delete m_hGameFrame;
	delete m_hGameServerSteamAPIActivated;
	delete m_hApplyGameSettings;
	delete m_hClientActive;
	delete m_hClientDisconnect;
	delete m_hClientPutInServer;
	delete m_hClientSettingsChanged;
	delete m_hOnClientConnected;
	delete m_hClientConnect;
	delete m_hClientCommand;
	delete m_hPostEventAbstract;
	delete m_hStartupServer;
	delete m_hCheckTransmit;
	delete m_hDispatchConCommand;
	delete m_hLoadEventsFromFile;
	delete m_hSpawn;
	delete m_hSetGameSpawnGroupMgr;
	delete m_hCreateWorkshopMapGroup;
	delete m_hGetTouchingList;
	delete m_hCheckMovingGround;
	delete m_hDropWeapon;
	delete m_hPlayerEquipUse;
	delete m_hPlayerEquipPrecache;
	delete m_hTriggerGravityPrecache;
	delete m_hTriggerGravityEndTouch;
	delete m_hOnTakeDamageAlive;
	delete m_hPlayerPawnTeleport;

	delete m_hTakeDamageOld;
	delete m_hTriggerPushTouch;
	delete m_hIsHearingClient;
	delete m_hSayTextFilter;
	delete m_hSayText2Filter;
	delete m_hCanUse;
	delete m_hEquipWeapon;
	delete m_hAcceptInput;
	delete m_hGetNearestNavArea;
	delete m_hProcessMovement;
	delete m_hProcessUsercmds;
	delete m_hInputTriggerForAllPlayers;
	delete m_hInputTriggerForActivatedPlayer;
	delete m_hGravityTouch;
	delete m_hGetFreeClient;
	delete m_hGetMaxSpeed;
	delete m_hFindUseEntity;
	delete m_hTraceFunc;
	delete m_hTraceShape;
	delete m_hFireOutputInternal;
	delete m_hGetEyePosition;
	delete m_hGetEyeAngles;
	delete m_hInputTestActivator;
	delete m_hCheckSteamBan;
	delete m_hCanAcquire;
	delete m_hScriptSetModel;
	delete m_hSetModel;
	delete m_hGoToIntermission;
}

KHook::Return<void> CHookManager::Hook_GameFrame_Post(IServerGameDLL* pThis, bool simulating, bool bFirstTick, bool bLastTick)
{
	VPROF_BUDGET("CS2Fixes::Hook_GameFramePost", "CS2FixesPerFrame");

	if (!GetGlobals())
		return {KHook::Action::Ignore};

	if (simulating && g_bHasTicked)
		g_flUniversalTime += GetGlobals()->curtime - g_flLastTickedTime;

	g_flLastTickedTime = GetGlobals()->curtime;
	g_bHasTicked = true;

	RunTimers();
	EntityHandler_OnGameFramePost(simulating, GetGlobals()->tickcount);

	return {KHook::Action::Ignore};
}

KHook::Return<void> CHookManager::Hook_GameServerSteamAPIActivated(IServerGameDLL* pThis)
{
	g_playerManager->OnSteamAPIActivated();

	if (g_cvarVoteManagerEnable.Get() && !g_pMapVoteSystem->IsMapListLoaded())
		g_pMapVoteSystem->LoadMapList();

	return {KHook::Action::Ignore};
}

KHook::Return<void> CHookManager::Hook_ApplyGameSettings(IServerGameDLL* pThis, KeyValues* pKV)
{
	g_pMapVoteSystem->ApplyGameSettings(pKV);
	g_pMapMigrations->ApplyGameSettings(pKV);

	return {KHook::Action::Ignore};
}

KHook::Return<void> CHookManager::Hook_ClientActive_Post(IServerGameClients* pThis, CPlayerSlot slot, bool bLoadGame, const char* pszName, uint64 xuid)
{
	Message("Hook_ClientActive(%d, %d, \"%s\", %lli)\n", slot, bLoadGame, pszName, xuid);

	return {KHook::Action::Ignore};
}

KHook::Return<void> CHookManager::Hook_ClientDisconnect_Post(IServerGameClients* pThis, CPlayerSlot slot, ENetworkDisconnectionReason reason, const char* pszName, uint64 xuid, const char* pszNetworkID)
{
	Message("Hook_ClientDisconnect(%d, %d, \"%s\", %lli)\n", slot, reason, pszName, xuid);

	if (g_cvarEnableZR.Get())
	{
		if (!ZR_CheckTeamWinConditions(CS_TEAM_T))
			ZR_CheckTeamWinConditions(CS_TEAM_CT);
	}

	ZEPlayer* pPlayer = g_playerManager->GetPlayer(slot);

	if (!pPlayer)
		return {KHook::Action::Ignore};

	if (reason != NETWORK_DISCONNECT_LOOPSHUTDOWN && reason != NETWORK_DISCONNECT_SHUTDOWN)
		g_pAdminSystem->AddDisconnectedPlayer(pszName, xuid, pPlayer ? pPlayer->GetIpAddress() : "");

	g_playerManager->OnClientDisconnect(slot);

	return {KHook::Action::Ignore};
}

KHook::Return<void> CHookManager::Hook_ClientPutInServer_Post(IServerGameClients* pThis, CPlayerSlot slot, const char* pszName, int type, uint64 xuid)
{
	Message("Hook_ClientPutInServer(%d, \"%s\", %d, %d, %lli)\n", slot, pszName, type, xuid);

	if (!g_playerManager->GetPlayer(slot))
		return {KHook::Action::Ignore};

	g_playerManager->OnClientPutInServer(slot);

	if (g_cvarEnableZR.Get())
		ZR_Hook_ClientPutInServer(slot, pszName, type, xuid);

	return {KHook::Action::Ignore};
}

KHook::Return<void> CHookManager::Hook_ClientSettingsChanged(IServerGameClients* pThis, CPlayerSlot slot)
{
#ifdef _DEBUG
	Message("Hook_ClientSettingsChanged(%d)\n", slot);
#endif

	return {KHook::Action::Ignore};
}

KHook::Return<void> CHookManager::Hook_OnClientConnected(IServerGameClients* pThis, CPlayerSlot slot, const char* pszName, uint64 xuid, const char* pszNetworkID, const char* pszAddress, bool bFakePlayer)
{
	Message("Hook_OnClientConnected(%d, \"%s\", %lli, \"%s\", \"%s\", %d)\n", slot, pszName, xuid, pszNetworkID, pszAddress, bFakePlayer);

	static ConVarRefAbstract tv_name("tv_name");
	const char* pszTvName = tv_name.GetString().Get();

	if (bFakePlayer && V_strcmp(pszName, pszTvName))
		g_playerManager->OnBotConnected(slot);

	return {KHook::Action::Ignore};
}

KHook::Return<bool> CHookManager::Hook_ClientConnect(IServerGameClients* pThis, CPlayerSlot slot, const char* pszName, uint64 xuid, const char* pszNetworkID, bool unk1, CBufferString* pRejectReason)
{
	Message("Hook_ClientConnect(%d, \"%s\", %lli, \"%s\", %d, \"%s\")\n", slot, pszName, xuid, pszNetworkID, unk1, pRejectReason->Get());

	if (!g_playerManager->OnClientConnected(slot, xuid, pszNetworkID))
		return {KHook::Action::Supersede, false};

	return {KHook::Action::Ignore};
}

KHook::Return<void> CHookManager::Hook_ClientCommand(IServerGameClients* pThis, CPlayerSlot slot, const CCommand& args)
{
#ifdef _DEBUG
	Message("Hook_ClientCommand(%d, \"%s\")\n", slot, args.GetCommandString());
#endif

	if (g_cvarIdleKickTime.Get() > 0.0f)
	{
		ZEPlayer* pPlayer = g_playerManager->GetPlayer(slot);

		if (pPlayer)
			pPlayer->UpdateLastInputTime();
	}

	if (g_cvarVoteManagerEnable.Get() && V_stricmp(args[0], "endmatch_votenextmap") == 0 && args.ArgC() == 2)
	{
		if (g_pMapVoteSystem->RegisterPlayerVote(slot, atoi(args[1])))
			return {KHook::Action::Ignore};
		else
			return {KHook::Action::Supersede};
	}

	if (g_cvarEnableZR.Get() && slot != -1 && !V_strncmp(args.Arg(0), "jointeam", 8))
	{
		ZR_Hook_ClientCommand_JoinTeam(slot, args);
		return {KHook::Action::Supersede};
	}

	return {KHook::Action::Ignore};
}

KHook::Return<void> CHookManager::Hook_PostEventAbstract(IGameEventSystem* pThis, CSplitScreenSlot nSlot, bool bLocalOnly, int nClientCount, const uint64* clients, INetworkMessageInternal* pEvent, const CNetMessage* pData, unsigned long nSize, NetChannelBufType_t bufType)
{
	NetMessageInfo_t* info = pEvent->GetNetMessageInfo();

	if (g_cvarEnableStopSound.Get() && info->m_MessageId == GE_FireBulletsId)
	{
		if (g_playerManager->GetSilenceSoundMask())
		{
			auto msg = const_cast<CNetMessage*>(pData)->ToPB<CMsgTEFireBullets>();

			int32_t weapon_id = msg->weapon_id();
			int32_t sound_type = msg->sound_type();
			int32_t item_def_index = msg->item_def_index();

			msg->set_weapon_id(0);
			msg->set_sound_type(9);
			msg->set_item_def_index(61); // weapon_usp_silencer

			uint64 clientMask = *(uint64*)clients & g_playerManager->GetSilenceSoundMask();

			m_hPostEventAbstract->CallOriginal(pThis, nSlot, bLocalOnly, nClientCount, &clientMask, pEvent, msg, nSize, bufType);

			msg->set_weapon_id(weapon_id);
			msg->set_sound_type(sound_type);
			msg->set_item_def_index(item_def_index);
		}

		*(uint64*)clients &= ~g_playerManager->GetStopSoundMask();
		*(uint64*)clients &= ~g_playerManager->GetSilenceSoundMask();
	}
	else if (info->m_MessageId == GE_PlaceDecalEvent)
	{
		*(uint64*)clients &= ~g_playerManager->GetStopDecalsMask();
	}
	else if (info->m_MessageId == GE_Source1LegacyGameEvent)
	{
		if (g_cvarEnableLeader.Get())
			Leader_PostEventAbstract_Source1LegacyGameEvent(clients, pData);
	}
	else if (info->m_MessageId == UM_Shake)
	{
		auto pPBData = const_cast<CNetMessage*>(pData)->ToPB<CUserMessageShake>();
		if (g_cvarMaxShakeAmp.Get() >= 0 && pPBData->amplitude() > g_cvarMaxShakeAmp.Get())
			pPBData->set_amplitude(g_cvarMaxShakeAmp.Get());

		if (g_cvarEnableNoShake.Get())
			*(uint64*)clients &= ~g_playerManager->GetNoShakeMask();
	}
	else if (info->m_MessageId == GE_SosStartSoundEvent)
	{
		auto msg = const_cast<CNetMessage*>(pData)->ToPB<CMsgSosStartSoundEvent>();

		if (g_cvarEnableZR.Get())
			ZR_PostEventAbstract_SosStartSoundEvent(clients, msg);

		if (g_cvarEnableStopSound.Get())
		{
			static std::set<uint32> soundEventHashes;

			ExecuteOnce(
				soundEventHashes.insert(GetSoundEventHash("Weapon_sg556.ZoomIn"));
				soundEventHashes.insert(GetSoundEventHash("Weapon_sg556.ZoomOut"));
				soundEventHashes.insert(GetSoundEventHash("Weapon_AUG.ZoomIn"));
				soundEventHashes.insert(GetSoundEventHash("Weapon_AUG.ZoomOut"));
				soundEventHashes.insert(GetSoundEventHash("Weapon_SSG08.Zoom"));
				soundEventHashes.insert(GetSoundEventHash("Weapon_SSG08.ZoomOut"));
				soundEventHashes.insert(GetSoundEventHash("Weapon_SCAR20.Zoom"));
				soundEventHashes.insert(GetSoundEventHash("Weapon_SCAR20.ZoomOut"));
				soundEventHashes.insert(GetSoundEventHash("Weapon_G3SG1.Zoom"));
				soundEventHashes.insert(GetSoundEventHash("Weapon_G3SG1.ZoomOut"));
				soundEventHashes.insert(GetSoundEventHash("Weapon_AWP.Zoom"));
				soundEventHashes.insert(GetSoundEventHash("Weapon_AWP.ZoomOut"));
				soundEventHashes.insert(GetSoundEventHash("Weapon_Revolver.Prepare"));
				soundEventHashes.insert(GetSoundEventHash("Weapon.AutoSemiAutoSwitch")););

			if (!soundEventHashes.contains(msg->soundevent_hash()))
				return {KHook::Action::Ignore};

			uint64 stopSoundMask = g_playerManager->GetStopSoundMask();
			uint64 silenceSoundMask = g_playerManager->GetSilenceSoundMask();

			if (!msg->has_source_entity_index())
				return {KHook::Action::Ignore};

			CBaseEntity* pSourceEntity = (CBaseEntity*)g_pEntitySystem->GetEntityInstance(CEntityIndex(msg->source_entity_index()));
			int playerSlot = -1;

			if (!pSourceEntity)
				return {KHook::Action::Ignore};

			if (pSourceEntity->IsPawn() && ((CCSPlayerPawn*)pSourceEntity)->GetController())
			{
				playerSlot = ((CCSPlayerPawn*)pSourceEntity)->GetController()->GetPlayerSlot();
			}
			else if (!V_strncasecmp(pSourceEntity->GetClassname(), "weapon_", 7))
			{
				CCSPlayerPawn* pPawn = (CCSPlayerPawn*)pSourceEntity->m_hOwnerEntity().Get();

				if (pPawn && pPawn->IsPawn() && pPawn->GetController())
					playerSlot = pPawn->GetController()->GetPlayerSlot();
			}

			if (playerSlot != -1 && g_playerManager->IsPlayerUsingStopSound(playerSlot))
				stopSoundMask &= ~((uint64)1 << playerSlot);

			if (playerSlot != -1 && g_playerManager->IsPlayerUsingSilenceSound(playerSlot))
				silenceSoundMask &= ~((uint64)1 << playerSlot);

			*(uint64*)clients &= ~stopSoundMask;
			*(uint64*)clients &= ~silenceSoundMask;
		}
	}
	else if (info->m_MessageId == UM_ParticleManager)
	{
		if (g_cvarBlockParticleMsgs.Get())
			*(uint64*)clients = 0;
	}

	return {KHook::Action::Ignore};
}

KHook::Return<void> CHookManager::Hook_StartupServer_Post(INetworkServerService* pThis, const GameSessionConfiguration_t& config, ISource2WorldSession* pSession, const char* pszMapName)
{
	g_pEntitySystem = GameEntitySystem();
	g_pEntitySystem->AddListenerEntity(g_pEntityListener);

	if (GetNetworkGameServer())
		m_hSetGameSpawnGroupMgr->Add(GetNetworkGameServer());

	Message("Hook_StartupServer: %s\n", pszMapName);

	RegisterEventListeners();

	if (g_bHasTicked)
		RemoveTimers(TIMERFLAG_MAP);

	g_bHasTicked = false;

	g_pPanoramaVoteHandler->Reset();
	g_pVoteManager->VoteManager_Init();
	g_pIdleSystem->Reset();

	INetworkStringTable* pInfoPanelTable = g_pNetworkStringTableServer->FindTable("InfoPanel");

	if (pInfoPanelTable && V_strcmp(g_cvarMotdUrl.Get(), ""))
	{
		SetStringUserDataRequest_t pUserData;
		pUserData.m_pRawData = (void*)g_cvarMotdUrl.Get().Get();
		pUserData.m_cbDataSize = g_cvarMotdUrl.Get().Length() + 1;

		pInfoPanelTable->AddString(true, "motd", &pUserData);
	}

	return {KHook::Action::Ignore};
}

KHook::Return<void> CHookManager::Hook_CheckTransmit_Post(ISource2GameEntities* pThis, CCheckTransmitInfo** ppInfoList, int infoCount, CBitVec<16384>& unionTransmitEdicts, CBitVec<16384>&, const Entity2Networkable_t** pNetworkables, const uint16* pEntityIndicies, int nEntities)
{
	if (!g_pEntitySystem || !GetGlobals())
		return {KHook::Action::Ignore};

	VPROF("CS2Fixes::Hook_CheckTransmit");

	for (int i = 0; i < infoCount; i++)
	{
		auto& pInfo = ppInfoList[i];

		static int offset = g_GameConfig->GetOffset("CheckTransmitPlayerSlot");
		int iPlayerSlot = (int)*((uint8*)pInfo + offset);

		CCSPlayerController* pSelfController = CCSPlayerController::FromSlot(iPlayerSlot);

		if (!pSelfController || !pSelfController->IsConnected())
			continue;

		auto pSelfZEPlayer = g_playerManager->GetPlayer(iPlayerSlot);

		if (!pSelfZEPlayer)
			continue;

		for (int j = 0; j < GetGlobals()->maxClients; j++)
		{
			CCSPlayerController* pController = CCSPlayerController::FromSlot(j);
			if (!pController || pController->m_bIsHLTV || j == iPlayerSlot)
				continue;

			CBarnLight* pFlashLight = pController->IsConnected() ? g_playerManager->GetPlayer(j)->GetFlashLight() : nullptr;

			if (!g_cvarFlashLightTransmitOthers.Get() && pFlashLight)
				pInfo->m_pTransmitEntity->Clear(pFlashLight->entindex());

			if (g_cvarEnableEntWatch.Get() && g_pEWHandler->IsConfigLoaded())
			{
				CPointWorldText* pHud = pController->IsConnected() ? g_playerManager->GetPlayer(j)->GetEntwatchHud() : nullptr;
				if (pHud)
					pInfo->m_pTransmitEntity->Clear(pHud->entindex());
			}

			if (!g_cvarEnableHide.Get() || pSelfController->GetPawnState() == STATE_OBSERVER_MODE)
				continue;

			CCSPlayerPawn* pPawn = pController->GetPlayerPawn();

			if (!pPawn)
				continue;

			ZEPlayer* pOtherZEPlayer = g_playerManager->GetPlayer(j);
			if (pSelfZEPlayer->ShouldBlockTransmit(j) && pOtherZEPlayer && !pOtherZEPlayer->IsLeader() && g_pEWHandler->FindItemInstanceByOwner(j, false, 0) == -1)
			{
				pInfo->m_pTransmitEntity->Clear(pPawn->entindex());

				if (g_cvarHideWeapons.Get())
				{
					auto pVecWeapons = pPawn->m_pWeaponServices->m_hMyWeapons();

					FOR_EACH_VEC(*pVecWeapons, i)
					{
						auto pWeapon = (*pVecWeapons)[i].Get();

						if (pWeapon)
							pInfo->m_pTransmitEntity->Clear(pWeapon->entindex());
					}
				}
			}
		}

		CBaseModelEntity* pGlowModel = pSelfZEPlayer->GetGlowModel();

		if (pGlowModel)
			pInfo->m_pTransmitEntity->Clear(pGlowModel->entindex());
	}

	return {KHook::Action::Ignore};
}

KHook::Return<void> CHookManager::Hook_DispatchConCommand(ICvar* pThis, ConCommandRef cmdHandle, const CCommandContext& ctx, const CCommand& args)
{
	VPROF_BUDGET("CS2Fixes::Hook_DispatchConCommand", "ConCommands");

	if (!g_pEntitySystem)
		return {KHook::Action::Ignore};

	auto iCommandPlayerSlot = ctx.GetPlayerSlot();

	if (!g_cvarEnableCommands.Get())
		return {KHook::Action::Ignore};

	bool bSay = !V_strcmp(args.Arg(0), "say");
	bool bTeamSay = !V_strcmp(args.Arg(0), "say_team");

	if (iCommandPlayerSlot != -1 && (bSay || bTeamSay))
	{
		auto pController = CCSPlayerController::FromSlot(iCommandPlayerSlot);
		bool bGagged = pController && pController->GetZEPlayer()->IsGagged();
		bool bFlooding = pController && pController->GetZEPlayer()->IsFlooding();
		bool bIsAdmin = pController && pController->GetZEPlayer()->IsAdminFlagSet(ADMFLAG_GENERIC);
		bool bAdminChat = bTeamSay && *args[1] == '@';
		bool bSilent = *args[1] == '/' || bAdminChat;
		bool bCommand = *args[1] == '!' || *args[1] == '/';

		if (pController)
		{
			IGameEvent* pEvent = g_gameEventManager->CreateEvent("player_chat");

			if (pEvent)
			{
				pEvent->SetBool("teamonly", bTeamSay);
				pEvent->SetInt("userid", pController->GetPlayerSlot());
				pEvent->SetString("text", args[1]);

				g_gameEventManager->FireEvent(pEvent, true);
			}
		}

		if (!bGagged && !bSilent && !bFlooding)
		{
			m_hDispatchConCommand->CallOriginal(pThis, cmdHandle, ctx, args);
		}
		else if (bFlooding)
		{
			if (pController)
				ClientPrint(pController, HUD_PRINTTALK, CHAT_PREFIX "You are flooding the server!");
		}
		else if (bAdminChat && GetGlobals())
		{
			char* pszMessage = (char*)(args.ArgS() + 2);
			pszMessage[V_strlen(pszMessage) - 1] = 0;

			for (int i = 0; i < GetGlobals()->maxClients; i++)
			{
				ZEPlayer* pPlayer = g_playerManager->GetPlayer(i);

				if (!pPlayer)
					continue;

				if (i == iCommandPlayerSlot.Get() || pPlayer->IsAdminFlagSet(ADMFLAG_GENERIC))
					ClientPrint(CCSPlayerController::FromSlot(i), HUD_PRINTTALK, " \4(%sADMINS) %s:\6 %s", bIsAdmin ? "" : "TO ", pController->GetPlayerName().c_str(), pszMessage);
			}
		}

		if (bCommand)
		{
			char* pszMessage = (char*)(args.ArgS() + 1);

			if (pszMessage[0] == '"' || pszMessage[0] == '!' || pszMessage[0] == '/')
				pszMessage += 1;

			if ((bGagged || bSilent || bFlooding) && pszMessage[V_strlen(pszMessage) - 1] == '"')
				pszMessage[V_strlen(pszMessage) - 1] = '\0';

			ParseChatCommand(pszMessage, pController);
		}

		return {KHook::Action::Supersede};
	}

	return {KHook::Action::Ignore};
}

KHook::Return<int> CHookManager::Hook_LoadEventsFromFile(IGameEventManager2* pThis, const char* filename, bool bSearchAll)
{
	ExecuteOnce(g_gameEventManager = pThis);

	return {KHook::Action::Ignore};
}

KHook::Return<void> CHookManager::Hook_Spawn_Post(CEntitySystem* pThis, int nCount, const EntitySpawnInfo_t* pInfo)
{
	for (int i = 0; i < nCount; i++)
		g_pMapMigrations->OnEntitySpawned(pInfo[i].m_pEntity->m_pInstance, pInfo[i].m_pKeyValues);

	return {KHook::Action::Ignore};
}

KHook::Return<void> CHookManager::Hook_SetGameSpawnGroupMgr(INetworkGameServer* pThis, IGameSpawnGroupMgr* pSpawnGroupMgr)
{
	g_pSpawnGroupMgr = (CSpawnGroupMgrGameSystem*)pSpawnGroupMgr;

	return {KHook::Action::Ignore};
}

KHook::Return<void> CHookManager::Hook_CreateWorkshopMapGroup(IGameTypes* pThis, const char* name, const CUtlStringList& mapList)
{
	if (g_cvarVoteManagerEnable.Get() && g_pMapVoteSystem->IsMapListLoaded())
		return KHook::Recall<void (IGameTypes::*)(const char*, const CUtlStringList&)>(nullptr, {KHook::Action::Ignore}, pThis, name, g_pMapVoteSystem->CreateWorkshopMapGroup());

	return {KHook::Action::Ignore};
}

KHook::Return<void> CHookManager::Hook_GetTouchingList_Post(CVPhys2World* pThis, CUtlVector<TouchLinked_t>* pList, bool unknown)
{
	if (!g_cvarFixPhysicsPlayerShuffle.Get() || pList->Count() <= 1)
		return {KHook::Action::Ignore};

	if (GetGlobals())
		std::srand(GetGlobals()->tickcount);

	std::vector<TouchLinked_t> touchingLinks;
	std::vector<TouchLinked_t> unTouchLinks;

	FOR_EACH_VEC(*pList, i)
	{
		const auto& link = pList->Element(i);
		if (link.IsUnTouching())
			unTouchLinks.push_back(link);
		else
			touchingLinks.push_back(link);
	}

	if (touchingLinks.size() <= 1)
		return {KHook::Action::Ignore};

	for (size_t i = touchingLinks.size() - 1; i > 0; --i)
	{
		const auto j = std::rand() % (i + 1);
		std::swap(touchingLinks[i], touchingLinks[j]);
	}

	pList->Purge();

	for (const auto& link : touchingLinks)
		pList->AddToTail(link);
	for (const auto& link : unTouchLinks)
		pList->AddToTail(link);

	return {KHook::Action::Ignore};
}

KHook::Return<void> CHookManager::Hook_CheckMovingGround(CCSPlayer_MovementServices* pThis, double frametime)
{
	CCSPlayerPawn* pPawn = pThis->GetPawn();

	if (!pPawn || !GetGlobals())
		return {KHook::Action::Ignore};

	CCSPlayerController* pController = pPawn->GetOriginalController();

	if (!pController)
		return {KHook::Action::Ignore};

	int iSlot = pController->GetPlayerSlot();

	static int aPlayerTicks[MAXPLAYERS] = {0};

	if (aPlayerTicks[iSlot] == GetGlobals()->tickcount)
		return {KHook::Action::Supersede};

	aPlayerTicks[iSlot] = GetGlobals()->tickcount;

	return {KHook::Action::Ignore};
}

KHook::Return<void> CHookManager::Hook_DropWeapon_Post(CCSPlayer_WeaponServices* pThis, CBasePlayerWeapon* pWeapon, Vector* pVecTarget, Vector* pVelocity)
{
	if (g_cvarEnableEntWatch.Get())
		EW_DropWeapon(pThis, pWeapon);

	return {KHook::Action::Ignore};
}

KHook::Return<void> CHookManager::Hook_PlayerEquipUse(CGamePlayerEquip* pThis, InputData_t* pInput)
{
	CGamePlayerEquipHandler::Use(pThis, pInput);

	return {KHook::Action::Ignore};
}

KHook::Return<void> CHookManager::Hook_PlayerEquipPrecache_Post(CGamePlayerEquip* pThis, CEntityPrecacheContext* param)
{
	const auto kv = param->m_pKeyValues;
	CGamePlayerEquipHandler::OnPrecache(pThis, kv);

	return {KHook::Action::Ignore};
}

KHook::Return<void> CHookManager::Hook_TriggerGravityPrecache_Post(CTriggerGravity* pThis, CEntityPrecacheContext* param)
{
	const auto kv = param->m_pKeyValues;
	CTriggerGravityHandler::OnPrecache(pThis, kv);

	return {KHook::Action::Ignore};
}

KHook::Return<void> CHookManager::Hook_TriggerGravityEndTouch_Post(CTriggerGravity* pThis, CBaseEntity* pOther)
{
	CTriggerGravityHandler::OnEndTouch(pThis, pOther);

	return {KHook::Action::Ignore};
}

KHook::Return<bool> CHookManager::Hook_OnTakeDamage_Alive(CCSPlayerPawn* pPawn, CTakeDamageResult* pDamageResult)
{
	if (g_cvarEnableZR.Get() && ZR_Hook_OnTakeDamage_Alive(pDamageResult->m_pOriginatingInfo, pPawn))
	{
		pDamageResult->m_bWasDamageSuppressed = true;
		pDamageResult->m_flDamageDealt = 0.0f;
		return {KHook::Action::Supersede, false};
	}

	if (g_cvarDropMapWeapons.Get() && pPawn && pPawn->m_iHealth() <= 0)
	{
		if (g_cvarEnableEntWatch.Get())
		{
			CCSPlayerController* pController = pPawn->GetOriginalController();
			if (pController)
				EW_PlayerDeathPre(pController);
		}

		pPawn->DropMapWeapons();
	}

	return {KHook::Action::Ignore};
}

KHook::Return<void> CHookManager::Hook_CCSPlayerPawn_Teleport(CCSPlayerPawn* pPawn, const Vector* pPosition, const QAngle* pAngles, const Vector* pVelocity)
{
	if (!pAngles)
		return {KHook::Action::Ignore};

	QAngle* pCastAngles = const_cast<QAngle*>(pAngles);

	if (pCastAngles->x != 0.0f)
		pCastAngles->x = 0.0f;

	if (pCastAngles->z != 0.0f)
		pCastAngles->z = 0.0f;

	return {KHook::Action::Ignore};
}

KHook::Return<int64> CHookManager::Hook_TakeDamageOld(CBaseEntity* pThis, CTakeDamageInfo* pInfo, CTakeDamageResult* pResult)
{
	// NOTE valve always return 1 here, since 2025/10/15 update

#ifdef _DEBUG
    Message("\n--------------------------------\nTakeDamage on %s\nAttacker: %s\nInflictor: %s\nAbility: %s\nDamage: %.2f\nDamage Type: %i\n--------------------------------\n", pThis->GetClassname(), pInfo->m_hAttacker.Get() ? pInfo->m_hAttacker.Get()->GetClassname() : "NULL", pInfo->m_hInflictor.Get() ? pInfo->m_hInflictor.Get()->GetClassname() : "NULL", pInfo->m_hAbility.Get() ? pInfo->m_hAbility.Get()->GetClassname() : "NULL", pInfo->m_flDamage, pInfo->m_bitsDamageType);
#endif

	if (g_cvarBlockAllDamage.Get() && pThis->IsPawn()) return {KHook::Action::Supersede, 1};

	CEntityInstance* pInflictor = pInfo->m_hInflictor.Get();
	const char* pszInflictorClass = pInflictor ? pInflictor->GetClassname() : "";

	if (g_cvarFixBlockDamage.Get() && pInfo->m_AttackerInfo.m_bIsPawn && pInfo->m_bitsDamageType ^ DMG_BULLET && pInfo->m_hAttacker != pThis->GetHandle())
	{
		if (V_strcasecmp(pszInflictorClass, "func_movelinear") == 0 || V_strcasecmp(pszInflictorClass, "func_mover") == 0 || V_strcasecmp(pszInflictorClass, "func_door") == 0 || V_strcasecmp(pszInflictorClass, "func_door_rotating") == 0 || V_strcasecmp(pszInflictorClass, "func_rotating") == 0 || V_strcasecmp(pszInflictorClass, "point_hurt") == 0)
		{
			pInfo->m_AttackerInfo.m_bIsPawn = false;
			pInfo->m_AttackerInfo.m_bIsWorld = true;
			pInfo->m_hAttacker = pInfo->m_hInflictor;

			pInfo->m_AttackerInfo.m_hAttackerPawn = CHandle<CCSPlayerPawn>(~0u);
			pInfo->m_AttackerInfo.m_nAttackerPlayerSlot = ~0;
		}
	}

	if (g_cvarBlockMolotovSelfDmg.Get() && pInfo->m_hAttacker == pThis && !V_strncmp(pszInflictorClass, "inferno", 7)) return {KHook::Action::Supersede, 1};

	if (!V_strcasecmp(pszInflictorClass, "hegrenade_projectile") && pInfo->m_AttackerInfo.m_bIsPawn && pInfo->m_AttackerInfo.m_nTeam == 0) return {KHook::Action::Supersede, 1};

	CTakeDamageResult damageResult(0);

	if (pResult == nullptr)
	{
		damageResult.CopyFrom(pInfo);
		return KHook::Recall<int64 (CBaseEntity::*)(CTakeDamageInfo*, CTakeDamageResult*)>(nullptr, {KHook::Action::Ignore}, pThis, pInfo, &damageResult);
	}

	return {KHook::Action::Ignore};
}

KHook::Return<int64> CHookManager::Hook_TakeDamageOld_Post(CBaseEntity* pThis, CTakeDamageInfo* pInfo, CTakeDamageResult* pResult)
{
	if (pResult->m_flDamageDealt > 0.0f && !pResult->m_bWasDamageSuppressed && g_cvarEnableZR.Get() && pThis->IsPawn()) ZR_OnPlayerTakeDamage(reinterpret_cast<CCSPlayerPawn*>(pThis), pInfo, pResult->m_flDamageDealt);

	return {KHook::Action::Ignore};
}

KHook::Return<void> CHookManager::Hook_TriggerPushTouch(CTriggerPush* pPush, CBaseEntity* pOther)
{
	if (!g_cvarUseOldPush.Get() || pPush->m_spawnflags() & SF_TRIG_PUSH_ONCE || pPush->m_bTriggerOnStartTouch()) return {KHook::Action::Ignore};

	MoveType_t movetype = pOther->m_nActualMoveType();

	if (movetype == MOVETYPE_VPHYSICS) return {KHook::Action::Ignore};

	if (movetype == MOVETYPE_NONE || movetype == MOVETYPE_PUSH || movetype == MOVETYPE_NOCLIP) return {KHook::Action::Supersede};

	CCollisionProperty* collisionProp = pOther->m_pCollision();
	if (!IsSolid(collisionProp->m_nSolidType(), collisionProp->m_usSolidFlags())) return {KHook::Action::Supersede};

	if (!pPush->PassesTriggerFilters(pOther)) return {KHook::Action::Supersede};

	if (pOther->m_CBodyComponent()->m_pSceneNode()->m_pParent()) return {KHook::Action::Supersede};

	Vector vecAbsDir;
	matrix3x4_t matTransform = pPush->m_CBodyComponent()->m_pSceneNode()->EntityToWorldTransform();

	Vector vecPushDir = pPush->m_vecPushDirEntitySpace();
	VectorRotate(vecPushDir, matTransform, vecAbsDir);

	Vector vecPush = vecAbsDir * pPush->m_flSpeed();

	uint32 flags = pOther->m_fFlags();

	if (flags & FL_BASEVELOCITY)
		vecPush = vecPush + pOther->m_vecBaseVelocity();

	if (vecPush.z > 0 && (flags & FL_ONGROUND))
	{
		pOther->SetGroundEntity(nullptr);
		Vector origin = pOther->GetAbsOrigin();
		origin.z += 1.0f;

		pOther->Teleport(&origin, nullptr, nullptr);
	}

	if (g_cvarLogPushes.Get() && GetGlobals())
	{
		Vector vecEntBaseVelocity = pOther->m_vecBaseVelocity;
		Vector vecOrigPush = vecAbsDir * pPush->m_flSpeed();

		Message("Pushing entity %i | frame = %i | tick = %i | entity basevelocity %s = %.2f %.2f %.2f | original push velocity = %.2f %.2f %.2f | final push velocity = %.2f %.2f %.2f\n",  pOther->GetEntityIndex(), GetGlobals()->framecount, GetGlobals()->tickcount, (flags & FL_BASEVELOCITY) ? "WITH FLAG" : "", vecEntBaseVelocity.x, vecEntBaseVelocity.y, vecEntBaseVelocity.z, vecOrigPush.x, vecOrigPush.y, vecOrigPush.z, vecPush.x, vecPush.y, vecPush.z);
	}

	pOther->m_vecBaseVelocity(vecPush);

	flags |= FL_BASEVELOCITY;
	pOther->m_fFlags(flags);

	return {KHook::Action::Supersede};
}

KHook::Return<bool> CHookManager::Hook_IsHearingClient(void* serverClient, int index)
{
	ZEPlayer* player = g_playerManager->GetPlayer(index);
	if (player && player->IsMuted()) return {KHook::Action::Supersede, false};

	return {KHook::Action::Ignore};
}

static KHook::Return<void> SayChatMessageWithTimer(IRecipientFilter& filter, const char* pText, CCSPlayerController* pPlayer, uint64 eMessageType)
{
	VPROF("SayChatMessageWithTimer");

	char buf[256];

	uint32 uiTextLength = strlen(pText);
	uint32 uiFilteredTextLength = 0;
	char filteredText[256];

	for (uint32 i = 0; i < uiTextLength; i++)
	{
		if (pText[i] >= 'A' && pText[i] <= 'Z') filteredText[uiFilteredTextLength++] = pText[i] + 32;
		if (pText[i] == ' ' || (pText[i] >= '0' && pText[i] <= '9') || (pText[i] >= 'a' && pText[i] <= 'z')) filteredText[uiFilteredTextLength++] = pText[i];
	}
	filteredText[uiFilteredTextLength] = '\0';

	CSplitString words(filteredText, " ");

	int iWordCount = words.Count();
	uint32 uiTriggerTimerLength = 0;

	if (iWordCount == 2) uiTriggerTimerLength = V_StringToUint32(words.Element(1), 0, NULL, NULL, PARSING_FLAG_SKIP_WARNING);

	for (int i = 1; i < iWordCount && uiTriggerTimerLength == 0; i++)
	{
		uint32 uiCurrentValue = V_StringToUint32(words.Element(i), 0, NULL, NULL, PARSING_FLAG_SKIP_WARNING);
		uint32 uiNextWordLength = 0;
		char* pNextWord = NULL;

		if (i + 1 < iWordCount)
		{
			pNextWord = words.Element(i + 1);
			uiNextWordLength = strlen(pNextWord);
		}

		if (pNextWord != NULL && uiCurrentValue > 0)
		{
			if (uiNextWordLength == 1)
			{
				if (pNextWord[0] == 's') uiTriggerTimerLength = uiCurrentValue;
			}
			else if (uiNextWordLength > 2)
			{
				if (pNextWord[0] == 's' && pNextWord[1] == 'e' && pNextWord[2] == 'c') uiTriggerTimerLength = uiCurrentValue;
				if (pNextWord[0] == 'm' && pNextWord[1] == 'i' && pNextWord[2] == 'n') uiTriggerTimerLength = uiCurrentValue * 60;
			}
		}

		if (uiCurrentValue == 0)
		{
			char* pCurrentWord = words.Element(i);
			uint32 uiCurrentScanLength = MIN(strlen(pCurrentWord), 4);

			for (uint32 j = 0; j < uiCurrentScanLength; j++)
			{
				if (pCurrentWord[j] >= '0' && pCurrentWord[j] <= '9') continue;

				if (pCurrentWord[j] == 's')
				{
					pCurrentWord[j] = '\0';
					uiTriggerTimerLength = V_StringToUint32(pCurrentWord, 0, NULL, NULL, PARSING_FLAG_SKIP_WARNING);
				}
				break;
			}
		}
	}

	float fCurrentRoundClock = g_pGameRules->m_iRoundTime - (GetGlobals()->curtime - g_pGameRules->m_fRoundStartTime.Get().GetTime());

	if ((uiTriggerTimerLength > 4) && (fCurrentRoundClock > uiTriggerTimerLength))
	{
		int iTriggerTime = fCurrentRoundClock - uiTriggerTimerLength;

		if ((int)(fCurrentRoundClock - 0.5f) == (int)fCurrentRoundClock) iTriggerTime++;

		int mins = iTriggerTime / 60;
		int secs = iTriggerTime % 60;

		V_snprintf(buf, sizeof(buf), "%s %s %s %2d:%02d", " \7CONSOLE:\4", pText + sizeof("Console:"), "\x10- @", mins, secs);
	}
	else V_snprintf(buf, sizeof(buf), "%s %s", " \7CONSOLE:\4", pText + sizeof("Console:"));

	return KHook::Recall<void (*)(IRecipientFilter&, const char*, CCSPlayerController*, uint64)>(nullptr, {KHook::Action::Ignore}, filter, buf, pPlayer, eMessageType);
}

KHook::Return<void> CHookManager::Hook_SayTextFilter(IRecipientFilter& filter, const char* pText, CCSPlayerController* pPlayer, uint64 eMessageType)
{
	if (pPlayer) return {KHook::Action::Ignore};

	if (g_cvarEnableTriggerTimer.Get() && GetGlobals() && g_pGameRules) return SayChatMessageWithTimer(filter, pText, pPlayer, eMessageType);

	char buf[256];
	V_snprintf(buf, sizeof(buf), "%s %s", " \7CONSOLE:\4", pText + sizeof("Console:"));

	return KHook::Recall<void (*)(IRecipientFilter&, const char*, CCSPlayerController*, uint64)>(nullptr, {KHook::Action::Ignore}, filter, buf, pPlayer, eMessageType);
}

KHook::Return<void> CHookManager::Hook_SayText2Filter(IRecipientFilter& filter,  CCSPlayerController* pEntity, uint64 eMessageType, const char* msg_name, const char* param1,  const char* param2, const char* param3, const char* param4)
{
#ifdef _DEBUG
    CPlayerSlot slot = filter.GetRecipientIndex(0);
    CCSPlayerController* target = CCSPlayerController::FromSlot(slot);

    if (target)
        Message("Chat from %s to %s: %s\n", param1, target->GetPlayerName().c_str(), param2);
#endif

	return KHook::Recall<void (*)(IRecipientFilter&, CCSPlayerController*, uint64, const char*, const char*, const char*, const char*, const char*)>(nullptr, {KHook::Action::Ignore}, filter, pEntity, eMessageType, msg_name, pEntity->GetPlayerName().c_str(), param2, param3, param4);
}

KHook::Return<bool> CHookManager::Hook_CanUse(CCSPlayer_WeaponServices* pWeaponServices, CBasePlayerWeapon* pPlayerWeapon)
{
	if (g_cvarEnableEntWatch.Get() && !EW_Detour_CCSPlayer_WeaponServices_CanUse(pWeaponServices, pPlayerWeapon)) return {KHook::Action::Supersede, false};

	return {KHook::Action::Ignore};
}

KHook::Return<void> CHookManager::Hook_EquipWeapon(CCSPlayer_WeaponServices* pWeaponServices, CBasePlayerWeapon* pPlayerWeapon)
{
	if (g_cvarEnableEntWatch.Get()) EW_Detour_CCSPlayer_WeaponServices_EquipWeapon(pWeaponServices, pPlayerWeapon);

	g_pMapMigrations->OnEquipWeapon(pPlayerWeapon);

	return {KHook::Action::Ignore};
}

static bool PrepareMapSetModel(CBaseModelEntity* pModel)
{
	if (!pModel->IsPawn()) return true;

	if (g_cvarDisableSetModel.Get() || g_pMapMigrations->Migrations20260420Enabled()) return false;

	int originalAlpha = pModel->m_clrRender().a();
	pModel->m_clrRender = Color(255, 255, 255, originalAlpha);

	return true;
}

KHook::Return<bool> CHookManager::Hook_AcceptInput(CEntityIdentity* pThis, CUtlSymbolLarge* pInputName, CEntityInstance* pActivator, CEntityInstance* pCaller, variant_t* value, int nOutputID, void* a7, void* a8)
{
	VPROF_SCOPE_BEGIN("CHookManager::Hook_AcceptInput");

		if (g_cvarEnableZR.Get())
		{
			bool result = ZR_Detour_CEntityIdentity_AcceptInput(pThis, pInputName, pActivator, pCaller, value, nOutputID);

			if (!result) return {KHook::Action::Supersede, result};
		}

		if (!V_strnicmp(pInputName->String(), "KeyValue", 8))
		{
			if ((value->m_type == FIELD_CSTRING || value->m_type == FIELD_STRING) && value->m_pszString)
			{
				return {KHook::Action::Supersede, CustomIO_HandleInput(pThis->m_pInstance, value->m_pszString, pActivator, pCaller)};
			}
			Message("Invalid value type for input %s\n", pInputName->String());
			return {KHook::Action::Supersede, false};
		}

		if (!V_strnicmp(pInputName->String(), "IgniteL", 7))
		{
			float flDuration = 0.f;

			if ((value->m_type == FIELD_CSTRING || value->m_type == FIELD_STRING) && value->m_pszString) flDuration = V_StringToFloat32(value->m_pszString, 0.f);
			else flDuration = value->m_float32;

			CCSPlayerPawn* pPawn = reinterpret_cast<CCSPlayerPawn*>(pThis->m_pInstance);

			if (pPawn->IsPawn() && IgnitePawn(pPawn, flDuration, pPawn, pPawn)) return {KHook::Action::Supersede, true};
		}
		else if (!V_strnicmp(pInputName->String(), "AddScore", 8))
		{
			int iScore = 0;

			if ((value->m_type == FIELD_CSTRING || value->m_type == FIELD_STRING) && value->m_pszString) iScore = V_StringToInt32(value->m_pszString, 0);
			else iScore = value->m_int32;

			CCSPlayerPawn* pPawn = reinterpret_cast<CCSPlayerPawn*>(pThis->m_pInstance);

			if (pPawn->IsPawn() && pPawn->GetOriginalController())
			{
				pPawn->GetOriginalController()->AddScore(iScore);
				return {KHook::Action::Supersede, true};
			}
		}
		else if (!V_strcasecmp(pInputName->String(), "SetMessage"))
		{
			if (const auto pHudHint = reinterpret_cast<CBaseEntity*>(pThis->m_pInstance)->AsHudHint())
			{
				if ((value->m_type == FIELD_CSTRING || value->m_type == FIELD_STRING) && value->m_pszString) pHudHint->m_iszMessage(GameEntitySystem()->AllocPooledString(value->m_pszString));
				return {KHook::Action::Supersede, true};
			}
		}
		else if (!V_strcasecmp(pInputName->String(), "SetModel"))
		{
			if (const auto pModelEntity = reinterpret_cast<CBaseEntity*>(pThis->m_pInstance)->AsBaseModelEntity())
			{
				if ((value->m_type == FIELD_CSTRING || value->m_type == FIELD_STRING) && value->m_pszString && PrepareMapSetModel(pModelEntity)) pModelEntity->SetModel(value->m_pszString);

				return {KHook::Action::Supersede, true};
			}
		}
		else if (const auto pGameUI = reinterpret_cast<CBaseEntity*>(pThis->m_pInstance)->AsGameUI())
		{
			if (!V_strcasecmp(pInputName->String(), "Activate")) return {KHook::Action::Supersede, CGameUIHandler::OnActivate(pGameUI, reinterpret_cast<CBaseEntity*>(pActivator))};
			if (!V_strcasecmp(pInputName->String(), "Deactivate")) return {KHook::Action::Supersede, CGameUIHandler::OnDeactivate(pGameUI, reinterpret_cast<CBaseEntity*>(pActivator))};
		}
		else if (const auto pViewControl = reinterpret_cast<CPointViewControl*>(pThis->m_pInstance)->AsPointViewControl())
		{
			if (!V_strcasecmp(pInputName->String(), "EnableCamera")) return {KHook::Action::Supersede, CPointViewControlHandler::OnEnable(pViewControl, reinterpret_cast<CBaseEntity*>(pActivator))};
			if (!V_strcasecmp(pInputName->String(), "DisableCamera")) return {KHook::Action::Supersede, CPointViewControlHandler::OnDisable(pViewControl, reinterpret_cast<CBaseEntity*>(pActivator))};
			if (!V_strcasecmp(pInputName->String(), "EnableCameraAll")) return {KHook::Action::Supersede, CPointViewControlHandler::OnEnableAll(pViewControl)};
			if (!V_strcasecmp(pInputName->String(), "DisableCameraAll")) return {KHook::Action::Supersede, CPointViewControlHandler::OnDisableAll(pViewControl)};
		}

	VPROF_SCOPE_END();

	return {KHook::Action::Ignore};
}

KHook::Return<void*> CHookManager::Hook_GetNearestNavArea(CNavMesh* pNavMesh, float* unk2, unsigned int* unk3, unsigned int unk4, int64_t unk5, float unk6, int64_t unk7)
{
	if (g_cvarBlockNavLookup.Get()) return {KHook::Action::Supersede, nullptr};

	return {KHook::Action::Ignore};
}

KHook::Return<void> CHookManager::Hook_ProcessMovement(CCSPlayer_MovementServices* pThis, void* pMove)
{
	CCSPlayerPawn* pPawn = pThis->GetPawn();

	if (!pPawn->IsAlive() || !GetGlobals()) return {KHook::Action::Ignore};

	CCSPlayerController* pController = pPawn->GetOriginalController();

	if (!pController || !pController->IsConnected()) return {KHook::Action::Ignore};

	float flSpeedMod = pController->GetZEPlayer()->GetSpeedMod();

	if (flSpeedMod == 1.f) return {KHook::Action::Ignore};

	m_flStoreFrametime = GetGlobals()->frametime;
	GetGlobals()->frametime *= flSpeedMod;

	return {KHook::Action::Ignore};
}

KHook::Return<void> CHookManager::Hook_ProcessMovement_Post(CCSPlayer_MovementServices* pThis, void* pMove)
{
	GetGlobals()->frametime = m_flStoreFrametime;
	return {KHook::Action::Ignore};
}

KHook::Return<void*> CHookManager::Hook_ProcessUsercmds(CCSPlayerController* pController, CUserCmd* cmds, int numcmds, bool paused, float margin)
{
	VPROF_SCOPE_BEGIN("CHookManager::Hook_ProcessUsercmds");

		for (int i = 0; i < numcmds; i++)
		{
			if (g_cvarDisableSubtickMovement.Get() || g_cvarUseOldPush.Get())
			{
				auto subtickMoves = cmds[i].cmd.mutable_base()->mutable_subtick_moves();
				auto iterator = subtickMoves->begin();

				while (iterator != subtickMoves->end())
				{
					uint64 button = iterator->button();

					if (button >= IN_DUCK && button <= IN_MOVERIGHT && button != IN_USE)
					{
						subtickMoves->erase(iterator);
					}
					else
					{
						if (iterator->pitch_delta() != 0.0f) iterator->set_pitch_delta(0.0f);

						if (iterator->yaw_delta() != 0.0f) iterator->set_yaw_delta(0.0f);

						iterator++;
					}
				}
			}

			if (g_cvarDisableSubtickShooting.Get())
			{
				cmds[i].cmd.set_attack1_start_history_index(-1);
				cmds[i].cmd.set_attack2_start_history_index(-1);
				cmds[i].cmd.mutable_input_history()->Clear();
			}
		}

	VPROF_SCOPE_END();

	return {KHook::Action::Ignore};
}

KHook::Return<void> CHookManager::Hook_InputTriggerForAllPlayers(CGamePlayerEquip* pEntity, InputData_t* pInput)
{
	CGamePlayerEquipHandler::TriggerForAllPlayers(pEntity, pInput);
	return {KHook::Action::Ignore};
}

KHook::Return<void> CHookManager::Hook_InputTriggerForActivatedPlayer(CGamePlayerEquip* pEntity, InputData_t* pInput)
{
	if (CGamePlayerEquipHandler::TriggerForActivatedPlayer(pEntity, pInput)) return {KHook::Action::Ignore};

	return {KHook::Action::Supersede};
}

KHook::Return<void> CHookManager::Hook_GravityTouch(CTriggerGravity* pEntity, CBaseEntity* pOther)
{
	if (CTriggerGravityHandler::GravityTouching(pEntity, pOther)) return {KHook::Action::Supersede};

	return {KHook::Action::Ignore};
}

KHook::Return<CServerSideClient*> CHookManager::Hook_GetFreeClient(int64_t unk1, const __m128i* unk2, unsigned int unk3, int64_t unk4, char unk5, void* unk6)
{
	if (!GetClientList() || !GetGlobals()) return {KHook::Action::Supersede, nullptr};

	if (GetGlobals()->maxClients != GetClientList()->Count()) return {KHook::Action::Ignore};

	for (int i = 0; i < GetClientList()->Count(); i++)
	{
		CServerSideClient* pClient = (*GetClientList())[i];

		if (pClient && pClient->GetSignonState() < SIGNONSTATE_CONNECTED) return {KHook::Action::Supersede, pClient};
	}

	return {KHook::Action::Supersede, nullptr};
}

KHook::Return<float> CHookManager::Hook_GetMaxSpeed(CCSPlayerPawn* pPawn)
{
	auto flMaxSpeed = m_hGetMaxSpeed->CallOriginal(pPawn);

	const auto pController = reinterpret_cast<CCSPlayerController*>(pPawn->GetController());
	if (const auto pPlayer = pController != nullptr ? pController->GetZEPlayer() : nullptr) flMaxSpeed *= pPlayer->GetMaxSpeed();

	return {KHook::Action::Supersede, flMaxSpeed};
}

KHook::Return<int64> CHookManager::Hook_FindUseEntity(CCSPlayer_UseServices* pThis, float a2)
{
	m_bFindingUseEntity = true;
	return {KHook::Action::Ignore};
}

KHook::Return<int64> CHookManager::Hook_FindUseEntity_Post(CCSPlayer_UseServices* pThis, float a2)
{
	m_bFindingUseEntity = false;
	return {KHook::Action::Ignore};
}

KHook::Return<bool> CHookManager::Hook_TraceFunc(int64* a1, int* a2, float* a3, uint64 traceMask)
{
	if (g_cvarPreventUsingPlayers.Get() && m_bFindingUseEntity)
	{
		uint64 newMask = traceMask & (~(CONTENTS_PLAYER & CONTENTS_NPC));
		KHook::Recall<void (*)(int64*, int*, float*, uint64)>(nullptr, {KHook::Action::Ignore}, a1, a2, a3, newMask);
	}

	return {KHook::Action::Ignore};
}

KHook::Return<bool> CHookManager::Hook_TraceShape(int64* a1, int64 a2, int64 a3, int64 a4, CTraceFilter* filter, int64 a6)
{
	if (g_cvarPreventUsingPlayers.Get() && m_bFindingUseEntity)
	{
		filter->DisableInteractsWithLayer(LAYER_INDEX_CONTENTS_PLAYER);
		filter->DisableInteractsWithLayer(LAYER_INDEX_CONTENTS_NPC);
	}

	return {KHook::Action::Ignore};
}

KHook::Return<void> CHookManager::Hook_FireOutputInternal(const CEntityIOOutput* pThis, CEntityInstance* pActivator, CEntityInstance* pCaller, const CVariant* value, float flDelay, void* a6, void* a7)
{
	if (g_cvarEnableButtonWatch.Get()) ButtonWatch(pThis, pActivator, pCaller, value, flDelay);

	if (g_cvarEnableEntWatch.Get()) EW_FireOutput(pThis, pActivator, pCaller, value, flDelay);

	return {KHook::Action::Ignore};
}

#ifdef PLATFORM_WINDOWS
KHook::Return<Vector*> CHookManager::Hook_GetEyePosition(CBasePlayerPawn* pPawn, Vector* pRet)
{
    if (pPawn->IsAlive() && CPointViewControlHandler::IsViewControl(reinterpret_cast<CCSPlayerPawn*>(pPawn)))
    {
        const auto& origin = pPawn->GetEyePosition();
        pRet->Init(origin.x, origin.y, origin.z);
        return {KHook::Action::Supersede, pRet};
    }

    return {KHook::Action::Ignore};
}

KHook::Return<QAngle*> CHookManager::Hook_GetEyeAngles(CBasePlayerPawn* pPawn, QAngle* pRet)
{
    if (pPawn->IsAlive() && CPointViewControlHandler::IsViewControl(reinterpret_cast<CCSPlayerPawn*>(pPawn)))
    {
        const auto& angles = pPawn->v_angle();
        pRet->Init(angles.x, angles.y, angles.z);
        return {KHook::Action::Supersede, pRet};
    }

    return {KHook::Action::Ignore};
}
#else
KHook::Return<Vector> CHookManager::Hook_GetEyePosition(CBasePlayerPawn* pPawn)
{
	if (pPawn->IsAlive() && CPointViewControlHandler::IsViewControl(reinterpret_cast<CCSPlayerPawn*>(pPawn)))
	{
		const auto& origin = pPawn->GetEyePosition();
		return {KHook::Action::Supersede, origin};
	}

	return {KHook::Action::Ignore};
}

KHook::Return<QAngle> CHookManager::Hook_GetEyeAngles(CBasePlayerPawn* pPawn)
{
	if (pPawn->IsAlive() && CPointViewControlHandler::IsViewControl(reinterpret_cast<CCSPlayerPawn*>(pPawn)))
	{
		const auto& angles = pPawn->v_angle();
		return {KHook::Action::Supersede, angles};
	}

	return {KHook::Action::Ignore};
}
#endif

KHook::Return<void> CHookManager::Hook_InputTestActivator(CBaseFilter* pThis, InputData_t& inputdata)
{
	if (!inputdata.pActivator) return {KHook::Action::Supersede};

	return {KHook::Action::Ignore};
}

KHook::Return<void> CHookManager::Hook_CheckSteamBan_Post()
{
	if (!g_cvarFixGameBans.Get()) return {KHook::Action::Ignore};

	auto pMap = addresses::sm_mapGcBanInformation;

	if (pMap->Count() > 0) pMap->RemoveAll();

	return {KHook::Action::Ignore};
}

KHook::Return<AcquireResult> CHookManager::Hook_CanAcquire(CCSPlayer_ItemServices* pItemServices, CEconItemView* pEconItem, AcquireMethod iAcquireMethod, uint64_t unk4)
{
	if (g_cvarEnableZR.Get())
	{
		AcquireResult zrResult = ZR_Detour_CCSPlayer_ItemServices_CanAcquire(pItemServices, pEconItem);

		if (zrResult != AcquireResult::Allowed) return {KHook::Action::Supersede, zrResult};
	}

	return {KHook::Action::Ignore};
}

KHook::Return<void> CHookManager::Hook_ScriptSetModel(uint64_t unk1)
{
	m_bInScriptSetModel = true;
	return {KHook::Action::Ignore};
}

KHook::Return<void> CHookManager::Hook_ScriptSetModel_Post(uint64_t unk1)
{
	m_bInScriptSetModel = false;
	return {KHook::Action::Ignore};
}

KHook::Return<void> CHookManager::Hook_SetModel(CBaseModelEntity* pModel, const char* pszModel)
{
	if (!m_bInScriptSetModel) return {KHook::Action::Ignore};

	if (PrepareMapSetModel(pModel)) return {KHook::Action::Ignore};

	return {KHook::Action::Supersede};
}

KHook::Return<void> CHookManager::Hook_GoToIntermission(CCSGameRules* pThis, bool bAbortedMatch)
{
	if (!g_pMapVoteSystem->IsIntermissionAllowed(false) && g_cvarVoteManagerEnable.Get()) return {KHook::Action::Supersede};

	if (g_cvarVoteManagerEnable.Get()) g_pVoteManager->OnIntermission();

	return {KHook::Action::Ignore};
}
