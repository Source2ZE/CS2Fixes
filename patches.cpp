#include "common.h"
#include "detour.h"
#include "dllpatch.h"
#include "icvar.h"
#include "irecipientfilter.h"
#include "cs2_sdk/interfaces/interfaces.h"
#include "cs2_sdk/entity/ccsplayercontroller.h"
#include "cs2_sdk/entity/ccsplayerpawn.h"
#include "addresses.h"

#include "tier0/memdbgon.h"

CDLLPatch g_CommonPatches[] =
{
	// Movement Unlocker
	// Refer to vauff's pin in #scripting
	CDLLPatch(
		GAMEBIN"server.dll",
		(byte*)"\x76\x2A\xF2\x0F\x10\x57\x2A\xF3\x0F\x10\x4F\x44\x0F\x28\xC2",
		(byte*)"\xEB",
		"ServerMovementUnlock",
		1
	),
	// Force the server to reject players at a given number which is configured below with set_max_players
	// Look for "NETWORK_DISCONNECT_REJECT_SERVERFULL to %s"
	CDLLPatch(
		ROOTBIN"engine2.dll",
		(byte*)"\x0F\x84\x2A\x2A\x2A\x2A\x49\x8B\xCF\xE8\x2A\x2A\x2A\x2A\x44\x8B\x54\x24\x2A",
		(byte*)"\x90\x90\x90\x90\x90\x90",
		"EngineMaxPlayers1",
		1
	),
	CDLLPatch(
		ROOTBIN"engine2.dll",
		(byte*)"\x41\x8B\xB7\x2A\x2A\x2A\x2A\x49\x8B\xCF\xE8\x2A\x2A\x2A\x2A",
		(byte*)"\xBE\x1C\x00\x00\x00\x90\x90",
		"EngineMaxPlayers2",
		1
	),
	// Re-enable vscript
	// Refer to tilgep's pin in #scripting
	CDLLPatch(
		ROOTBIN"vscript.dll",
		(byte*)"\xBE\x01\x2A\x2A\x2A\x2B\xD6\x74\x61\x3B\xD6",
		(byte*)"\xBE\x02",
		"VScriptEnable",
		1
	),
	// Remove "Interpenetrating entities! (%s and %s)" warning spam
	//{
	//	GAMEBIN"server.dll",
	//	(byte*)"\x0F\x85\x00\x00\x00\x00\x48\x8B\x46\x10\x48\x8D\x15\x00\x00\x00\x00",
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
		(byte*)"\x76\x2A\xF2\x0F\x10\x57\x2A\xF3\x0F\x10\x4F\x44\x0F\x28\xC2",
		(byte*)"\xEB",
		"ClientMovementUnlock",
		1
	),
	// Remove client steam check
	// Look for "Failed to connect to local..."
	CDLLPatch(
		GAMEBIN"client.dll",
		(byte*)"\x75\x73\xFF\x15\x2A\x2A\x2A\x2A\x48\x8D\x15",
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
		(byte*)"\xFF\x15\x2A\x2A\x2A\x2A\x84\xC0\x0F\x85\x2A\x2A\x2A\x2A\xB9",
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

typedef void(__fastcall *UTIL_SayTextFilter)(IRecipientFilter &, const char *, void *, uint64);
void __fastcall Detour_UTIL_SayTextFilter(IRecipientFilter &, const char *, void *, uint64);

typedef void(__fastcall *UTIL_SayText2Filter)(IRecipientFilter &, CBaseEntity *, uint64, const char *, const char *, const char *, const char *, const char *);
void __fastcall Detour_UTIL_SayText2Filter(IRecipientFilter &, CBaseEntity *, uint64, const char *, const char *, const char *, const char *, const char *);

typedef void(__fastcall *Host_Say)(CCSPlayerController*, CCommand *, bool, uint, int64);
void __fastcall Detour_Host_Say(CCSPlayerController*pController, CCommand *args, bool teamonly, uint unk1, int64 unk2);

// both of these are called from Host_Say
CDetour UTIL_SayTextFilter_Detour(
	GAMEBIN "server.dll",
	Detour_UTIL_SayTextFilter,
	"Host_Say",
	(uint8 *)"\x48\x89\x5C\x24\x2A\x55\x56\x57\x48\x8D\x6C\x24\x2A\x48\x81\xEC\x2A\x2A\x2A\x2A\x49\x8B\xD8");

CDetour UTIL_SayText2Filter_Detour(
	GAMEBIN "server.dll",
	Detour_UTIL_SayText2Filter,
	"Host_Say",
	(uint8 *)"\x48\x89\x5C\x24\x2A\x55\x56\x57\x48\x8D\x6C\x24\x2A\x48\x81\xEC\x2A\x2A\x2A\x2A\x41\x0F\xB6\xF8");

// look for string "\"Console<0>\" say \"%s\"\n"
CDetour Host_Say_Detour(
	GAMEBIN "server.dll",
	Detour_Host_Say,
	"Host_Say",
	(uint8 *)"\x44\x89\x4C\x24\x2A\x44\x88\x44\x24\x2A\x55\x53\x56\x57\x41\x54\x41\x55");

void __fastcall Detour_UTIL_SayTextFilter(IRecipientFilter &filter, const char *pText, void *pPlayer, uint64 eMessageType)
{
	if (pPlayer)
		((UTIL_SayTextFilter)UTIL_SayTextFilter_Detour.GetOriginalFunction())(filter, pText, pPlayer, eMessageType);

	char buf[256];
	V_snprintf(buf, sizeof(buf), "%s%s", " \7CONSOLE:\4", pText + sizeof("Console:"));

	((UTIL_SayTextFilter)UTIL_SayTextFilter_Detour.GetOriginalFunction())(filter, buf, pPlayer, eMessageType);
}

void __fastcall Detour_UTIL_SayText2Filter(IRecipientFilter &filter, CBaseEntity *pEntity, uint64 eMessageType, const char *msg_name, const char *param1, const char *param2, const char *param3, const char *param4)
{
	((UTIL_SayText2Filter)UTIL_SayText2Filter_Detour.GetOriginalFunction())(filter, pEntity, eMessageType, msg_name, param1, param2, param3, param4);
}

struct WeaponMapEntry_t
{
	const char* command;
	const char *szWeaponName;
	int iPrice;
};


WeaponMapEntry_t WeaponMap[] = {
	{"bizon", "weapon_bizon", 1400},
	{"mac10", "weapon_mac10", 1400},
	{"mp7", "weapon_mp7", 1700},
	{"mp9", "weapon_mp9", 1250},
	{"p90", "weapon_p90", 2350},
	{"ump45", "weapon_ump45", 1700},
	{"ak47", "weapon_ak47", 2500},
	{"aug", "weapon_aug", 3500},
	{"famas", "weapon_famas", 2250},
	{"galilar", "weapon_galilar", 2000},
	{"m4a4", "weapon_m4a4", 3100},
	{"m4a1_silencer", "weapon_m4a1_silencer", 3100},
	{"m4a1", "weapon_m4a1_silencer", 3100},
	{"a1", "weapon_m4a1_silencer", 3100},
	{"sg556", "weapon_sg556", 3500},
	{"awp", "weapon_awp", 4750},
	{"g3sg1", "weapon_g3sg1", 5000},
	{"scar20", "weapon_scar20", 5000},
	{"ssg08", "weapon_ssg08", 2500},
	{"mag7", "weapon_mag7", 2000},
	{"nova", "weapon_nova", 1500},
	{"sawedoff", "weapon_sawedoff", 1500},
	{"xm1014", "weapon_xm1014", 3000},
	{"m249", "weapon_m249", 5750},
	{"negev", "weapon_negev", 5750},
	{"deagle", "weapon_deagle", 700},
	{"elite", "weapon_elite", 800},
	{"fiveseven", "weapon_fiveseven", 500},
	{"glock", "weapon_glock", 200},
	{"hkp2000", "weapon_hkp2000", 200},
	{"p250", "weapon_p250", 300},
	{"tec9", "weapon_tec9", 500},
	{"usp_silencer", "weapon_usp_silencer", 200},
	{"cz75a", "weapon_cz75a", 500},
	{"revolver", "weapon_revolver", 600},
	{"kevlar", "item_kevlar", 600},
	{"he", "weapon_hegrenade", 300},
	{"molotov", "weapon_hegrenade", 850},
};

void SendConsoleChat(const char* msg, ...)
{
	va_list args;
	va_start(args, msg);

	char buf[256] = {};
	V_vsnprintf(buf, sizeof(buf), msg, args);

	va_end(args);

	CCommand newArgs;
	newArgs.Tokenize(buf);
	((Host_Say)Host_Say_Detour.GetOriginalFunction())(nullptr, &newArgs, false, 0, 0);
}

bool ParseChatCommand(char *pCommand, CCSPlayerController* pController)
{
	char *token, *pos;

	pCommand = strdup(pCommand);
	token = strtok_s(pCommand + 1, " ", &pos);

	if (!V_stricmp(token, "map"))
	{
		token = strtok_s(nullptr, " ", &pos);

		SendConsoleChat("[CS2Fixes] Changing map to %s", token);
		Message("[CS2Fixes] Changing map to %s\n", token);
	}

	for (int i = 0; i < sizeof(WeaponMap) / sizeof(*WeaponMap); i++)
	{
		auto weaponEntry = WeaponMap[i];

		if(!V_stricmp(token, weaponEntry.command))
		{
			auto pItemServices = pController->m_hPawn().Get<CCSPlayerPawn>()->m_pItemServices.Get();
			ConMsg("debil %llx, %s, %llx", pItemServices, weaponEntry.szWeaponName, pController->m_hPawn().Get());
			//addresses::GiveNamedItem((uintptr_t)pItemServices, weaponEntry.szWeaponName, 0, 0, 0, 0);
			/*
			auto money = pController->m_pInGameMoneyServices()->m_iAccount.Get();
			if (money >= weaponEntry.iPrice)
			{
				pController->m_pInGameMoneyServices()->m_iAccount.Set(money - weaponEntry.iPrice);
				auto pItemServices = pController->m_hPawn().Get<CCSPlayerPawn>();
				addresses::GiveNamedItem((uintptr_t)pController, weaponEntry.szWeaponName, 0, 0, 0, 0);
			}*/
		
			break;
		}
	}

	free(pCommand);

	return *pCommand == '/' ? false : true;
}

void __fastcall Detour_Host_Say(CCSPlayerController *pController, CCommand *args, bool teamonly, uint unk1, int64 unk2)
{
	char* pMessage = (char*)args->ArgS();

	// Messages sent by players are enclosed within additional quotes, remove them
	if (pController)
	{
		*pMessage++ = 0;
		pMessage[V_strlen(pMessage) - 1] = 0;
	}

	if ((*pMessage == '!' || *pMessage == '/') && !ParseChatCommand(pMessage, pController))
		return;


	/*
		auto money = pController->m_pInGameMoneyServices()->m_iAccount.Get();
		Message("host_say: %d\n", money);
		pController->m_pInGameMoneyServices()->m_iAccount.Set(money - 10);*/
	((Host_Say)Host_Say_Detour.GetOriginalFunction())(pController, args, teamonly, unk1, unk2);
}

void InitPatches()
{
	for (int i = 0; i < sizeof(g_CommonPatches) / sizeof(*g_CommonPatches); i++)
		g_CommonPatches[i].PerformPatch();

	// Same location as the above 2 patches for maxplayers
	CDLLPatch MaxPlayerPatch(
		ROOTBIN "engine2.dll",
		(byte *)"\x41\x3B\x87\x2A\x2A\x2A\x2A\x0F\x8E\x2A\x2A\x2A\x2A\x8B\x0D\x2A\x2A\x2A\x2A",
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

	UTIL_SayTextFilter_Detour.CreateDetour();
	UTIL_SayTextFilter_Detour.EnableDetour();
	
	//UTIL_SayText2Filter_Detour.CreateDetour();
	//UTIL_SayText2Filter_Detour.EnableDetour();

	Host_Say_Detour.CreateDetour();
	Host_Say_Detour.EnableDetour();
}
