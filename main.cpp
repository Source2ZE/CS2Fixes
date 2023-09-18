#include "appframework/IAppSystem.h"
#include "common.h"
#include "icvar.h"
#include "interface.h"
#include <MinHook.h>
#include <Psapi.h>

#include "tier0/memdbgon.h"

ICvar *g_pCVar = nullptr;

HMODULE g_hTier0 = nullptr;
MODULEINFO g_Tier0Info;

void Message(const char *msg, ...)
{
	va_list args;
	va_start(args, msg);

	char buf[1024] = {};
	V_vsnprintf(buf, sizeof(buf) - 1, msg, args);

	ConColorMsg(Color(255, 0, 255, 255), buf);

#ifdef _DEBUG
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
		Warning(buf);
	else
		MessageBoxA(nullptr, buf, "Warning", 0);

	va_end(args);
}

void Init()
{
	static bool attached = false;

	if (!attached)
		attached = true;
	else
		return;

#ifdef _DEBUG
	AllocConsole();
	freopen("CONOUT$", "w", stdout);
#endif

	HANDLE hProcess = GetCurrentProcess();

	// Find tier0
	g_hTier0 = LoadLibraryA("tier0");
	if (!g_hTier0)
	{
		Plat_FatalErrorFunc("Failed to get tier0.dll's address");
		return;
	}

	GetModuleInformation(GetCurrentProcess(), g_hTier0, &g_Tier0Info, sizeof(g_Tier0Info));

	MH_Initialize();

#ifdef HOOK_CONVARS
	HookConVars();
#endif
#ifdef HOOK_CONCOMMANDS
	HookConCommands();
#endif
	InitPatches();
	InitLoggingDetours();
}

#ifdef USE_TICKRATE
void *g_pfnGetTickInterval = nullptr;

float GetTickIntervalHook()
{
	return 1.0f / CommandLine()->ParmValue("-tickrate", 64.0f);
}
#endif

typedef bool (*AppSystemConnectFn)(IAppSystem *appSystem, CreateInterfaceFn factory);
static AppSystemConnectFn g_pfnServerConfigConnect = nullptr;

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
	ToggleLogs();
}

CON_COMMAND_F(set_max_players, "Set max players through a patch", FCVAR_NONE)
{
	if (args.ArgC() < 2)
	{
		Msg("Usage: set_max_players <maxplayers>\n");
		return;
	}

	SetMaxPlayers(atoi(args.Arg(1)));
}

bool Connect(IAppSystem *appSystem, CreateInterfaceFn factory)
{
	g_pCVar = (ICvar *)factory(CVAR_INTERFACE_VERSION, nullptr);

	g_pCVar->RegisterConCommand(&unlock_cvars_command);
	g_pCVar->RegisterConCommand(&unlock_commands_command);
	g_pCVar->RegisterConCommand(&toggle_logs_command);
	g_pCVar->RegisterConCommand(&set_max_players_command);

	return g_pfnServerConfigConnect(appSystem, factory);
}

typedef void *CreateInterface_t(const char *name, int *ret);
CreateInterface_t *g_pfnCreateInterface = nullptr;

DLL_EXPORT void *CreateInterface(const char *pName, int *pReturnCode)
{
	Init();

#ifdef _DEBUG
	printf("CreateInterface: %s %i\n", pName, *pReturnCode);
#endif

	if (!g_pfnCreateInterface)
	{
		char szServerDLLPath[MAX_PATH];
		V_strncpy(szServerDLLPath, Plat_GetGameDirectory(), MAX_PATH);
		V_strcat(szServerDLLPath, GAMEBIN "server.dll", MAX_PATH);

		g_pfnCreateInterface = (CreateInterface_t *)Plat_GetProcAddress(szServerDLLPath, "CreateInterface");
	}

	void *pInterface = g_pfnCreateInterface(pName, pReturnCode);

	if (!V_stricmp(pName, SOURCE2SERVERCONFIG_INTERFACE_VERSION))
	{
		void **vtable = *(void ***)pInterface;

		MH_CreateHook(vtable[0], Connect, (LPVOID *)&g_pfnServerConfigConnect);
		MH_EnableHook(vtable[0]);

#ifdef USE_TICKRATE
		MH_CreateHook(vtable[13], GetTickIntervalHook, &g_pfnGetTickInterval);
		MH_EnableHook(vtable[13]);
#endif
	}

	return pInterface;
}
