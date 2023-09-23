#include "common.h"
#include "detour.h"
#include "dllpatch.h"
#include "icvar.h"

#include "tier0/memdbgon.h"

CDLLPatch g_CommonPatches[] =
{
	// Movement Unlocker
	// Refer to vauff's pin in #scripting
	CDLLPatch(
		GAMEBIN"server.dll",
		(byte*)"\x76\x2A\xF2\x0F\x10\x57\x3C\xF3\x0F\x10\x4F\x44\x0F\x28\xC2",
		"x?xxxx?xxxxxxxx",
		(byte*)"\xEB",
		"ServerMovementUnlock",
		1
	),
	// Force the server to reject players at a given number which is configured below with set_max_players
	// Look for "NETWORK_DISCONNECT_REJECT_SERVERFULL to %s"
	CDLLPatch(
		ROOTBIN"engine2.dll",
		(byte*)"\x0F\x84\x00\x00\x00\x00\x49\x8B\xCF\xE8\x00\x00\x00\x00\x44\x8B\x54\x24\x00",
		"xx????xxxx????xxxx?",
		(byte*)"\x90\x90\x90\x90\x90\x90",
		"EngineMaxPlayers1",
		1
	),
	CDLLPatch(
		ROOTBIN"engine2.dll",
		(byte*)"\x41\x8B\xB7\x00\x00\x00\x00\x49\x8B\xCF\xE8\x00\x00\x00\x00",
		"xxx????xxxx????",
		(byte*)"\xBE\x1C\x00\x00\x00\x90\x90",
		"EngineMaxPlayers2",
		1
	),
	// Re-enable vscript
	// Refer to tilgep's pin in #scripting
	CDLLPatch(
		ROOTBIN"vscript.dll",
		(byte*)"\xBE\x01\x00\x00\x00\x2B\xD6\x74\x61\x3B\xD6",
		"x????xxxxxx",
		(byte*)"\xBE\x02",
		"VScriptEnable",
		1
	),
	// Remove "Interpenetrating entities! (%s and %s)" warning spam
	//{
	//	GAMEBIN"server.dll",
	//	(byte*)"\x0F\x85\x00\x00\x00\x00\x48\x8B\x46\x10\x48\x8D\x15\x00\x00\x00\x00",
	//	"xx????xxxxxxx????",
	//	"\x90\xE9",
	//	"ServerInterpenetratingWarning",
	//	1
	//},
};

CDLLPatch g_ClientPatches[] =
{
	// Client-side movement unlocker
	// Identical to the server since it's shared code
	CDLLPatch(
		GAMEBIN"client.dll",
		(byte*)"\x76\x2A\xF2\x0F\x10\x57\x3C\xF3\x0F\x10\x4F\x44\x0F\x28\xC2",
		"x?xxxx?xxxxxxxx",
		(byte*)"\xEB",
		"ClientMovementUnlock",
		1
	),
	// Remove client steam check
	// Look for "Failed to connect to local..."
	CDLLPatch(
		GAMEBIN"client.dll",
		(byte*)"\x75\x73\xFF\x15\x00\x00\x00\x00\x48\x8D\x15",
		"xxxx????xxx",
		(byte*)"\xEB",
		"ClientSteamCheck",
		1
	),
};

CDLLPatch g_ToolsPatches[] =
{
	// Remove some -nocustomermachine checks without needing -nocustomermachine itself as it can break stuff, mainly to enable device selection in compiles
	// Find "Noise removal", there should be 3 customermachine checks
	CDLLPatch(
		ROOTBIN"tools/hammer.dll",
		(byte*)"\xFF\x15\x00\x00\x00\x00\x84\xC0\x0F\x85\x00\x00\x00\x00\xB9",
		"xx????xxxx????x",
		(byte*)"\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90",
		"HammerNoCustomerMachine",
		4
	),
};

extern HMODULE g_hTier0;

void Detour_Log()
{
	return;
}

bool __fastcall Detour_IsChannelEnabled(LoggingChannelID_t channelID, LoggingSeverity_t severity)
{
	return false;
}

CDetour g_LoggingDetours[] =
{
	CDetour( ROOTBIN"tier0.dll", Detour_Log, "Msg" ),
	CDetour( ROOTBIN"tier0.dll", Detour_Log, "?ConMsg@@YAXPEBDZZ" ),
	CDetour( ROOTBIN"tier0.dll", Detour_Log, "?ConColorMsg@@YAXAEBVColor@@PEBDZZ" ),
	CDetour( ROOTBIN"tier0.dll", Detour_Log, "ConDMsg" ),
	CDetour( ROOTBIN"tier0.dll", Detour_Log, "DevMsg" ),
	CDetour( ROOTBIN"tier0.dll", Detour_Log, "Warning" ),
	CDetour( ROOTBIN"tier0.dll", Detour_Log, "DevWarning" ),
	CDetour( ROOTBIN"tier0.dll", Detour_Log, "?DevWarning@@YAXPEBDZZ" ),
	CDetour( ROOTBIN"tier0.dll", Detour_Log, "LoggingSystem_Log" ),
	CDetour( ROOTBIN"tier0.dll", Detour_Log, "LoggingSystem_LogDirect" ),
	CDetour( ROOTBIN"tier0.dll", Detour_Log, "LoggingSystem_LogAssert" ),
	CDetour( ROOTBIN"tier0.dll", Detour_IsChannelEnabled, "LoggingSystem_IsChannelEnabled" ),
};

void InitLoggingDetours()
{
	for (int i = 0; i < sizeof(g_LoggingDetours) / sizeof(*g_LoggingDetours); i++)
		g_LoggingDetours[i].CreateDetour();
}

void ToggleLogs()
{
	static bool bBlock = false;

	if (!bBlock)
	{
		for (int i = 0; i < sizeof(g_LoggingDetours) / sizeof(*g_LoggingDetours); i++)
			g_LoggingDetours[i].EnableDetour();
	}
	else
	{
		for (int i = 0; i < sizeof(g_LoggingDetours) / sizeof(*g_LoggingDetours); i++)
			g_LoggingDetours[i].DisableDetour();
	}

	bBlock = !bBlock;
}

byte *g_pMaxPlayers = nullptr;

void SetMaxPlayers(byte iMaxPlayers)
{
	clamp(iMaxPlayers, 1, 64);

	WriteProcessMemory(GetCurrentProcess(), g_pMaxPlayers, &iMaxPlayers, 1, nullptr);
}

typedef void(__fastcall *Host_Say)(byte *, CCommand *, bool, uint, int64);
void __fastcall Detour_Host_Say(byte *pEdict, CCommand *args, bool teamonly, uint unk1, int64 unk2);

// look for string "\"Console<0>\" say \"%s\"\n"
CDetour Host_Say_Detour(
	GAMEBIN "server.dll",
	Detour_Host_Say,
	"Host_Say",
	(uint8 *)"\x44\x89\x4C\x24\x00\x44\x88\x44\x24\x00\x55\x53\x56\x57\x41\x54\x41\x55",
	"xxxx?xxxx?xxxxxxxx");

void __fastcall Detour_Host_Say(byte *pEdict, CCommand *args, bool teamonly, uint unk1, int64 unk2)
{
	if (args->ArgC() > 1 && !pEdict)
	{
		char buf[256];
		V_snprintf(buf, sizeof(buf), "%s %s", "say \x02", args->ArgS()); // 0x02 is red, maybe add a command to configure this later

		args->Tokenize(buf);
	}

	((Host_Say)Host_Say_Detour.GetOriginalFunction())(pEdict, args, teamonly, unk1, unk2);
}

void InitPatches()
{
	for (int i = 0; i < sizeof(g_CommonPatches) / sizeof(*g_CommonPatches); i++)
		g_CommonPatches[i].PerformPatch();

	// Same location as the above 2 patches for maxplayers
	CDLLPatch MaxPlayerPatch(
		ROOTBIN "engine2.dll",
		(byte *)"\x41\x3B\x87\x00\x00\x00\x00\x0F\x8E\x00\x00\x00\x00\x8B\x0D\x00\x00\x00\x00",
		"xxx????xx????xx????",
		(byte *)"\x83\xF8\x40\x90\x90\x90\x90",
		"EngineMaxPlayers3",
		1);

	MaxPlayerPatch.PerformPatch();

	g_pMaxPlayers = (byte *)MaxPlayerPatch.GetPatchAddress() + 2;

	// Dedicated servers don't load client.dll
	if (!CommandLine()->HasParm("-dedicated"))
	{
		for (int i = 0; i < sizeof(g_ClientPatches) / sizeof(*g_ClientPatches); i++)
			g_ClientPatches[i].PerformPatch();
	}

	// None of the tools are loaded without, well, -tools
	if (CommandLine()->HasParm("-tools"))
	{
		for (int i = 0; i < sizeof(g_ToolsPatches) / sizeof(*g_ToolsPatches); i++)
			g_ToolsPatches[i].PerformPatch();
	}

	Host_Say_Detour.CreateDetour();
	Host_Say_Detour.EnableDetour();
}
