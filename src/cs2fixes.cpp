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
#include "entitylistener.h"
#include "entitysystem.h"
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

SH_DECL_HOOK3_void(IServerGameDLL, GameFrame, SH_NOATTRIB, 0, bool, bool, bool);
SH_DECL_HOOK0_void(IServerGameDLL, GameServerSteamAPIActivated, SH_NOATTRIB, 0);
SH_DECL_HOOK0_void(IServerGameDLL, GameServerSteamAPIDeactivated, SH_NOATTRIB, 0);
SH_DECL_HOOK1_void(IServerGameDLL, ApplyGameSettings, SH_NOATTRIB, 0, KeyValues*);
SH_DECL_HOOK4_void(IServerGameClients, ClientActive, SH_NOATTRIB, 0, CPlayerSlot, bool, const char*, uint64);
SH_DECL_HOOK5_void(IServerGameClients, ClientDisconnect, SH_NOATTRIB, 0, CPlayerSlot, ENetworkDisconnectionReason, const char*, uint64, const char*);
SH_DECL_HOOK4_void(IServerGameClients, ClientPutInServer, SH_NOATTRIB, 0, CPlayerSlot, char const*, int, uint64);
SH_DECL_HOOK1_void(IServerGameClients, ClientSettingsChanged, SH_NOATTRIB, 0, CPlayerSlot);
SH_DECL_HOOK6_void(IServerGameClients, OnClientConnected, SH_NOATTRIB, 0, CPlayerSlot, const char*, uint64, const char*, const char*, bool);
SH_DECL_HOOK6(IServerGameClients, ClientConnect, SH_NOATTRIB, 0, bool, CPlayerSlot, const char*, uint64, const char*, bool, CBufferString*);
SH_DECL_HOOK8_void(IGameEventSystem, PostEventAbstract, SH_NOATTRIB, 0, CSplitScreenSlot, bool, int, const uint64*,
				   INetworkMessageInternal*, const CNetMessage*, unsigned long, NetChannelBufType_t)
	SH_DECL_HOOK3_void(INetworkServerService, StartupServer, SH_NOATTRIB, 0, const GameSessionConfiguration_t&, ISource2WorldSession*, const char*);
SH_DECL_HOOK7_void(ISource2GameEntities, CheckTransmit, SH_NOATTRIB, 0, CCheckTransmitInfo**, int, CBitVec<16384>&, const Entity2Networkable_t**, const uint16*, int, bool);
SH_DECL_HOOK2_void(IServerGameClients, ClientCommand, SH_NOATTRIB, 0, CPlayerSlot, const CCommand&);
SH_DECL_HOOK3_void(ICvar, DispatchConCommand, SH_NOATTRIB, 0, ConCommandHandle, const CCommandContext&, const CCommand&);
SH_DECL_MANUALHOOK1_void(CGamePlayerEquipUse, 0, 0, 0, InputData_t*);
SH_DECL_MANUALHOOK2_void(CreateWorkshopMapGroup, 0, 0, 0, const char*, const CUtlStringList&);
SH_DECL_MANUALHOOK1(OnTakeDamage_Alive, 0, 0, 0, bool, CTakeDamageInfoContainer*);
SH_DECL_MANUALHOOK1_void(CheckMovingGround, 0, 0, 0, double);
SH_DECL_HOOK2(IGameEventManager2, LoadEventsFromFile, SH_NOATTRIB, 0, int, const char*, bool);
SH_DECL_MANUALHOOK1_void(GoToIntermission, 0, 0, 0, bool);
SH_DECL_MANUALHOOK2_void(PhysicsTouchShuffle, 0, 0, 0, CUtlVector<TouchLinked_t>*, bool);

CS2Fixes g_CS2Fixes;

IGameEventSystem* g_gameEventSystem = nullptr;
IGameEventManager2* g_gameEventManager = nullptr;
INetworkGameServer* g_pNetworkGameServer = nullptr;
CGameEntitySystem* g_pEntitySystem = nullptr;
CEntityListener* g_pEntityListener = nullptr;
CGlobalVars* gpGlobals = nullptr;
CPlayerManager* g_playerManager = nullptr;
IVEngineServer2* g_pEngineServer2 = nullptr;
CGameConfig* g_GameConfig = nullptr;
ISteamHTTP* g_http = nullptr;
CSteamGameServerAPIContext g_steamAPI;
CCSGameRules* g_pGameRules = nullptr;
int g_iCGamePlayerEquipUseId = -1;
int g_iCreateWorkshopMapGroupId = -1;
int g_iOnTakeDamageAliveId = -1;
int g_iCheckMovingGroundId = -1;
int g_iLoadEventsFromFileId = -1;
int g_iGoToIntermissionId = -1;
int g_iPhysicsTouchShuffle = -1;

CGameEntitySystem* GameEntitySystem()
{
	static int offset = g_GameConfig->GetOffset("GameEntitySystem");
	return *reinterpret_cast<CGameEntitySystem**>((uintptr_t)(g_pGameResourceServiceServer) + offset);
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

	int offset = g_GameConfig->GetOffset("IGameTypes_CreateWorkshopMapGroup");
	SH_MANUALHOOK_RECONFIGURE(CreateWorkshopMapGroup, offset, 0, 0);

	SH_ADD_HOOK(IServerGameDLL, GameFrame, g_pSource2Server, SH_MEMBER(this, &CS2Fixes::Hook_GameFramePost), true);
	SH_ADD_HOOK(IServerGameDLL, GameServerSteamAPIActivated, g_pSource2Server, SH_MEMBER(this, &CS2Fixes::Hook_GameServerSteamAPIActivated), false);
	SH_ADD_HOOK(IServerGameDLL, GameServerSteamAPIDeactivated, g_pSource2Server, SH_MEMBER(this, &CS2Fixes::Hook_GameServerSteamAPIDeactivated), false);
	SH_ADD_HOOK(IServerGameDLL, ApplyGameSettings, g_pSource2Server, SH_MEMBER(this, &CS2Fixes::Hook_ApplyGameSettings), false);
	SH_ADD_HOOK(IServerGameClients, ClientActive, g_pSource2GameClients, SH_MEMBER(this, &CS2Fixes::Hook_ClientActive), true);
	SH_ADD_HOOK(IServerGameClients, ClientDisconnect, g_pSource2GameClients, SH_MEMBER(this, &CS2Fixes::Hook_ClientDisconnect), true);
	SH_ADD_HOOK(IServerGameClients, ClientPutInServer, g_pSource2GameClients, SH_MEMBER(this, &CS2Fixes::Hook_ClientPutInServer), true);
	SH_ADD_HOOK(IServerGameClients, ClientSettingsChanged, g_pSource2GameClients, SH_MEMBER(this, &CS2Fixes::Hook_ClientSettingsChanged), false);
	SH_ADD_HOOK(IServerGameClients, OnClientConnected, g_pSource2GameClients, SH_MEMBER(this, &CS2Fixes::Hook_OnClientConnected), false);
	SH_ADD_HOOK(IServerGameClients, ClientConnect, g_pSource2GameClients, SH_MEMBER(this, &CS2Fixes::Hook_ClientConnect), false);
	SH_ADD_HOOK(IServerGameClients, ClientCommand, g_pSource2GameClients, SH_MEMBER(this, &CS2Fixes::Hook_ClientCommand), false);
	SH_ADD_HOOK(IGameEventSystem, PostEventAbstract, g_gameEventSystem, SH_MEMBER(this, &CS2Fixes::Hook_PostEvent), false);
	SH_ADD_HOOK(INetworkServerService, StartupServer, g_pNetworkServerService, SH_MEMBER(this, &CS2Fixes::Hook_StartupServer), true);
	SH_ADD_HOOK(ISource2GameEntities, CheckTransmit, g_pSource2GameEntities, SH_MEMBER(this, &CS2Fixes::Hook_CheckTransmit), true);
	SH_ADD_HOOK(ICvar, DispatchConCommand, g_pCVar, SH_MEMBER(this, &CS2Fixes::Hook_DispatchConCommand), false);
	g_iCreateWorkshopMapGroupId = SH_ADD_MANUALVPHOOK(CreateWorkshopMapGroup, g_pGameTypes, SH_MEMBER(this, &CS2Fixes::Hook_CreateWorkshopMapGroup), false);

	META_CONPRINTF("All hooks started!\n");

	bool bRequiredInitLoaded = true;

	if (!addresses::Initialize(g_GameConfig))
		bRequiredInitLoaded = false;

	if (!InitPatches(g_GameConfig))
		bRequiredInitLoaded = false;

	if (!InitDetours(g_GameConfig))
		bRequiredInitLoaded = false;

	if (!InitGameSystems())
		bRequiredInitLoaded = false;

	const auto pCGamePlayerEquipVTable = modules::server->FindVirtualTable("CGamePlayerEquip");
	if (!pCGamePlayerEquipVTable)
	{
		snprintf(error, maxlen, "Failed to find CGamePlayerEquip vtable\n");
		bRequiredInitLoaded = false;
	}

	offset = g_GameConfig->GetOffset("CBaseEntity::Use");
	if (offset == -1)
	{
		snprintf(error, maxlen, "Failed to find CBaseEntity::Use\n");
		bRequiredInitLoaded = false;
	}
	SH_MANUALHOOK_RECONFIGURE(CGamePlayerEquipUse, offset, 0, 0);
	g_iCGamePlayerEquipUseId = SH_ADD_MANUALDVPHOOK(CGamePlayerEquipUse, pCGamePlayerEquipVTable, SH_MEMBER(this, &CS2Fixes::Hook_CGamePlayerEquipUse), false);

	const auto pCCSPlayerPawnVTable = modules::server->FindVirtualTable("CCSPlayerPawn");
	if (!pCCSPlayerPawnVTable)
	{
		snprintf(error, maxlen, "Failed to find CCSPlayerPawn vtable\n");
		bRequiredInitLoaded = false;
	}

	offset = g_GameConfig->GetOffset("CBasePlayerPawn::OnTakeDamage_Alive");
	if (offset == -1)
	{
		snprintf(error, maxlen, "Failed to find CBasePlayerPawn::OnTakeDamage_Alive\n");
		bRequiredInitLoaded = false;
	}
	SH_MANUALHOOK_RECONFIGURE(OnTakeDamage_Alive, offset, 0, 0);
	g_iOnTakeDamageAliveId = SH_ADD_MANUALDVPHOOK(OnTakeDamage_Alive, pCCSPlayerPawnVTable, SH_MEMBER(this, &CS2Fixes::Hook_OnTakeDamage_Alive), false);

	const auto pCCSPlayer_MovementServicesVTable = modules::server->FindVirtualTable("CCSPlayer_MovementServices");
	offset = g_GameConfig->GetOffset("CCSPlayer_MovementServices::CheckMovingGround");
	if (offset == -1)
	{
		snprintf(error, maxlen, "Failed to find CCSPlayer_MovementServices::CheckMovingGround\n");
		bRequiredInitLoaded = false;
	}
	SH_MANUALHOOK_RECONFIGURE(CheckMovingGround, offset, 0, 0);
	g_iCheckMovingGroundId = SH_ADD_MANUALDVPHOOK(CheckMovingGround, pCCSPlayer_MovementServicesVTable, SH_MEMBER(this, &CS2Fixes::Hook_CheckMovingGround), false);

	auto pCVPhys2WorldVTable = modules::vphysics2->FindVirtualTable("CVPhys2World");

	offset = g_GameConfig->GetOffset("CVPhys2World::GetTouchingList");
	if (offset == -1)
	{
		snprintf(error, maxlen, "Failed to find CVPhys2World::GetTouchingList\n");
		bRequiredInitLoaded = false;
	}
	SH_MANUALHOOK_RECONFIGURE(PhysicsTouchShuffle, offset, 0, 0);
	g_iPhysicsTouchShuffle = SH_ADD_MANUALDVPHOOK(PhysicsTouchShuffle, pCVPhys2WorldVTable, SH_MEMBER(this, &CS2Fixes::Hook_PhysicsTouchShuffle), true);

	auto pCGameEventManagerVTable = (IGameEventManager2*)modules::server->FindVirtualTable("CGameEventManager");

	g_iLoadEventsFromFileId = SH_ADD_DVPHOOK(IGameEventManager2, LoadEventsFromFile, pCGameEventManagerVTable, SH_MEMBER(this, &CS2Fixes::Hook_LoadEventsFromFile), false);

	if (!bRequiredInitLoaded)
	{
		snprintf(error, maxlen, "One or more address lookups, patches or detours failed, please refer to startup logs for more information");
		return false;
	}

	auto pCCSGameRulesVTable = modules::server->FindVirtualTable("CCSGameRules");

	offset = g_GameConfig->GetOffset("CCSGameRules_GoToIntermission");
	if (offset == -1)
	{
		snprintf(error, maxlen, "Failed to find CCSGameRules::GoToIntermission\n");
		bRequiredInitLoaded = false;
	}
	SH_MANUALHOOK_RECONFIGURE(GoToIntermission, offset, 0, 0);
	g_iGoToIntermissionId = SH_ADD_MANUALDVPHOOK(GoToIntermission, pCCSGameRulesVTable, SH_MEMBER(this, &CS2Fixes::Hook_GoToIntermission), false);

	Message("All hooks started!\n");

	UnlockConVars();
	UnlockConCommands();
	ConVar_Register(FCVAR_RELEASE | FCVAR_CLIENT_CAN_EXECUTE | FCVAR_GAMEDLL);

	if (late)
	{
		RegisterEventListeners();
		g_pEntitySystem = GameEntitySystem();
		g_pEntitySystem->AddListenerEntity(g_pEntityListener);
		g_pNetworkGameServer = g_pNetworkServerService->GetIGameServer();
		gpGlobals = g_pEngineServer2->GetServerGlobals();
	}

	g_pAdminSystem = new CAdminSystem();
	g_playerManager = new CPlayerManager(late);
	g_pDiscordBotManager = new CDiscordBotManager();
	g_pZRPlayerClassManager = new CZRPlayerClassManager();
	g_pMapVoteSystem = new CMapVoteSystem();
	g_pUserPreferencesSystem = new CUserPreferencesSystem();
	g_pUserPreferencesStorage = new CUserPreferencesREST();
	g_pZRWeaponConfig = new ZRWeaponConfig();
	g_pZRHitgroupConfig = new ZRHitgroupConfig();
	g_pEntityListener = new CEntityListener();
	g_pIdleSystem = new CIdleSystem();
	g_pPanoramaVoteHandler = new CPanoramaVoteHandler();

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

	Message("Plugin successfully started!\n");

	return true;
}

bool CS2Fixes::Unload(char* error, size_t maxlen)
{
	SH_REMOVE_HOOK(IServerGameDLL, GameFrame, g_pSource2Server, SH_MEMBER(this, &CS2Fixes::Hook_GameFramePost), true);
	SH_REMOVE_HOOK(IServerGameDLL, GameServerSteamAPIActivated, g_pSource2Server, SH_MEMBER(this, &CS2Fixes::Hook_GameServerSteamAPIActivated), false);
	SH_REMOVE_HOOK(IServerGameDLL, GameServerSteamAPIDeactivated, g_pSource2Server, SH_MEMBER(this, &CS2Fixes::Hook_GameServerSteamAPIDeactivated), false);
	SH_REMOVE_HOOK(IServerGameDLL, ApplyGameSettings, g_pSource2Server, SH_MEMBER(this, &CS2Fixes::Hook_ApplyGameSettings), false);
	SH_REMOVE_HOOK(IServerGameClients, ClientActive, g_pSource2GameClients, SH_MEMBER(this, &CS2Fixes::Hook_ClientActive), true);
	SH_REMOVE_HOOK(IServerGameClients, ClientDisconnect, g_pSource2GameClients, SH_MEMBER(this, &CS2Fixes::Hook_ClientDisconnect), true);
	SH_REMOVE_HOOK(IServerGameClients, ClientPutInServer, g_pSource2GameClients, SH_MEMBER(this, &CS2Fixes::Hook_ClientPutInServer), true);
	SH_REMOVE_HOOK(IServerGameClients, ClientSettingsChanged, g_pSource2GameClients, SH_MEMBER(this, &CS2Fixes::Hook_ClientSettingsChanged), false);
	SH_REMOVE_HOOK(IServerGameClients, OnClientConnected, g_pSource2GameClients, SH_MEMBER(this, &CS2Fixes::Hook_OnClientConnected), false);
	SH_REMOVE_HOOK(IServerGameClients, ClientConnect, g_pSource2GameClients, SH_MEMBER(this, &CS2Fixes::Hook_ClientConnect), false);
	SH_REMOVE_HOOK(IServerGameClients, ClientCommand, g_pSource2GameClients, SH_MEMBER(this, &CS2Fixes::Hook_ClientCommand), false);
	SH_REMOVE_HOOK(IGameEventSystem, PostEventAbstract, g_gameEventSystem, SH_MEMBER(this, &CS2Fixes::Hook_PostEvent), false);
	SH_REMOVE_HOOK(INetworkServerService, StartupServer, g_pNetworkServerService, SH_MEMBER(this, &CS2Fixes::Hook_StartupServer), true);
	SH_REMOVE_HOOK(ISource2GameEntities, CheckTransmit, g_pSource2GameEntities, SH_MEMBER(this, &CS2Fixes::Hook_CheckTransmit), true);
	SH_REMOVE_HOOK(ICvar, DispatchConCommand, g_pCVar, SH_MEMBER(this, &CS2Fixes::Hook_DispatchConCommand), false);
	SH_REMOVE_HOOK_ID(g_iLoadEventsFromFileId);
	SH_REMOVE_HOOK_ID(g_iCreateWorkshopMapGroupId);
	SH_REMOVE_HOOK_ID(g_iOnTakeDamageAliveId);
	SH_REMOVE_HOOK_ID(g_iCheckMovingGroundId);
	SH_REMOVE_HOOK_ID(g_iPhysicsTouchShuffle);

	if (g_iGoToIntermissionId != -1)
		SH_REMOVE_HOOK_ID(g_iGoToIntermissionId);

	ConVar_Unregister();

	g_CommandList.Purge();

	FlushAllDetours();
	UndoPatches();
	RemoveTimers();
	UnregisterEventListeners();

	if (g_playerManager)
		delete g_playerManager;

	if (g_pAdminSystem)
		delete g_pAdminSystem;

	if (g_pDiscordBotManager)
		delete g_pDiscordBotManager;

	if (g_GameConfig)
		delete g_GameConfig;

	if (g_pZRPlayerClassManager)
		delete g_pZRPlayerClassManager;

	if (g_pZRWeaponConfig)
		delete g_pZRWeaponConfig;

	if (g_pZRHitgroupConfig)
		delete g_pZRHitgroupConfig;

	if (g_pUserPreferencesSystem)
		delete g_pUserPreferencesSystem;

	if (g_pUserPreferencesStorage)
		delete g_pUserPreferencesStorage;

	if (g_pEntityListener)
		delete g_pEntityListener;

	if (g_pIdleSystem)
		delete g_pIdleSystem;

	if (g_pPanoramaVoteHandler)
		delete g_pPanoramaVoteHandler;

	if (g_iCGamePlayerEquipUseId != -1)
		SH_REMOVE_HOOK_ID(g_iCGamePlayerEquipUseId);

	return true;
}

void CS2Fixes::Hook_DispatchConCommand(ConCommandHandle cmdHandle, const CCommandContext& ctx, const CCommand& args)
{
	VPROF_BUDGET("CS2Fixes::Hook_DispatchConCommand", "ConCommands");

	if (!g_pEntitySystem)
		RETURN_META(MRES_IGNORED);

	auto iCommandPlayerSlot = ctx.GetPlayerSlot();

	if (!g_bEnableCommands)
		RETURN_META(MRES_IGNORED);

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
			SH_CALL(g_pCVar, &ICvar::DispatchConCommand)
			(cmdHandle, ctx, args);
		}
		else if (bFlooding)
		{
			if (pController)
				ClientPrint(pController, HUD_PRINTTALK, CHAT_PREFIX "You are flooding the server!");
		}
		else if (bAdminChat) // Admin chat can be sent by anyone but only seen by admins, use flood protection here too
		{
			// HACK: At this point, we can safely modify the arg buffer as it won't be passed anywhere else
			// The string here is originally ("@foo bar"), trim it to be (foo bar)
			char* pszMessage = (char*)(args.ArgS() + 2);
			pszMessage[V_strlen(pszMessage) - 1] = 0;

			for (int i = 0; i < gpGlobals->maxClients; i++)
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
			if (bSilent && pszMessage[V_strlen(pszMessage) - 1] == '"')
				pszMessage[V_strlen(pszMessage) - 1] = '\0';

			ParseChatCommand(pszMessage, pController);
		}

		RETURN_META(MRES_SUPERCEDE);
	}

	RETURN_META(MRES_IGNORED);
}

void CS2Fixes::Hook_StartupServer(const GameSessionConfiguration_t& config, ISource2WorldSession* pSession, const char* pszMapName)
{
	g_pNetworkGameServer = g_pNetworkServerService->GetIGameServer();
	g_pEntitySystem = GameEntitySystem();
	g_pEntitySystem->AddListenerEntity(g_pEntityListener);
	gpGlobals = g_pEngineServer2->GetServerGlobals();

	Message("Hook_StartupServer: %s\n", pszMapName);

	if (g_bHasTicked)
		RemoveMapTimers();

	g_bHasTicked = false;

	RegisterEventListeners();

	g_pPanoramaVoteHandler->Reset();
	VoteManager_Init();

	g_pIdleSystem->Reset();
}

class CGamePlayerEquip;
void CS2Fixes::Hook_CGamePlayerEquipUse(InputData_t* pInput)
{
	CGamePlayerEquipHandler::Use(META_IFACEPTR(CGamePlayerEquip), pInput);
	RETURN_META(MRES_IGNORED);
}

void CS2Fixes::Hook_GameServerSteamAPIActivated()
{
	g_steamAPI.Init();
	g_http = g_steamAPI.SteamHTTP();

	g_playerManager->OnSteamAPIActivated();

	if (g_bVoteManagerEnable && !g_pMapVoteSystem->IsMapListLoaded())
		g_pMapVoteSystem->LoadMapList();

	RETURN_META(MRES_IGNORED);
}

void CS2Fixes::Hook_GameServerSteamAPIDeactivated()
{
	g_http = nullptr;

	RETURN_META(MRES_IGNORED);
}

void CS2Fixes::Hook_PostEvent(CSplitScreenSlot nSlot, bool bLocalOnly, int nClientCount, const uint64* clients,
							  INetworkMessageInternal* pEvent, const CNetMessage* pData, unsigned long nSize, NetChannelBufType_t bufType)
{
	// Message( "Hook_PostEvent(%d, %d, %d, %lli)\n", nSlot, bLocalOnly, nClientCount, clients );
	// Need to explicitly get a pointer to the right function as it's overloaded and SH_CALL can't resolve that
	static void (IGameEventSystem::*PostEventAbstract)(CSplitScreenSlot, bool, int, const uint64*,
													   INetworkMessageInternal*, const CNetMessage*, unsigned long, NetChannelBufType_t) = &IGameEventSystem::PostEventAbstract;

	NetMessageInfo_t* info = pEvent->GetNetMessageInfo();

	if (g_bEnableStopSound && info->m_MessageId == GE_FireBulletsId)
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

			SH_CALL(g_gameEventSystem, PostEventAbstract)
			(nSlot, bLocalOnly, nClientCount, &clientMask, pEvent, msg, nSize, bufType);

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
		if (g_bEnableLeader)
			Leader_PostEventAbstract_Source1LegacyGameEvent(clients, pData);
	}
	else if (info->m_MessageId == UM_Shake)
	{
		auto pPBData = const_cast<CNetMessage*>(pData)->ToPB<CUserMessageShake>();
		if (g_flMaxShakeAmp >= 0 && pPBData->amplitude() > g_flMaxShakeAmp)
			pPBData->set_amplitude(g_flMaxShakeAmp);

		// remove client with noshake from the event
		if (g_bEnableNoShake)
			*(uint64*)clients &= ~g_playerManager->GetNoShakeMask();
	}
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
	if (!g_pNetworkGameServer)
		return nullptr;

	static int offset = g_GameConfig->GetOffset("CNetworkGameServer_ClientList");
	return (CUtlVector<CServerSideClient*>*)(&g_pNetworkGameServer[offset]);
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

	FOR_EACH_VEC(*pClients, i)
	(*pClients)[i]->ForceFullUpdate();
}

// Because sv_fullupdate doesn't work
CON_COMMAND_F(cs2f_fullupdate, "Force a full update for all clients.", FCVAR_LINKED_CONCOMMAND | FCVAR_SPONLY)
{
	FullUpdateAllClients();
}

void CS2Fixes::Hook_ClientActive(CPlayerSlot slot, bool bLoadGame, const char* pszName, uint64 xuid)
{
	Message("Hook_ClientActive(%d, %d, \"%s\", %lli)\n", slot, bLoadGame, pszName, xuid);
}

void CS2Fixes::Hook_ClientCommand(CPlayerSlot slot, const CCommand& args)
{
#ifdef _DEBUG
	Message("Hook_ClientCommand(%d, \"%s\")\n", slot, args.GetCommandString());
#endif

	if (g_fIdleKickTime > 0.0f)
	{
		ZEPlayer* pPlayer = g_playerManager->GetPlayer(slot);

		if (pPlayer)
			pPlayer->UpdateLastInputTime();
	}

	if (g_bVoteManagerEnable && V_stricmp(args[0], "endmatch_votenextmap") == 0 && args.ArgC() == 2)
	{
		if (g_pMapVoteSystem->RegisterPlayerVote(slot, atoi(args[1])))
			RETURN_META(MRES_HANDLED);
		else
			RETURN_META(MRES_SUPERCEDE);
	}

	if (g_bEnableZR && slot != -1 && !V_strncmp(args.Arg(0), "jointeam", 8))
	{
		ZR_Hook_ClientCommand_JoinTeam(slot, args);
		RETURN_META(MRES_SUPERCEDE);
	}
}

void CS2Fixes::Hook_ClientSettingsChanged(CPlayerSlot slot)
{
#ifdef _DEBUG
	Message("Hook_ClientSettingsChanged(%d)\n", slot);
#endif
}

void CS2Fixes::Hook_OnClientConnected(CPlayerSlot slot, const char* pszName, uint64 xuid, const char* pszNetworkID, const char* pszAddress, bool bFakePlayer)
{
	Message("Hook_OnClientConnected(%d, \"%s\", %lli, \"%s\", \"%s\", %d)\n", slot, pszName, xuid, pszNetworkID, pszAddress, bFakePlayer);

	// CONVAR_TODO
	// HACK: values is actually the cvar value itself, hence this ugly cast.
	ConVar* cvar = g_pCVar->GetConVar(g_pCVar->FindConVar("tv_name"));
	const char* pszTvName = *(const char**)&cvar->values;

	// Ideally we would use CServerSideClient::IsHLTV().. but it doesn't work :(
	if (bFakePlayer && V_strcmp(pszName, pszTvName))
		g_playerManager->OnBotConnected(slot);
}

bool CS2Fixes::Hook_ClientConnect(CPlayerSlot slot, const char* pszName, uint64 xuid, const char* pszNetworkID, bool unk1, CBufferString* pRejectReason)
{
	Message("Hook_ClientConnect(%d, \"%s\", %lli, \"%s\", %d, \"%s\")\n", slot, pszName, xuid, pszNetworkID, unk1, pRejectReason->ToGrowable()->Get());

	// Player is banned
	if (!g_playerManager->OnClientConnected(slot, xuid, pszNetworkID))
		RETURN_META_VALUE(MRES_SUPERCEDE, false);

	RETURN_META_VALUE(MRES_IGNORED, true);
}

void CS2Fixes::Hook_ClientPutInServer(CPlayerSlot slot, char const* pszName, int type, uint64 xuid)
{
	Message("Hook_ClientPutInServer(%d, \"%s\", %d, %d, %lli)\n", slot, pszName, type, xuid);

	if (!g_playerManager->GetPlayer(slot))
		return;

	g_playerManager->OnClientPutInServer(slot);

	if (g_bEnableZR)
		ZR_Hook_ClientPutInServer(slot, pszName, type, xuid);
}

void CS2Fixes::Hook_ClientDisconnect(CPlayerSlot slot, ENetworkDisconnectionReason reason, const char* pszName, uint64 xuid, const char* pszNetworkID)
{
	Message("Hook_ClientDisconnect(%d, %d, \"%s\", %lli)\n", slot, reason, pszName, xuid);
	ZEPlayer* pPlayer = g_playerManager->GetPlayer(slot);

	if (!pPlayer)
		return;

	// Dont add to c_listdc clients that are downloading MultiAddonManager stuff or were present during a map change
	if (reason != NETWORK_DISCONNECT_LOOPSHUTDOWN && reason != NETWORK_DISCONNECT_SHUTDOWN)
		g_pAdminSystem->AddDisconnectedPlayer(pszName, xuid, pPlayer ? pPlayer->GetIpAddress() : "");

	g_playerManager->OnClientDisconnect(slot);
}

void CS2Fixes::Hook_GameFramePost(bool simulating, bool bFirstTick, bool bLastTick)
{
	/**
	 * simulating:
	 * ***********
	 * true  | game is ticking
	 * false | game is not ticking
	 */

	VPROF_BUDGET("CS2Fixes::Hook_GameFramePost", "CS2FixesPerFrame");

	if (simulating && g_bHasTicked)
		g_flUniversalTime += gpGlobals->curtime - g_flLastTickedTime;

	g_flLastTickedTime = gpGlobals->curtime;
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

	if (g_bEnableZR)
		CZRRegenTimer::Tick();

	EntityHandler_OnGameFramePost(simulating, gpGlobals->tickcount);
}

extern bool g_bFlashLightTransmitOthers;

void CS2Fixes::Hook_CheckTransmit(CCheckTransmitInfo** ppInfoList, int infoCount, CBitVec<16384>& unionTransmitEdicts,
								  const Entity2Networkable_t** pNetworkables, const uint16* pEntityIndicies, int nEntities, bool bEnablePVSBits)
{
	if (!g_pEntitySystem)
		return;

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

		for (int j = 0; j < gpGlobals->maxClients; j++)
		{
			CCSPlayerController* pController = CCSPlayerController::FromSlot(j);
			// Always transmit to themselves
			if (!pController || pController->m_bIsHLTV || j == iPlayerSlot)
				continue;

			// Don't transmit other players' flashlights
			CBarnLight* pFlashLight = pController->IsConnected() ? g_playerManager->GetPlayer(j)->GetFlashLight() : nullptr;

			if (!g_bFlashLightTransmitOthers && pFlashLight)
				pInfo->m_pTransmitEntity->Clear(pFlashLight->entindex());
			// Always transmit other players if spectating
			if (!g_bEnableHide || pSelfController->GetPawnState() == STATE_OBSERVER_MODE)
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
}

void CS2Fixes::Hook_ApplyGameSettings(KeyValues* pKV)
{
	if (!pKV->FindKey("launchoptions"))
		return;

	if (pKV->FindKey("launchoptions")->FindKey("customgamemode"))
		g_pMapVoteSystem->SetCurrentWorkshopMap(pKV->FindKey("launchoptions")->GetUint64("customgamemode"));
	else if (pKV->FindKey("launchoptions")->FindKey("levelname"))
		g_pMapVoteSystem->SetCurrentMap(pKV->FindKey("launchoptions")->GetString("levelname"));
}

void CS2Fixes::Hook_CreateWorkshopMapGroup(const char* name, const CUtlStringList& mapList)
{
	if (g_bVoteManagerEnable && g_pMapVoteSystem->IsMapListLoaded())
		RETURN_META_MNEWPARAMS(MRES_HANDLED, CreateWorkshopMapGroup, (name, g_pMapVoteSystem->CreateWorkshopMapGroup()));
	else
		RETURN_META(MRES_IGNORED);
}

void CS2Fixes::Hook_GoToIntermission(bool bAbortedMatch)
{
	if (!g_pMapVoteSystem->IsIntermissionAllowed())
		RETURN_META(MRES_SUPERCEDE);

	RETURN_META(MRES_IGNORED);
}

bool g_bDropMapWeapons = false;

FAKE_BOOL_CVAR(cs2f_drop_map_weapons, "Whether to force drop map-spawned weapons on death", g_bDropMapWeapons, false, false)

bool CS2Fixes::Hook_OnTakeDamage_Alive(CTakeDamageInfoContainer* pInfoContainer)
{
	CCSPlayerPawn* pPawn = META_IFACEPTR(CCSPlayerPawn);

	if (g_bEnableZR && ZR_Hook_OnTakeDamage_Alive(pInfoContainer->pInfo, pPawn))
		RETURN_META_VALUE(MRES_SUPERCEDE, false);

	// This is a shit place to be doing this, but player_death event is too late and there is no pre-hook alternative
	// Check if this is going to kill the player
	if (g_bDropMapWeapons && pPawn && pPawn->m_iHealth() <= 0)
		pPawn->DropMapWeapons();

	RETURN_META_VALUE(MRES_IGNORED, true);
}

bool g_bFixPhysicsPlayerShuffle = false;
FAKE_BOOL_CVAR(cs2f_shuffle_player_physics_sim, "Whether to enable shuffle player list in physics simulate", g_bFixPhysicsPlayerShuffle, false, false);

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
void CS2Fixes::Hook_PhysicsTouchShuffle(CUtlVector<TouchLinked_t>* pList, bool unknown)
{
	if (!g_bFixPhysicsPlayerShuffle || g_SHPtr->GetStatus() == MRES_SUPERCEDE || pList->Count() <= 1)
		return;

	// [Kxnrl]
	// seems it sorted by flags?

	std::srand(gpGlobals->tickcount);

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
		return;

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
}

void CS2Fixes::Hook_CheckMovingGround(double frametime)
{
	CCSPlayer_MovementServices* pMove = META_IFACEPTR(CCSPlayer_MovementServices);
	CCSPlayerPawn* pPawn = pMove->GetPawn();

	if (!pPawn)
		RETURN_META(MRES_IGNORED);

	CCSPlayerController* pController = pPawn->GetOriginalController();

	if (!pController)
		RETURN_META(MRES_IGNORED);

	int iSlot = pController->GetPlayerSlot();

	static int aPlayerTicks[MAXPLAYERS] = {0};

	// The point of doing this is to avoid running the function (and applying/resetting basevelocity) multiple times per tick
	// This can happen when the client or server lags
	if (aPlayerTicks[iSlot] == gpGlobals->tickcount)
		RETURN_META(MRES_SUPERCEDE);

	aPlayerTicks[iSlot] = gpGlobals->tickcount;

	RETURN_META(MRES_IGNORED);
}

int CS2Fixes::Hook_LoadEventsFromFile(const char* filename, bool bSearchAll)
{
	ExecuteOnce(g_gameEventManager = META_IFACEPTR(IGameEventManager2));

	RETURN_META_VALUE(MRES_IGNORED, 0);
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

	if (g_bEnableZR)
		ZR_OnLevelInit();
}

// Potentially might not work
void CS2Fixes::OnLevelShutdown()
{
	Message("OnLevelShutdown()\n");
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