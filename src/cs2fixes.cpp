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

#include "cs2fixes.h"
#include "iserver.h"

#include "adminsystem.h"
#include "appframework/IAppSystem.h"
#include "commands.h"
#include "common.h"
#include "cs_gameevents.pb.h"
#include "ctimer.h"
#include "detours.h"
#include "discord.h"
#include "engine/igameeventsystem.h"
#include "entities.h"
#include "entity/ccsplayercontroller.h"
#include "entity/cgamerules.h"
#include "entity/services.h"
#include "entitylistener.h"
#include "entitysystem.h"
#include "entwatch.h"
#include "eventlistener.h"
#include "gameconfig.h"
#include "gameevents.pb.h"
#include "gamesystem.h"
#include "httpmanager.h"
#include "icvar.h"
#include "idlemanager.h"
#include "interface.h"
#include "leader.h"
#include "map_votes.h"
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

double g_flUniversalTime;
float g_flLastTickedTime;
bool g_bHasTicked;
int g_iRoundNum = 0;

void Message(const char* msg, ...)
{
	va_list args;
	va_start(args, msg);

	char buf[1024] = {};
	V_vsnprintf(buf, sizeof(buf) - 1, msg, args);

	ConColorMsg(Color(255, 0, 255, 255), "[CS2Fixes] %s", buf);

	va_end(args);
}

void Panic(const char* msg, ...)
{
	va_list args;
	va_start(args, msg);

	char buf[1024] = {};
	V_vsnprintf(buf, sizeof(buf) - 1, msg, args);

	Warning("[CS2Fixes] %s", buf);

	va_end(args);
}

class GameSessionConfiguration_t
{};

KHook::Virtual gameFrameHook(&IServerGameDLL::GameFrame, &g_CS2Fixes, nullptr, &CS2Fixes::Hook_GameFrame_Post);
KHook::Virtual gameServerSteamAPIActivatedHook(&IServerGameDLL::GameServerSteamAPIActivated, &g_CS2Fixes, &CS2Fixes::Hook_GameServerSteamAPIActivated, nullptr);
KHook::Virtual gameServerSteamAPIDeactivatedHook(&IServerGameDLL::GameServerSteamAPIDeactivated, &g_CS2Fixes, &CS2Fixes::Hook_GameServerSteamAPIDeactivated, nullptr);
KHook::Virtual applyGameSettingsHook(&IServerGameDLL::ApplyGameSettings, &g_CS2Fixes, &CS2Fixes::Hook_ApplyGameSettings, nullptr);
KHook::Virtual clientActiveHook(&IServerGameClients::ClientActive, &g_CS2Fixes, nullptr, &CS2Fixes::Hook_ClientActive_Post);
KHook::Virtual clientDisconnectHook(&IServerGameClients::ClientDisconnect, &g_CS2Fixes, nullptr, &CS2Fixes::Hook_ClientDisconnect_Post);
KHook::Virtual clientPutInServerHook(&IServerGameClients::ClientPutInServer, &g_CS2Fixes, nullptr, &CS2Fixes::Hook_ClientPutInServer_Post);
KHook::Virtual clientSettingsChangedHook(&IServerGameClients::ClientSettingsChanged, &g_CS2Fixes, &CS2Fixes::Hook_ClientSettingsChanged, nullptr);
KHook::Virtual onClientConnectedHook(&IServerGameClients::OnClientConnected, &g_CS2Fixes, &CS2Fixes::Hook_OnClientConnected, nullptr);
KHook::Virtual clientConnectHook(&IServerGameClients::ClientConnect, &g_CS2Fixes, &CS2Fixes::Hook_ClientConnect, nullptr);
KHook::Virtual clientCommandHook(&IServerGameClients::ClientCommand, &g_CS2Fixes, &CS2Fixes::Hook_ClientCommand, nullptr);
KHook::Virtual postEventAbstractHook(&IGameEventSystem::PostEventAbstract, &g_CS2Fixes, &CS2Fixes::Hook_PostEventAbstract, nullptr);
KHook::Virtual startupServerHook(&INetworkServerService::StartupServer, &g_CS2Fixes, nullptr, &CS2Fixes::Hook_StartupServer_Post);
KHook::Virtual checkTransmitHook(&ISource2GameEntities::CheckTransmit, &g_CS2Fixes, nullptr, &CS2Fixes::Hook_CheckTransmit_Post);
KHook::Virtual dispatchConCommandHook(&ICvar::DispatchConCommand, &g_CS2Fixes, &CS2Fixes::Hook_DispatchConCommand, nullptr);
KHook::Virtual loadEventsFromFileHook(&IGameEventManager2::LoadEventsFromFile, &g_CS2Fixes, &CS2Fixes::Hook_LoadEventsFromFile, nullptr);
KHook::Virtual playerEquipUseHook(&g_CS2Fixes, &CS2Fixes::Hook_PlayerEquipUse, nullptr);
KHook::Virtual playerEquipPrecacheHook(&g_CS2Fixes, nullptr, &CS2Fixes::Hook_PlayerEquipPrecache_Post);
KHook::Virtual createWorkshopMapGroupHook(&g_CS2Fixes, &CS2Fixes::Hook_CreateWorkshopMapGroup, nullptr);
KHook::Virtual onTakeDamageAliveHook(&g_CS2Fixes, &CS2Fixes::Hook_OnTakeDamage_Alive, nullptr);
KHook::Virtual checkMovingGroundHook(&g_CS2Fixes, &CS2Fixes::Hook_CheckMovingGround, nullptr);
KHook::Virtual goToIntermissionHook(&g_CS2Fixes, &CS2Fixes::Hook_GoToIntermission, nullptr);
KHook::Virtual physicsTouchShuffleHook(&g_CS2Fixes, nullptr, &CS2Fixes::Hook_PhysicsTouchShuffle_Post);
KHook::Virtual dropWeaponHook(&g_CS2Fixes, nullptr, &CS2Fixes::Hook_DropWeapon_Post);

CS2Fixes g_CS2Fixes;

IGameEventSystem* g_gameEventSystem = nullptr;
IGameEventManager2* g_gameEventManager = nullptr;
CGameEntitySystem* g_pEntitySystem = nullptr;
CEntityListener* g_pEntityListener = nullptr;
CPlayerManager* g_playerManager = nullptr;
IVEngineServer2* g_pEngineServer2 = nullptr;
CGameConfig* g_GameConfig = nullptr;
ISteamHTTP* g_http = nullptr;
CSteamGameServerAPIContext g_steamAPI;
CCSGameRules* g_pGameRules = nullptr; // Will be null between map end & new map startup, null check if necessary!
IGameEventManager2* g_pCGameEventManagerVTable = nullptr;
CGamePlayerEquip* g_pCGamePlayerEquipVTable = nullptr;
CCSPlayerPawn* g_pCCSPlayerPawnVTable = nullptr;
CCSPlayer_MovementServices* g_pCCSPlayer_MovementServicesVTable = nullptr;
CCSGameRules* g_pCCSGameRulesVTable = nullptr;
CVPhys2World* g_pCVPhys2WorldVTable = nullptr;
CCSPlayer_WeaponServices* g_pCCSPlayer_WeaponServicesVTable = nullptr;

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

	CBufferStringGrowable<256> gamedirpath;
	g_pEngineServer2->GetGameDir(gamedirpath);

	std::string gamedirname = CGameConfig::GetDirectoryName(gamedirpath.Get());

	const char* gamedataPath = "addons/cs2fixes/gamedata/cs2fixes.games.txt";
	Message("Loading %s for game: %s\n", gamedataPath, gamedirname.c_str());

	g_GameConfig = new CGameConfig(gamedirname, gamedataPath);
	char conf_error[255] = "";
	if (!g_GameConfig->Init(g_pFullFileSystem, conf_error, sizeof(conf_error)))
	{
		snprintf(error, maxlen, "Could not read %s: %s", g_GameConfig->GetPath().c_str(), conf_error);
		Panic("%s\n", error);
		return false;
	}

	gameFrameHook.Add(g_pSource2Server);
	gameServerSteamAPIActivatedHook.Add(g_pSource2Server);
	gameServerSteamAPIDeactivatedHook.Add(g_pSource2Server);
	applyGameSettingsHook.Add(g_pSource2Server);
	clientActiveHook.Add(g_pSource2GameClients);
	clientDisconnectHook.Add(g_pSource2GameClients);
	clientPutInServerHook.Add(g_pSource2GameClients);
	clientSettingsChangedHook.Add(g_pSource2GameClients);
	onClientConnectedHook.Add(g_pSource2GameClients);
	clientConnectHook.Add(g_pSource2GameClients);
	clientCommandHook.Add(g_pSource2GameClients);
	postEventAbstractHook.Add(g_gameEventSystem);
	startupServerHook.Add(g_pNetworkServerService);
	checkTransmitHook.Add(g_pSource2GameEntities);
	dispatchConCommandHook.Add(g_pCVar);

	bool bRequiredInitLoaded = true;

	if (!addresses::Initialize(g_GameConfig))
		bRequiredInitLoaded = false;

	if (!InitPatches(g_GameConfig))
		bRequiredInitLoaded = false;

	if (!InitDetours(g_GameConfig))
		bRequiredInitLoaded = false;

	if (!InitGameSystems())
		bRequiredInitLoaded = false;

	g_pCGameEventManagerVTable = (IGameEventManager2*)modules::server->FindVirtualTable("CGameEventManager");

	if (!g_pCGameEventManagerVTable)
	{
		Panic("Failed to find CGameEventManager vtable\n");
		bRequiredInitLoaded = false;
	}

	loadEventsFromFileHook.Add(g_pCGameEventManagerVTable);

	g_pCGamePlayerEquipVTable = (CGamePlayerEquip*)modules::server->FindVirtualTable("CGamePlayerEquip");
	if (!g_pCGamePlayerEquipVTable)
	{
		Panic("Failed to find CGamePlayerEquip vtable\n");
		bRequiredInitLoaded = false;
	}

	int offset = g_GameConfig->GetOffset("CBaseEntity::Use");

	if (offset == -1)
	{
		Panic("Failed to find CBaseEntity::Use\n");
		bRequiredInitLoaded = false;
	}

	playerEquipUseHook.SetIndex(offset);
	playerEquipUseHook.Add(g_pCGamePlayerEquipVTable);

	offset = g_GameConfig->GetOffset("CBaseEntity::Precache");
	if (offset == -1)
	{
		Panic("Failed to find CBaseEntity::Precache\n");
		bRequiredInitLoaded = false;
	}

	playerEquipPrecacheHook.SetIndex(offset);
	playerEquipPrecacheHook.Add(g_pCGamePlayerEquipVTable);

	offset = g_GameConfig->GetOffset("IGameTypes_CreateWorkshopMapGroup");

	if (offset == -1)
	{
		Panic("Failed to find IGameTypes_CreateWorkshopMapGroup\n");
		bRequiredInitLoaded = false;
	}

	createWorkshopMapGroupHook.SetIndex(offset);
	createWorkshopMapGroupHook.Add(g_pGameTypes);

	g_pCCSPlayerPawnVTable = (CCSPlayerPawn*)modules::server->FindVirtualTable("CCSPlayerPawn");

	if (!g_pCCSPlayerPawnVTable)
	{
		Panic("Failed to find CCSPlayerPawn vtable\n");
		bRequiredInitLoaded = false;
	}

	offset = g_GameConfig->GetOffset("CBasePlayerPawn::OnTakeDamage_Alive");
	if (offset == -1)
	{
		Panic("Failed to find CBasePlayerPawn::OnTakeDamage_Alive\n");
		bRequiredInitLoaded = false;
	}

	onTakeDamageAliveHook.SetIndex(offset);
	onTakeDamageAliveHook.Add(g_pCCSPlayerPawnVTable);

	g_pCCSPlayer_MovementServicesVTable = (CCSPlayer_MovementServices*)modules::server->FindVirtualTable("CCSPlayer_MovementServices");

	if (!g_pCCSPlayer_MovementServicesVTable)
	{
		Panic("Failed to find CCSPlayer_MovementServices vtable\n");
		bRequiredInitLoaded = false;
	}

	offset = g_GameConfig->GetOffset("CCSPlayer_MovementServices::CheckMovingGround");
	if (offset == -1)
	{
		Panic("Failed to find CCSPlayer_MovementServices::CheckMovingGround\n");
		bRequiredInitLoaded = false;
	}

	checkMovingGroundHook.SetIndex(offset);
	checkMovingGroundHook.Add(g_pCCSPlayer_MovementServicesVTable);

	g_pCCSGameRulesVTable = (CCSGameRules*)modules::server->FindVirtualTable("CCSGameRules");

	if (!g_pCCSGameRulesVTable)
	{
		Panic("Failed to find CCSGameRules vtable\n");
		bRequiredInitLoaded = false;
	}

	offset = g_GameConfig->GetOffset("CCSGameRules_GoToIntermission");
	if (offset == -1)
	{
		Panic("Failed to find CCSGameRules_GoToIntermission\n");
		bRequiredInitLoaded = false;
	}

	goToIntermissionHook.SetIndex(offset);
	goToIntermissionHook.Add(g_pCCSGameRulesVTable);

	g_pCVPhys2WorldVTable = (CVPhys2World*)modules::vphysics2->FindVirtualTable("CVPhys2World");

	if (!g_pCVPhys2WorldVTable)
	{
		Panic("Failed to find CVPhys2World vtable\n");
		bRequiredInitLoaded = false;
	}

	offset = g_GameConfig->GetOffset("CVPhys2World::GetTouchingList");
	if (offset == -1)
	{
		Panic("Failed to find CVPhys2World::GetTouchingList\n");
		bRequiredInitLoaded = false;
	}

	physicsTouchShuffleHook.SetIndex(offset);
	physicsTouchShuffleHook.Add(g_pCVPhys2WorldVTable);

	g_pCCSPlayer_WeaponServicesVTable = (CCSPlayer_WeaponServices*)modules::server->FindVirtualTable("CCSPlayer_WeaponServices");

	if (!g_pCCSPlayer_WeaponServicesVTable)
	{
		Panic("Failed to find CCSPlayer_WeaponServices vtable\n");
		bRequiredInitLoaded = false;
	}

	offset = g_GameConfig->GetOffset("CCSPlayer_WeaponServices::DropWeapon");
	if (offset == -1)
	{
		Panic("Failed to find CCSPlayer_WeaponServices::DropWeapon\n");
		bRequiredInitLoaded = false;
	}

	dropWeaponHook.SetIndex(offset);
	dropWeaponHook.Add(g_pCCSPlayer_WeaponServicesVTable);

	if (!bRequiredInitLoaded)
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

	RegisterWeaponCommands();

	// Check hide distance
	new CTimer(0.5f, true, true, []() {
		g_playerManager->CheckHideDistances();
		return 0.5f;
	});

	// Check for the expiration of infractions like mutes or gags
	new CTimer(30.0f, true, true, []() {
		g_playerManager->CheckInfractions();
		return 30.0f;
	});

	// Check for idle players and kick them if permitted by cs2f_idle_kick_* 'convars'
	new CTimer(5.0f, true, true, []() {
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

		g_steamAPI.Init();
		g_http = g_steamAPI.SteamHTTP();

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
	gameFrameHook.Remove(g_pSource2Server);
	gameServerSteamAPIActivatedHook.Remove(g_pSource2Server);
	gameServerSteamAPIDeactivatedHook.Remove(g_pSource2Server);
	applyGameSettingsHook.Remove(g_pSource2Server);
	clientActiveHook.Remove(g_pSource2GameClients);
	clientDisconnectHook.Remove(g_pSource2GameClients);
	clientPutInServerHook.Remove(g_pSource2GameClients);
	clientSettingsChangedHook.Remove(g_pSource2GameClients);
	onClientConnectedHook.Remove(g_pSource2GameClients);
	clientConnectHook.Remove(g_pSource2GameClients);
	clientCommandHook.Remove(g_pSource2GameClients);
	postEventAbstractHook.Remove(g_gameEventSystem);
	startupServerHook.Remove(g_pNetworkServerService);
	checkTransmitHook.Remove(g_pSource2GameEntities);
	dispatchConCommandHook.Remove(g_pCVar);
	loadEventsFromFileHook.Remove(g_pCGameEventManagerVTable);
	playerEquipUseHook.Remove(g_pCGamePlayerEquipVTable);
	playerEquipPrecacheHook.Remove(g_pCGamePlayerEquipVTable);
	createWorkshopMapGroupHook.Remove(g_pGameTypes);
	onTakeDamageAliveHook.Remove(g_pCCSPlayerPawnVTable);
	checkMovingGroundHook.Remove(g_pCCSPlayer_MovementServicesVTable);
	goToIntermissionHook.Remove(g_pCCSGameRulesVTable);
	physicsTouchShuffleHook.Remove(g_pCVPhys2WorldVTable);
	dropWeaponHook.Remove(g_pCCSPlayer_WeaponServicesVTable);

	ConVar_Unregister();

	UnregisterGameSystem();

	g_CommandList.Purge();

	FlushAllDetours();
	UndoPatches();
	RemoveTimers();
	UnregisterEventListeners();

	if (g_GameConfig)
		delete g_GameConfig;

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

	if (g_pEntityListener)
	{
		g_pEntitySystem->RemoveListenerEntity(g_pEntityListener);
		delete g_pEntityListener;
	}

	if (g_pIdleSystem)
		delete g_pIdleSystem;

	if (g_pPanoramaVoteHandler)
		delete g_pPanoramaVoteHandler;

	if (g_pEWHandler)
	{
		g_pEWHandler->RemoveAllUseHooks();
		g_pEWHandler->RemoveAllTriggers();
		delete g_pEWHandler;
	}

	return true;
}

KHook::Return<void> CS2Fixes::Hook_DispatchConCommand(ICvar* pThis, ConCommandRef cmdHandle, const CCommandContext& ctx, const CCommand& args)
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
		bool bAdminChat = bTeamSay && *args[1] == '@';
		bool bSilent = *args[1] == '/' || bAdminChat;
		bool bCommand = *args[1] == '!' || *args[1] == '/';

		// Chat messages should generate events regardless
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
			auto originalFunc = *(std::function<void(ConCommandRef, const CCommandContext&, const CCommand&)>*)KHook::GetOriginalFunction();
			originalFunc(cmdHandle, ctx, args);
		}
		else if (bFlooding)
		{
			if (pController)
				ClientPrint(pController, HUD_PRINTTALK, CHAT_PREFIX "You are flooding the server!");
		}
		else if (bAdminChat && GetGlobals()) // Admin chat can be sent by anyone but only seen by admins, use flood protection here too
		{
			// HACK: At this point, we can safely modify the arg buffer as it won't be passed anywhere else
			// The string here is originally ("@foo bar"), trim it to be (foo bar)
			char* pszMessage = (char*)(args.ArgS() + 2);
			pszMessage[V_strlen(pszMessage) - 1] = 0;

			for (int i = 0; i < GetGlobals()->maxClients; i++)
			{
				ZEPlayer* pPlayer = g_playerManager->GetPlayer(i);

				if (!pPlayer)
					continue;

				if (pPlayer->IsAdminFlagSet(ADMFLAG_GENERIC))
					ClientPrint(CCSPlayerController::FromSlot(i), HUD_PRINTTALK, " \4(ADMINS) %s:\1 %s", pController->GetPlayerName(), pszMessage);
				else if (i == iCommandPlayerSlot.Get()) // Sender is not an admin
					ClientPrint(pController, HUD_PRINTTALK, " \4(TO ADMINS) %s:\1 %s", pController->GetPlayerName(), pszMessage);
			}
		}

		// Finally, run the chat command if it is one, so anything will print after the player's message
		if (bCommand)
		{
			char* pszMessage = (char*)(args.ArgS() + 1);

			if (pszMessage[0] == '"' || pszMessage[0] == '!' || pszMessage[0] == '/')
				pszMessage += 1;

			// Host_Say at some point removes the trailing " for whatever reason, so we only remove if it was never called
			if ((bGagged || bSilent || bFlooding) && pszMessage[V_strlen(pszMessage) - 1] == '"')
				pszMessage[V_strlen(pszMessage) - 1] = '\0';

			ParseChatCommand(pszMessage, pController);
		}

		return {KHook::Action::Supercede};
	}

	return {KHook::Action::Ignore};
}

KHook::Return<void> CS2Fixes::Hook_StartupServer_Post(INetworkServerService* pThis, const GameSessionConfiguration_t& config, ISource2WorldSession* pSession, const char* pszMapName)
{
	g_pEntitySystem = GameEntitySystem();
	g_pEntitySystem->AddListenerEntity(g_pEntityListener);

	Message("Hook_StartupServer: %s\n", pszMapName);

	RegisterEventListeners();

	if (g_bHasTicked)
		RemoveMapTimers();

	g_bHasTicked = false;

	g_pPanoramaVoteHandler->Reset();
	g_pVoteManager->VoteManager_Init();

	g_pIdleSystem->Reset();

	return {KHook::Action::Ignore};
}

KHook::Return<void> CS2Fixes::Hook_PlayerEquipUse(CGamePlayerEquip* pThis, InputData_t* pInput)
{
	CGamePlayerEquipHandler::Use(pThis, pInput);

	return {KHook::Action::Ignore};
}
KHook::Return<void> CS2Fixes::Hook_PlayerEquipPrecache_Post(CGamePlayerEquip* pThis, void** param)
{
	const auto kv = reinterpret_cast<CEntityKeyValues*>(*param);
	CGamePlayerEquipHandler::OnPrecache(pThis, kv);

	return {KHook::Action::Ignore};
}

KHook::Return<void> CS2Fixes::Hook_GameServerSteamAPIActivated(IServerGameDLL* pThis)
{
	g_steamAPI.Init();
	g_http = g_steamAPI.SteamHTTP();

	g_playerManager->OnSteamAPIActivated();

	if (g_cvarVoteManagerEnable.Get() && !g_pMapVoteSystem->IsMapListLoaded())
		g_pMapVoteSystem->LoadMapList();

	return {KHook::Action::Ignore};
}

KHook::Return<void> CS2Fixes::Hook_GameServerSteamAPIDeactivated(IServerGameDLL* pThis)
{
	g_http = nullptr;

	return {KHook::Action::Ignore};
}

KHook::Return<void> CS2Fixes::Hook_PostEventAbstract(IGameEventSystem* pThis, CSplitScreenSlot nSlot, bool bLocalOnly, int nClientCount, const uint64* clients,
							  INetworkMessageInternal* pEvent, const CNetMessage* pData, unsigned long nSize, NetChannelBufType_t bufType)
{
	// Message( "Hook_PostEvent(%d, %d, %d, %lli)\n", nSlot, bLocalOnly, nClientCount, clients );
	NetMessageInfo_t* info = pEvent->GetNetMessageInfo();

	if (g_cvarEnableStopSound.Get() && info->m_MessageId == GE_FireBulletsId)
	{
		if (g_playerManager->GetSilenceSoundMask())
		{
			// Post the silenced sound to those who use silencesound
			// Creating a new event object requires us to include the protobuf c files which I didn't feel like doing yet
			// So instead just edit the event in place and reset later
			auto msg = const_cast<CNetMessage*>(pData)->ToPB<CMsgTEFireBullets>();

			int32_t weapon_id = msg->weapon_id();
			int32_t sound_type = msg->sound_type();
			int32_t item_def_index = msg->item_def_index();

			// original weapon_id will override new settings if not removed
			msg->set_weapon_id(0);
			msg->set_sound_type(9);
			msg->set_item_def_index(61); // weapon_usp_silencer

			uint64 clientMask = *(uint64*)clients & g_playerManager->GetSilenceSoundMask();

			auto originalFunc = *(std::function<void(CSplitScreenSlot, bool, int, const uint64*, INetworkMessageInternal*, const CNetMessage*, unsigned long, NetChannelBufType_t)>*)KHook::GetOriginalFunction();
			originalFunc(nSlot, bLocalOnly, nClientCount, &clientMask, pEvent, msg, nSize, bufType);

			msg->set_weapon_id(weapon_id);
			msg->set_sound_type(sound_type);
			msg->set_item_def_index(item_def_index);
		}

		// Filter out people using stop/silence sound from the original event
		*(uint64*)clients &= ~g_playerManager->GetStopSoundMask();
		*(uint64*)clients &= ~g_playerManager->GetSilenceSoundMask();
	}
	else if (info->m_MessageId == TE_WorldDecalId)
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

		// remove client with noshake from the event
		if (g_cvarEnableNoShake.Get())
			*(uint64*)clients &= ~g_playerManager->GetNoShakeMask();
	}
	else if (g_cvarEnableStopSound.Get() && info->m_MessageId == GE_SosStartSoundEvent)
	{
		auto msg = const_cast<CNetMessage*>(pData)->ToPB<CMsgSosStartSoundEvent>();

		if (msg->soundevent_hash() == MurmurHash2LowerCase("Weapon_Revolver.Prepare", 0x53524332))
		{
			// Filter out people using stop/silence sound from hearing R8 windup
			*(uint64*)clients &= ~g_playerManager->GetStopSoundMask();
			*(uint64*)clients &= ~g_playerManager->GetSilenceSoundMask();
		}
	}

	return {KHook::Action::Ignore};
}

void CS2Fixes::AllPluginsLoaded()
{
	/* This is where we'd do stuff that relies on the mod or other plugins
	 * being initialized (for example, cvars added and events registered).
	 */

	Message("AllPluginsLoaded\n");
}

CUtlVector<CServerSideClient*>* GetClientList()
{
	if (!GetNetworkGameServer())
		return nullptr;

	static int offset = g_GameConfig->GetOffset("CNetworkGameServer_ClientList");
	return (CUtlVector<CServerSideClient*>*)(&GetNetworkGameServer()[offset]);
}

CServerSideClient* GetClientBySlot(CPlayerSlot slot)
{
	CUtlVector<CServerSideClient*>* pClients = GetClientList();

	if (!pClients)
		return nullptr;

	return pClients->Element(slot.Get());
}

void FullUpdateAllClients()
{
	auto pClients = GetClientList();

	if (!pClients)
		return;

	FOR_EACH_VEC(*pClients, i)
	(*pClients)[i]->ForceFullUpdate();
}

// Because sv_fullupdate doesn't work
CON_COMMAND_F(cs2f_fullupdate, "- Force a full update for all clients.", FCVAR_LINKED_CONCOMMAND | FCVAR_SPONLY)
{
	FullUpdateAllClients();
}

KHook::Return<void> CS2Fixes::Hook_ClientActive_Post(IServerGameClients* pThis, CPlayerSlot slot, bool bLoadGame, const char* pszName, uint64 xuid)
{
	Message("Hook_ClientActive(%d, %d, \"%s\", %lli)\n", slot, bLoadGame, pszName, xuid);

	return {KHook::Action::Ignore};
}

KHook::Return<void> CS2Fixes::Hook_ClientCommand(IServerGameClients* pThis, CPlayerSlot slot, const CCommand& args)
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
			return {KHook::Action::Supercede};
	}

	if (g_cvarEnableZR.Get() && slot != -1 && !V_strncmp(args.Arg(0), "jointeam", 8))
	{
		ZR_Hook_ClientCommand_JoinTeam(slot, args);
		return {KHook::Action::Supercede};
	}

	return {KHook::Action::Ignore};
}

KHook::Return<void> CS2Fixes::Hook_ClientSettingsChanged(IServerGameClients* pThis, CPlayerSlot slot)
{
#ifdef _DEBUG
	Message("Hook_ClientSettingsChanged(%d)\n", slot);
#endif

	return {KHook::Action::Ignore};
}

KHook::Return<void> CS2Fixes::Hook_OnClientConnected(IServerGameClients* pThis, CPlayerSlot slot, const char* pszName, uint64 xuid, const char* pszNetworkID, const char* pszAddress, bool bFakePlayer)
{
	Message("Hook_OnClientConnected(%d, \"%s\", %lli, \"%s\", \"%s\", %d)\n", slot, pszName, xuid, pszNetworkID, pszAddress, bFakePlayer);

	static ConVarRefAbstract tv_name("tv_name");
	const char* pszTvName = tv_name.GetString().Get();

	// Ideally we would use CServerSideClient::IsHLTV().. but it doesn't work :(
	if (bFakePlayer && V_strcmp(pszName, pszTvName))
		g_playerManager->OnBotConnected(slot);

	return {KHook::Action::Ignore};
}

KHook::Return<bool> CS2Fixes::Hook_ClientConnect(IServerGameClients* pThis, CPlayerSlot slot, const char* pszName, uint64 xuid, const char* pszNetworkID, bool unk1, CBufferString* pRejectReason)
{
	Message("Hook_ClientConnect(%d, \"%s\", %lli, \"%s\", %d, \"%s\")\n", slot, pszName, xuid, pszNetworkID, unk1, pRejectReason->Get());

	// Player is banned
	if (!g_playerManager->OnClientConnected(slot, xuid, pszNetworkID))
		return {KHook::Action::Supercede, false};

	// TODO: verify true return isn't needed
	return {KHook::Action::Ignore};
}

KHook::Return<void> CS2Fixes::Hook_ClientPutInServer_Post(IServerGameClients* pThis, CPlayerSlot slot, char const* pszName, int type, uint64 xuid)
{
	Message("Hook_ClientPutInServer(%d, \"%s\", %d, %d, %lli)\n", slot, pszName, type, xuid);

	if (!g_playerManager->GetPlayer(slot))
		return {KHook::Action::Ignore};

	g_playerManager->OnClientPutInServer(slot);

	if (g_cvarEnableZR.Get())
		ZR_Hook_ClientPutInServer(slot, pszName, type, xuid);

	return {KHook::Action::Ignore};
}

KHook::Return<void> CS2Fixes::Hook_ClientDisconnect_Post(IServerGameClients* pThis, CPlayerSlot slot, ENetworkDisconnectionReason reason, const char* pszName, uint64 xuid, const char* pszNetworkID)
{
	Message("Hook_ClientDisconnect(%d, %d, \"%s\", %lli)\n", slot, reason, pszName, xuid);
	ZEPlayer* pPlayer = g_playerManager->GetPlayer(slot);

	if (!pPlayer)
		return {KHook::Action::Ignore};

	// Dont add to c_listdc clients that are downloading MultiAddonManager stuff or were present during a map change
	if (reason != NETWORK_DISCONNECT_LOOPSHUTDOWN && reason != NETWORK_DISCONNECT_SHUTDOWN)
		g_pAdminSystem->AddDisconnectedPlayer(pszName, xuid, pPlayer ? pPlayer->GetIpAddress() : "");

	g_playerManager->OnClientDisconnect(slot);

	return {KHook::Action::Ignore};
}

KHook::Return<void> CS2Fixes::Hook_GameFrame_Post(IServerGameDLL* pThis, bool simulating, bool bFirstTick, bool bLastTick)
{
	/**
	 * simulating:
	 * ***********
	 * true  | game is ticking
	 * false | game is not ticking
	 */

	VPROF_BUDGET("CS2Fixes::Hook_GameFramePost", "CS2FixesPerFrame");

	if (!GetGlobals())
		return {KHook::Action::Ignore};

	if (simulating && g_bHasTicked)
		g_flUniversalTime += GetGlobals()->curtime - g_flLastTickedTime;

	g_flLastTickedTime = GetGlobals()->curtime;
	g_bHasTicked = true;

	for (int i = g_timers.Tail(); i != g_timers.InvalidIndex();)
	{
		auto timer = g_timers[i];

		int prevIndex = i;
		i = g_timers.Previous(i);

		if (timer->m_flLastExecute == -1)
			timer->m_flLastExecute = g_flUniversalTime;

		// Timer execute
		if (timer->m_flLastExecute + timer->m_flInterval <= g_flUniversalTime)
		{
			if ((!timer->m_bPreserveRoundChange && timer->m_iRoundNum != g_iRoundNum) || !timer->Execute())
			{
				delete timer;
				g_timers.Remove(prevIndex);
			}
			else
			{
				timer->m_flLastExecute = g_flUniversalTime;
			}
		}
	}

	if (g_cvarEnableZR.Get())
		CZRRegenTimer::Tick();

	EntityHandler_OnGameFramePost(simulating, GetGlobals()->tickcount);

	return {KHook::Action::Ignore};
}

extern CConVar<bool> g_cvarFlashLightTransmitOthers;

KHook::Return<void> CS2Fixes::Hook_CheckTransmit_Post(ISource2GameEntities* pThis, CCheckTransmitInfo** ppInfoList, int infoCount, CBitVec<16384>& unionTransmitEdicts,
								  const Entity2Networkable_t** pNetworkables, const uint16* pEntityIndicies, int nEntities, bool bEnablePVSBits)
{
	if (!g_pEntitySystem || !GetGlobals())
		return {KHook::Action::Ignore};

	VPROF("CS2Fixes::Hook_CheckTransmit");

	for (int i = 0; i < infoCount; i++)
	{
		auto& pInfo = ppInfoList[i];

		// the offset happens to have a player index here,
		// though this is probably part of the client class that contains the CCheckTransmitInfo
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
			// Always transmit to themselves
			if (!pController || pController->m_bIsHLTV || j == iPlayerSlot)
				continue;

			// Don't transmit other players' flashlights
			CBarnLight* pFlashLight = pController->IsConnected() ? g_playerManager->GetPlayer(j)->GetFlashLight() : nullptr;

			if (!g_cvarFlashLightTransmitOthers.Get() && pFlashLight)
				pInfo->m_pTransmitEntity->Clear(pFlashLight->entindex());

			if (g_cvarEnableEntWatch.Get() && g_pEWHandler->IsConfigLoaded())
			{
				// Don't transmit other players' entwatch hud
				CPointWorldText* pHud = pController->IsConnected() ? g_playerManager->GetPlayer(j)->GetEntwatchHud() : nullptr;
				if (pHud)
					pInfo->m_pTransmitEntity->Clear(pHud->entindex());
			}

			// Always transmit other players if spectating
			if (!g_cvarEnableHide.Get() || pSelfController->GetPawnState() == STATE_OBSERVER_MODE)
				continue;

			// Get the actual pawn as the player could be currently spectating
			CCSPlayerPawn* pPawn = pController->GetPlayerPawn();

			if (!pPawn)
				continue;

			// Hide players marked as hidden or ANY dead player, it seems that a ragdoll of a previously hidden player can crash?
			// TODO: Revert this if/when valve fixes the issue?
			// Also do not hide leaders to other players
			ZEPlayer* pOtherZEPlayer = g_playerManager->GetPlayer(j);
			if ((pSelfZEPlayer->ShouldBlockTransmit(j) && (pOtherZEPlayer && !pOtherZEPlayer->IsLeader())) || !pPawn->IsAlive())
				pInfo->m_pTransmitEntity->Clear(pPawn->entindex());
		}

		// Don't transmit glow model to it's owner
		CBaseModelEntity* pGlowModel = pSelfZEPlayer->GetGlowModel();

		if (pGlowModel)
			pInfo->m_pTransmitEntity->Clear(pGlowModel->entindex());
	}

	return {KHook::Action::Ignore};
}

KHook::Return<void> CS2Fixes::Hook_ApplyGameSettings(IServerGameDLL* pThis, KeyValues* pKV)
{
	g_pMapVoteSystem->ApplyGameSettings(pKV);

	return {KHook::Action::Ignore};
}

KHook::Return<void> CS2Fixes::Hook_CreateWorkshopMapGroup(IGameTypes* pThis, const char* name, CUtlStringList& mapList)
{
	// TODO: does this even work? just guessing how param override might work in khook..
	if (g_cvarVoteManagerEnable.Get() && g_pMapVoteSystem->IsMapListLoaded())
		mapList = g_pMapVoteSystem->CreateWorkshopMapGroup();

	return {KHook::Action::Ignore};
}

KHook::Return<void> CS2Fixes::Hook_GoToIntermission(CCSGameRules* pThis, bool bAbortedMatch)
{
	if (!g_pMapVoteSystem->IsIntermissionAllowed())
		return {KHook::Action::Supercede};

	if (g_cvarVoteManagerEnable.Get())
		g_pVoteManager->OnIntermission();

	return {KHook::Action::Ignore};
}

CConVar<bool> g_cvarDropMapWeapons("cs2f_drop_map_weapons", FCVAR_NONE, "Whether to force drop map-spawned weapons on death", false);

KHook::Return<bool> CS2Fixes::Hook_OnTakeDamage_Alive(CCSPlayerPawn* pPawn, CTakeDamageInfoContainer* pInfoContainer)
{
	if (g_cvarEnableZR.Get() && ZR_Hook_OnTakeDamage_Alive(pInfoContainer->pInfo, pPawn))
		return {KHook::Action::Supercede, false};

	// This is a shit place to be doing this, but player_death event is too late and there is no pre-hook alternative
	// Check if this is going to kill the player
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

	// TODO: verify true return isn't needed
	return {KHook::Action::Ignore};
}

CConVar<bool> g_cvarFixPhysicsPlayerShuffle("cs2f_shuffle_player_physics_sim", FCVAR_NONE, "Whether to enable shuffle player list in physics simulate", false);

struct TouchLinked_t
{
	uint32_t TouchFlags;

private:
	uint8_t padding_0[20];

public:
	CBaseHandle SourceHandle;
	CBaseHandle TargetHandle;

private:
	uint8_t padding_1[208];

public:
	[[nodiscard]] bool IsUnTouching() const
	{
		return !!(TouchFlags & 0x10);
	}

	[[nodiscard]] bool IsTouching() const
	{
		return (!!(TouchFlags & 4)) || (!!(TouchFlags & 8));
	}
};
static_assert(sizeof(TouchLinked_t) == 240, "Touch_t size mismatch");
KHook::Return<void> CS2Fixes::Hook_PhysicsTouchShuffle_Post(CVPhys2World* pThis, CUtlVector<TouchLinked_t>* pList, bool unknown)
{
	if (!g_cvarFixPhysicsPlayerShuffle.Get() || pList->Count() <= 1)
		return {KHook::Action::Ignore};

	// [Kxnrl]
	// seems it sorted by flags?

	if (GetGlobals())
		std::srand(GetGlobals()->tickcount);

	// Fisher-Yates shuffle

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

KHook::Return<void> CS2Fixes::Hook_CheckMovingGround(CCSPlayer_MovementServices* pThis, double frametime)
{
	CCSPlayerPawn* pPawn = pThis->GetPawn();

	if (!pPawn || !GetGlobals())
		return {KHook::Action::Ignore};

	CCSPlayerController* pController = pPawn->GetOriginalController();

	if (!pController)
		return {KHook::Action::Ignore};

	int iSlot = pController->GetPlayerSlot();

	static int aPlayerTicks[MAXPLAYERS] = {0};

	// The point of doing this is to avoid running the function (and applying/resetting basevelocity) multiple times per tick
	// This can happen when the client or server lags
	if (aPlayerTicks[iSlot] == GetGlobals()->tickcount)
		return {KHook::Action::Supercede};

	aPlayerTicks[iSlot] = GetGlobals()->tickcount;

	return {KHook::Action::Ignore};
}

KHook::Return<void> CS2Fixes::Hook_DropWeapon_Post(CCSPlayer_WeaponServices* pThis, CBasePlayerWeapon* pWeapon, Vector* pVecTarget, Vector* pVelocity)
{
	if (g_cvarEnableEntWatch.Get())
		EW_DropWeapon(pThis, pWeapon);

	return {KHook::Action::Ignore};
}

KHook::Return<int> CS2Fixes::Hook_LoadEventsFromFile(IGameEventManager2* pThis, const char* filename, bool bSearchAll)
{
	ExecuteOnce(g_gameEventManager = pThis);

	// TODO: verify 0 return isn't needed
	return {KHook::Action::Ignore};
}

void CS2Fixes::OnLevelInit(char const* pMapName,
						   char const* pMapEntities,
						   char const* pOldLevel,
						   char const* pLandmarkName,
						   bool loadGame,
						   bool background)
{
	Message("OnLevelInit(%s)\n", pMapName);
	g_iRoundNum = 0;

	// run our cfg
	g_pEngineServer2->ServerCommand("exec cs2fixes/cs2fixes");

	// Run map cfg (if present)
	char cmd[MAX_PATH];
	V_snprintf(cmd, sizeof(cmd), "exec cs2fixes/maps/%s", pMapName);
	g_pEngineServer2->ServerCommand(cmd);

	g_playerManager->SetupInfiniteAmmo();
	g_pMapVoteSystem->OnLevelInit(pMapName);

	if (g_cvarEnableZR.Get())
		ZR_OnLevelInit();

	CCSPlayer_ItemServices::ResetAwsProcessing();

	EntityHandler_OnLevelInit();

	if (g_cvarEnableEntWatch.Get())
		EW_OnLevelInit(pMapName);
}

void CS2Fixes::OnLevelShutdown()
{
	Message("OnLevelShutdown()\n");

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

const char* CS2Fixes::GetLicense()
{
	return "GPL v3 License";
}

const char* CS2Fixes::GetVersion()
{
#ifndef CS2FIXES_VERSION
	#define CS2FIXES_VERSION "1.7-dev"
#endif

	return CS2FIXES_VERSION; // defined by the build script
}

const char* CS2Fixes::GetDate()
{
	return __DATE__;
}

const char* CS2Fixes::GetLogTag()
{
	return "CS2Fixes";
}

const char* CS2Fixes::GetAuthor()
{
	return "xen, Poggu, and the Source2ZE community";
}

const char* CS2Fixes::GetDescription()
{
	return "A bunch of experiments thrown together into one big mess of a plugin.";
}

const char* CS2Fixes::GetName()
{
	return "CS2Fixes";
}

const char* CS2Fixes::GetURL()
{
	return "https://github.com/Source2ZE/CS2Fixes";
}