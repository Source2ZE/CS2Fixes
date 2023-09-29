/**
 * vim: set ts=4 sw=4 tw=99 noet :
 * ======================================================
 * CS2Fixes
 * Written by xen and poggu
 * ======================================================
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * In no event will the authors be held liable for any damages arising from 
 * the use of this software.
 */

#include "detour.h"
#include <stdio.h>
#include "cs2fixes.h"
#include "iserver.h"

#include "appframework/IAppSystem.h"
#include "common.h"
#include "icvar.h"
#include "interface.h"
#include "tier0/dbg.h"
#ifdef _WIN32
//#include "MinHook.h"
//#include <Psapi.h>
#endif
#include "interfaces/cs2_interfaces.h"
#include "plat.h"

void Message(const char *msg, ...)
{
	va_list args;
	va_start(args, msg);

	char buf[1024] = {};
	V_vsnprintf(buf, sizeof(buf) - 1, msg, args);

	ConColorMsg(Color(255, 0, 255, 255), "%s", buf);

#ifdef USE_DEBUG_CONSOLE
	if (!CommandLine()->HasParm("-dedicated"))
		printf(buf);
#endif

	va_end(args);
}

void Panic(const char *msg, ...)
{
	va_list args;
	va_start(args, msg);

	char buf[1024] = {};
	V_vsnprintf(buf, sizeof(buf) - 1, msg, args);

	if (CommandLine()->HasParm("-dedicated"))
		Warning("%s", buf);
	/*else
		MessageBoxA(nullptr, buf, "Warning", 0);*/

	va_end(args);
}

SH_DECL_HOOK3_void(IServerGameDLL, GameFrame, SH_NOATTRIB, 0, bool, bool, bool);
SH_DECL_HOOK4_void(IServerGameClients, ClientActive, SH_NOATTRIB, 0, CPlayerSlot, bool, const char *, uint64);
SH_DECL_HOOK5_void(IServerGameClients, ClientDisconnect, SH_NOATTRIB, 0, CPlayerSlot, int, const char *, uint64, const char *);
SH_DECL_HOOK4_void(IServerGameClients, ClientPutInServer, SH_NOATTRIB, 0, CPlayerSlot, char const *, int, uint64);
SH_DECL_HOOK1_void(IServerGameClients, ClientSettingsChanged, SH_NOATTRIB, 0, CPlayerSlot );
SH_DECL_HOOK6_void(IServerGameClients, OnClientConnected, SH_NOATTRIB, 0, CPlayerSlot, const char*, uint64, const char *, const char *, bool);
SH_DECL_HOOK6(IServerGameClients, ClientConnect, SH_NOATTRIB, 0, bool, CPlayerSlot, const char*, uint64, const char *, bool, CBufferString *);
SH_DECL_HOOK2(IGameEventManager2, FireEvent, SH_NOATTRIB, 0, bool, IGameEvent *, bool);

SH_DECL_HOOK2_void( IServerGameClients, ClientCommand, SH_NOATTRIB, 0, CPlayerSlot, const CCommand & );

CS2Fixes g_CS2Fixes;

// Should only be called within the active game loop (i e map should be loaded and active)
// otherwise that'll be nullptr!
CGlobalVars *GetGameGlobals()
{
	INetworkGameServer *server = g_pNetworkServerService->GetIGameServer();

	if(!server)
		return nullptr;

	return g_pNetworkServerService->GetIGameServer()->GetGlobals();
}

#if 0
// Currently unavailable, requires hl2sdk work!
ConVar sample_cvar("sample_cvar", "42", 0);
#endif

CON_COMMAND_F(sample_command, "Sample command", FCVAR_NONE)
{
	META_CONPRINTF( "Sample command called by %d. Command: %s\n", context.GetPlayerSlot(), args.GetCommandString() );
}

CON_COMMAND_F(unlock_cvars, "Unlock all cvars", FCVAR_NONE)
{
	UnlockConVars();
}

CON_COMMAND_F(unlock_commands, "Unlock all commands", FCVAR_NONE)
{
	UnlockConCommands();
}


CON_COMMAND_F(toggle_logs, "Toggle printing most logs and warnings", FCVAR_NONE)
{
	//ToggleLogs();
}

CON_COMMAND_F(set_max_players, "Set max players through a patch", FCVAR_NONE)
{
	if (args.ArgC() < 2)
	{
		Msg("Usage: set_max_players <maxplayers>\n");
		return;
	}

	//SetMaxPlayers(atoi(args.Arg(1)));
}

void Init()
{
	static bool attached = false;

	if (attached)
		return;
	
	attached = true;

#ifdef USE_DEBUG_CONSOLE
	AllocConsole();
	freopen("CONOUT$", "w", stdout);
#endif

	//MH_Initialize();

#ifdef HOOK_CONVARS
	HookConVars();
#endif
#ifdef HOOK_CONCOMMANDS
	HookConCommands();
#endif
	addresses::Initialize();
	interfaces::Initialize();

	g_pCVar->RegisterConCommand(&unlock_cvars_command);
	g_pCVar->RegisterConCommand(&unlock_commands_command);
	g_pCVar->RegisterConCommand(&toggle_logs_command);
	g_pCVar->RegisterConCommand(&set_max_players_command);

	//InitPatches();
	//InitLoggingDetours();
}

#ifdef USE_TICKRATE
void *g_pfnGetTickInterval = nullptr;

float GetTickIntervalHook()
{
	return 1.0f / CommandLine()->ParmValue("-tickrate", 64.0f);
}
#endif

PLUGIN_EXPOSE(CS2Fixes, g_CS2Fixes);
bool CS2Fixes::Load(PluginId id, ISmmAPI *ismm, char *error, size_t maxlen, bool late)
{
	PLUGIN_SAVEVARS();

	GET_V_IFACE_CURRENT(GetEngineFactory, g_pSource2EngineToServer, ISource2EngineToServer, SOURCE2ENGINETOSERVER_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, g_pCVar, ICvar, CVAR_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetServerFactory, g_pSource2Server, ISource2Server, SOURCE2SERVER_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetServerFactory, g_pSource2GameClients, IServerGameClients, SOURCE2GAMECLIENTS_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetEngineFactory, g_pNetworkServerService, INetworkServerService, NETWORKSERVERSERVICE_INTERFACE_VERSION);

	// Currently doesn't work from within mm side, use GetGameGlobals() in the mean time instead
	// gpGlobals = ismm->GetCGlobals();

	META_CONPRINTF( "Starting plugin.\n" );

	SH_ADD_HOOK_MEMFUNC(IServerGameDLL, GameFrame, g_pSource2Server, this, &CS2Fixes::Hook_GameFrame, true);
	SH_ADD_HOOK_MEMFUNC(IServerGameClients, ClientActive, g_pSource2GameClients, this, &CS2Fixes::Hook_ClientActive, true);
	SH_ADD_HOOK_MEMFUNC(IServerGameClients, ClientDisconnect, g_pSource2GameClients, this, &CS2Fixes::Hook_ClientDisconnect, true);
	SH_ADD_HOOK_MEMFUNC(IServerGameClients, ClientPutInServer, g_pSource2GameClients, this, &CS2Fixes::Hook_ClientPutInServer, true);
	SH_ADD_HOOK_MEMFUNC(IServerGameClients, ClientSettingsChanged, g_pSource2GameClients, this, &CS2Fixes::Hook_ClientSettingsChanged, false);
	SH_ADD_HOOK_MEMFUNC(IServerGameClients, OnClientConnected, g_pSource2GameClients, this, &CS2Fixes::Hook_OnClientConnected, false);
	SH_ADD_HOOK_MEMFUNC(IServerGameClients, ClientConnect, g_pSource2GameClients, this, &CS2Fixes::Hook_ClientConnect, false );
	SH_ADD_HOOK_MEMFUNC(IServerGameClients, ClientCommand, g_pSource2GameClients, this, &CS2Fixes::Hook_ClientCommand, false);

	META_CONPRINTF( "All hooks started!\n" );

	ConVar_Register( FCVAR_RELEASE | FCVAR_CLIENT_CAN_EXECUTE | FCVAR_GAMEDLL );
	
	Init();

	return true;
}

void FlushAllDetours()
{
	FOR_EACH_VEC(g_vecDetours, i)
	{
		ConMsg("Nuking detour %s", g_vecDetours[i]->GetName());
		g_vecDetours[i]->FreeDetour();
	}

	g_vecDetours.RemoveAll();
}

bool CS2Fixes::Unload(char *error, size_t maxlen)
{
	SH_REMOVE_HOOK_MEMFUNC(IServerGameDLL, GameFrame, g_pSource2Server, this, &CS2Fixes::Hook_GameFrame, true);
	SH_REMOVE_HOOK_MEMFUNC(IServerGameClients, ClientActive, g_pSource2GameClients, this, &CS2Fixes::Hook_ClientActive, true);
	SH_REMOVE_HOOK_MEMFUNC(IServerGameClients, ClientDisconnect, g_pSource2GameClients, this, &CS2Fixes::Hook_ClientDisconnect, true);
	SH_REMOVE_HOOK_MEMFUNC(IServerGameClients, ClientPutInServer, g_pSource2GameClients, this, &CS2Fixes::Hook_ClientPutInServer, true);
	SH_REMOVE_HOOK_MEMFUNC(IServerGameClients, ClientSettingsChanged, g_pSource2GameClients, this, &CS2Fixes::Hook_ClientSettingsChanged, false);
	SH_REMOVE_HOOK_MEMFUNC(IServerGameClients, OnClientConnected, g_pSource2GameClients, this, &CS2Fixes::Hook_OnClientConnected, false);
	SH_REMOVE_HOOK_MEMFUNC(IServerGameClients, ClientConnect, g_pSource2GameClients, this, &CS2Fixes::Hook_ClientConnect, false);
	SH_REMOVE_HOOK_MEMFUNC(IServerGameClients, ClientCommand, g_pSource2GameClients, this, &CS2Fixes::Hook_ClientCommand, false);

	FlushAllDetours();

	return true;
}

void CS2Fixes::AllPluginsLoaded()
{
	/* This is where we'd do stuff that relies on the mod or other plugins 
	 * being initialized (for example, cvars added and events registered).
	 */
}

void CS2Fixes::Hook_ClientActive( CPlayerSlot slot, bool bLoadGame, const char *pszName, uint64 xuid )
{
	META_CONPRINTF( "Hook_ClientActive(%d, %d, \"%s\", %d)\n", slot, bLoadGame, pszName, xuid );
}

void CS2Fixes::Hook_ClientCommand( CPlayerSlot slot, const CCommand &args )
{
	META_CONPRINTF( "Hook_ClientCommand(%d, \"%s\")\n", slot, args.GetCommandString() );
}

void CS2Fixes::Hook_ClientSettingsChanged( CPlayerSlot slot )
{
	META_CONPRINTF( "Hook_ClientSettingsChanged(%d)\n", slot );
}

void CS2Fixes::Hook_OnClientConnected( CPlayerSlot slot, const char *pszName, uint64 xuid, const char *pszNetworkID, const char *pszAddress, bool bFakePlayer )
{
	META_CONPRINTF( "Hook_OnClientConnected(%d, \"%s\", %d, \"%s\", \"%s\", %d)\n", slot, pszName, xuid, pszNetworkID, pszAddress, bFakePlayer );
}

bool CS2Fixes::Hook_ClientConnect( CPlayerSlot slot, const char *pszName, uint64 xuid, const char *pszNetworkID, bool unk1, CBufferString *pRejectReason )
{
	META_CONPRINTF( "Hook_ClientConnect(%d, \"%s\", %d, \"%s\", %d, \"%s\")\n", slot, pszName, xuid, pszNetworkID, unk1, pRejectReason->ToGrowable()->Get() );

	RETURN_META_VALUE(MRES_IGNORED, true);
}

void CS2Fixes::Hook_ClientPutInServer( CPlayerSlot slot, char const *pszName, int type, uint64 xuid )
{
	META_CONPRINTF( "Hook_ClientPutInServer(%d, \"%s\", %d, %d, %d)\n", slot, pszName, type, xuid );
}

void CS2Fixes::Hook_ClientDisconnect( CPlayerSlot slot, int reason, const char *pszName, uint64 xuid, const char *pszNetworkID )
{
	META_CONPRINTF( "Hook_ClientDisconnect(%d, %d, \"%s\", %d, \"%s\")\n", slot, reason, pszName, xuid, pszNetworkID );
}

void CS2Fixes::Hook_GameFrame( bool simulating, bool bFirstTick, bool bLastTick )
{
	/**
	 * simulating:
	 * ***********
	 * true  | game is ticking
	 * false | game is not ticking
	 */
}

// Potentially might not work
void CS2Fixes::OnLevelInit( char const *pMapName,
									 char const *pMapEntities,
									 char const *pOldLevel,
									 char const *pLandmarkName,
									 bool loadGame,
									 bool background )
{
	META_CONPRINTF("OnLevelInit(%s)\n", pMapName);
}

// Potentially might not work
void CS2Fixes::OnLevelShutdown()
{
	META_CONPRINTF("OnLevelShutdown()\n");
}

bool CS2Fixes::Pause(char *error, size_t maxlen)
{
	return true;
}

bool CS2Fixes::Unpause(char *error, size_t maxlen)
{
	return true;
}

const char *CS2Fixes::GetLicense()
{
	return "MIT License";
}

const char *CS2Fixes::GetVersion()
{
	return "1.0.0.0";
}

const char *CS2Fixes::GetDate()
{
	return __DATE__;
}

const char *CS2Fixes::GetLogTag()
{
	return "CS2Fixes";
}

const char *CS2Fixes::GetAuthor()
{
	return "xen & poggu";
}

const char *CS2Fixes::GetDescription()
{
	return "A bunch of experiments thrown together into one big mess of a plugin.";
}

const char *CS2Fixes::GetName()
{
	return "CS2Fixes";
}

const char *CS2Fixes::GetURL()
{
	return "https://github.com/Source2ZE";
}
