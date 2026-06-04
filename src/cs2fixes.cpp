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

#include "cs2fixes.h"
#include "iserver.h"

#include "adminsystem.h"
#include "appframework/IAppSystem.h"
#include "commands.h"
#include "common.h"
#include "cs_gameevents.pb.h"
#include "ctimer.h"
#include "discord.h"
#include "entities.h"
#include "entity/ccsplayercontroller.h"
#include "entity/services.h"
#include "entitylistener.h"
#include "entitysystem.h"
#include "entwatch.h"
#include "eventlistener.h"
#include "gameconfig.h"
#include "gameevents.pb.h"
#include "gamesystem.h"
#include "hookmanager.h"
#include "httpmanager.h"
#include "hud_manager.h"
#include "icvar.h"
#include "idlemanager.h"
#include "interface.h"
#include "leader.h"
#include "map_votes.h"
#include "mapmigrations.h"
#include "networkstringtabledefs.h"
#include "panoramavote.h"
#include "patches.h"
#include "plat.h"
#include "playermanager.h"
#include "schemasystem/schemasystem.h"
#include "serversideclient.h"
#include "te.pb.h"
#include "tier0/dbg.h"
#include "tier0/vprof.h"
#include "user_preferences.h"
#include "usermessages.pb.h"
#include "votemanager.h"
#include "zombiereborn.h"
#include <entity.h>

#include "tier0/memdbgon.h"

CS2Fixes g_CS2Fixes;
IGameEventSystem* g_gameEventSystem = nullptr;
IGameEventManager2* g_gameEventManager = nullptr;
CGameEntitySystem* g_pEntitySystem = nullptr;
IVEngineServer2* g_pEngineServer2 = nullptr;
CCSGameRules* g_pGameRules = nullptr;				  // Will be null between map end & new map startup, null check if necessary!
CSpawnGroupMgrGameSystem* g_pSpawnGroupMgr = nullptr; // Will be null between map end & new map startup, null check if necessary!

bool g_bRequiredInitLoaded = true;

CGameEntitySystem* GameEntitySystem()
{
	static int offset = g_GameConfig->GetOffset("GameEntitySystem");
	return *reinterpret_cast<CGameEntitySystem**>((uintptr_t)(g_pGameResourceServiceServer) + offset);
}

// Will return null between map end & new map startup, null check if necessary!
INetworkGameServer* GetNetworkGameServer()
{
	return g_pNetworkServerService->GetIGameServer();
}

// Will return null between map end & new map startup, null check if necessary!
CGlobalVars* GetGlobals()
{
	return g_pEngineServer2->GetServerGlobals();
}

PLUGIN_EXPOSE(CS2Fixes, g_CS2Fixes);
bool CS2Fixes::Load(PluginId id, ISmmAPI* ismm, char* error, size_t maxlen, bool late)
{
	PLUGIN_SAVEVARS();

	GET_V_IFACE_CURRENT(GetEngineFactory, g_pEngineServer2, IVEngineServer2, SOURCE2ENGINETOSERVER_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, g_pGameResourceServiceServer, IGameResourceService, GAMERESOURCESERVICESERVER_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, g_pCVar, ICvar, CVAR_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, g_pSchemaSystem, ISchemaSystem, SCHEMASYSTEM_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetServerFactory, g_pSource2Server, ISource2Server, SOURCE2SERVER_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetServerFactory, g_pSource2ServerConfig, ISource2ServerConfig, SOURCE2SERVERCONFIG_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetServerFactory, g_pSource2GameEntities, ISource2GameEntities, SOURCE2GAMEENTITIES_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetServerFactory, g_pSource2GameClients, IServerGameClients, SOURCE2GAMECLIENTS_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetEngineFactory, g_pNetworkServerService, INetworkServerService, NETWORKSERVERSERVICE_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetEngineFactory, g_gameEventSystem, IGameEventSystem, GAMEEVENTSYSTEM_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetEngineFactory, g_pNetworkMessages, INetworkMessages, NETWORKMESSAGES_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetEngineFactory, g_pGameTypes, IGameTypes, GAMETYPES_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetFileSystemFactory, g_pFullFileSystem, IFileSystem, FILESYSTEM_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetEngineFactory, g_pNetworkStringTableServer, INetworkStringTableContainer, INTERFACENAME_NETWORKSTRINGTABLESERVER);

	// Required to get the IMetamodListener events
	g_SMAPI->AddListener(this, this);

	Message("Starting plugin.\n");

	g_GameConfig = new CGameConfig();
	char conf_error[255] = "";

	if (!g_GameConfig->Init(conf_error, sizeof(conf_error)))
	{
		snprintf(error, maxlen, "%s", conf_error);
		Panic("%s\n", error);
		return false;
	}

	if (!addresses::Initialize(g_GameConfig))
		g_bRequiredInitLoaded = false;

	if (!InitPatches(g_GameConfig))
		g_bRequiredInitLoaded = false;

	g_pHookManager = new CHookManager(g_GameConfig);

	if (!InitGameSystems())
		g_bRequiredInitLoaded = false;

	if (!g_bRequiredInitLoaded)
	{
		snprintf(error, maxlen, "One or more address lookups, patches or detours failed, please refer to startup logs for more information");
		return false;
	}

	Message("All hooks started!\n");

	UnlockConVars();
	UnlockConCommands();
	META_CONVAR_REGISTER(FCVAR_RELEASE | FCVAR_GAMEDLL);

	g_pAdminSystem = new CAdminSystem();
	g_playerManager = new CPlayerManager();
	g_pDiscordBotManager = new CDiscordBotManager();
	g_pMapVoteSystem = new CMapVoteSystem();
	g_pVoteManager = new CVoteManager();
	g_pUserPreferencesSystem = new CUserPreferencesSystem();
	g_pUserPreferencesStorage = new CUserPreferencesREST();
	g_pZRPlayerClassManager = new CZRPlayerClassManager();
	g_pZRWeaponConfig = new ZRWeaponConfig();
	g_pZRHitgroupConfig = new ZRHitgroupConfig();
	g_pEntityListener = new CEntityListener();
	g_pIdleSystem = new CIdleSystem();
	g_pPanoramaVoteHandler = new CPanoramaVoteHandler();
	g_pEWHandler = new CEWHandler();
	g_pMapMigrations = new CMapMigrations();

	RegisterWeaponCommands();

	// Check hide distance
	CTimer::Create(0.5f, TIMERFLAG_NONE, []() {
		g_playerManager->CheckHideDistances();
		return 0.5f;
	});

	// Check for the expiration of infractions like mutes or gags
	CTimer::Create(30.0f, TIMERFLAG_NONE, []() {
		g_playerManager->CheckInfractions();
		return 30.0f;
	});

	// Check for idle players and kick them if permitted by cs2f_idle_kick_* 'convars'
	CTimer::Create(5.0f, TIMERFLAG_NONE, []() {
		g_pIdleSystem->CheckForIdleClients();
		return 5.0f;
	});

	// run our cfg
	g_pEngineServer2->ServerCommand("exec cs2fixes/cs2fixes");

	srand(time(0));

	if (late)
	{
		RegisterEventListeners();
		g_pEntitySystem = GameEntitySystem();
		g_pEntitySystem->AddListenerEntity(g_pEntityListener);

		g_playerManager->OnLateLoad();

		g_pPanoramaVoteHandler->Reset();
		g_pVoteManager->VoteManager_Init();

		g_pIdleSystem->Reset();
		g_playerManager->OnSteamAPIActivated();

		if (g_cvarVoteManagerEnable.Get() && !g_pMapVoteSystem->IsMapListLoaded())
			g_pMapVoteSystem->LoadMapList();

		Message("Plugin late load finished\n");
	}

	Message("Plugin successfully started!\n");

	return true;
}

bool CS2Fixes::Unload(char* error, size_t maxlen)
{

	ConVar_Unregister();

	UnregisterGameSystem();
	CommandList().clear();
	UndoPatches();
	RemoveAllTimers();
	UnregisterEventListeners();

	if (g_GameConfig)
		delete g_GameConfig;

	if (g_pHookManager)
		delete g_pHookManager;

	if (g_pAdminSystem)
		delete g_pAdminSystem;

	if (g_playerManager)
		delete g_playerManager;

	if (g_pDiscordBotManager)
		delete g_pDiscordBotManager;

	if (g_pMapVoteSystem)
		delete g_pMapVoteSystem;

	if (g_pVoteManager)
		delete g_pVoteManager;

	if (g_pUserPreferencesSystem)
		delete g_pUserPreferencesSystem;

	if (g_pUserPreferencesStorage)
		delete g_pUserPreferencesStorage;

	if (g_pZRPlayerClassManager)
		delete g_pZRPlayerClassManager;

	if (g_pZRWeaponConfig)
		delete g_pZRWeaponConfig;

	if (g_pZRHitgroupConfig)
		delete g_pZRHitgroupConfig;

	if (g_pEntitySystem && g_pEntityListener)
	{
		g_pEntitySystem->RemoveListenerEntity(g_pEntityListener);
		delete g_pEntityListener;
	}

	if (g_pIdleSystem)
		delete g_pIdleSystem;

	if (g_pPanoramaVoteHandler)
		delete g_pPanoramaVoteHandler;

	if (g_pEWHandler)
		delete g_pEWHandler;

	if (g_pMapMigrations)
		delete g_pMapMigrations;

	return true;
}

void CS2Fixes::AllPluginsLoaded()
{
	/* This is where we'd do stuff that relies on the mod or other plugins
	 * being initialized (for example, cvars added and events registered).
	 */

	Message("AllPluginsLoaded\n");
}

void* CS2Fixes::OnMetamodQuery(const char* iface, int* ret)
{
	if (V_strcmp(iface, CS2FIXES_INTERFACE))
	{
		if (ret)
			*ret = META_IFACE_FAILED;

		return nullptr;
	}

	if (ret)
		*ret = META_IFACE_OK;

	return static_cast<ICS2Fixes*>(&g_CS2Fixes);
}

std::uint64_t CS2Fixes::GetAdminFlags(std::uint64_t iSteam64ID) const
{
	if (!g_pAdminSystem)
		return 0;

	const CAdmin* admin = g_pAdminSystem->FindAdmin(static_cast<uint64>(iSteam64ID));
	if (!admin)
		return 0;

	return admin->GetFlags();
}

bool CS2Fixes::SetAdminFlags(std::uint64_t iSteam64ID, std::uint64_t iFlags)
{
	if (!g_pAdminSystem)
		return false;

	CAdmin* admin = g_pAdminSystem->FindAdmin(static_cast<uint64>(iSteam64ID));
	g_pAdminSystem->AddOrUpdateAdmin(static_cast<uint64>(iSteam64ID), iFlags, admin ? admin->GetImmunity() : 0);
	return true;
}

int CS2Fixes::GetAdminImmunity(std::uint64_t iSteam64ID) const
{
	if (!g_pAdminSystem)
		return 0;

	const CAdmin* admin = g_pAdminSystem->FindAdmin(static_cast<uint64>(iSteam64ID));
	if (!admin)
		return 0;

	return admin->GetImmunity();
}

bool CS2Fixes::SetAdminImmunity(std::uint64_t iSteam64ID, std::uint32_t iImmunity)
{
	if (!g_pAdminSystem)
		return false;

	CAdmin* admin = g_pAdminSystem->FindAdmin(static_cast<uint64>(iSteam64ID));
	g_pAdminSystem->AddOrUpdateAdmin(static_cast<uint64>(iSteam64ID), admin ? admin->GetFlags() : 0, iImmunity);
	return true;
}

void CS2Fixes::OnLevelInit(char const* pMapName,
						   char const* pMapEntities,
						   char const* pOldLevel,
						   char const* pLandmarkName,
						   bool loadGame,
						   bool background)
{
	Message("OnLevelInit(%s)\n", pMapName);

	// run our cfg
	g_pEngineServer2->ServerCommand("exec cs2fixes/cs2fixes");

	// Run map cfg (if present)
	char cmd[MAX_PATH];
	V_snprintf(cmd, sizeof(cmd), "exec cs2fixes/maps/%s", pMapName);
	g_pEngineServer2->ServerCommand(cmd);

	// Only patch BotNavIgnore while a map is loaded, else adding bots will crash
	if (V_strcmp(pMapName, "error"))
		g_CommonPatches[1].PerformPatch(g_GameConfig);

	g_playerManager->SetupInfiniteAmmo();
	g_pMapVoteSystem->OnLevelInit(pMapName);

	if (g_cvarEnableZR.Get())
		ZR_OnLevelInit();

	CCSPlayer_ItemServices::ResetAwsProcessing();

	EntityHandler_OnLevelInit();

	if (g_cvarEnableEntWatch.Get())
		EW_OnLevelInit(pMapName);

	StartFlashingFixTimer();
}

void CS2Fixes::OnLevelShutdown()
{
	Message("OnLevelShutdown()\n");

	// Only patch BotNavIgnore while a map is loaded, else adding bots will crash
	g_CommonPatches[1].UndoPatch();

	if (g_cvarVoteManagerEnable.Get())
		g_pMapVoteSystem->OnLevelShutdown();
}

bool CS2Fixes::Pause(char* error, size_t maxlen)
{
	return true;
}

bool CS2Fixes::Unpause(char* error, size_t maxlen)
{
	return true;
}
