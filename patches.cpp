#include "common.h"
#include "detour.h"
#include "dllpatch.h"
#include "icvar.h"
#include "irecipientfilter.h"
#include "interfaces/cs2_interfaces.h"
#include "entity/ccsplayercontroller.h"
#include "entity/ccsplayerpawn.h"
#include "entity/cbasemodelentity.h"
#include "addresses.h"

#include "tier0/memdbgon.h"

CDLLPatch g_CommonPatches[] =
{
	// Movement Unlocker
	// Refer to vauff's pin in #scripting
	CDLLPatch(
		GAMEBIN"server.dll",
		(byte*)"\x76\x2A\xF2\x0F\x10\x57\x2A\xF3\x0F\x10\x2A\x2A\x0F\x28\xCA\xF3\x0F\x59\xC0",
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
};

CDLLPatch g_ClientPatches[] =
{
	// Client-side movement unlocker
	// Identical to the server since it's shared code
	CDLLPatch(
		GAMEBIN"client.dll",
		(byte*)"\x76\x2A\xF2\x0F\x10\x57\x2A\xF3\x0F\x10\x2A\x2A\x0F\x28\xCA\xF3\x0F\x59\xC0",
		(byte*)"\xEB",
		"ClientMovementUnlock",
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

void Detour_Log()
{
	return;
}

bool __fastcall Detour_IsChannelEnabled(LoggingChannelID_t channelID, LoggingSeverity_t severity)
{
	return false;
}

CDetour<void> g_LoggingDetours[] =
{
	CDetour<void>( ROOTBIN"tier0.dll", Detour_Log, "Msg" ),
	CDetour<void>( ROOTBIN"tier0.dll", Detour_Log, "?ConMsg@@YAXPEBDZZ" ),
	CDetour<void>( ROOTBIN"tier0.dll", Detour_Log, "?ConColorMsg@@YAXAEBVColor@@PEBDZZ" ),
	CDetour<void>( ROOTBIN"tier0.dll", Detour_Log, "ConDMsg" ),
	CDetour<void>( ROOTBIN"tier0.dll", Detour_Log, "DevMsg" ),
	CDetour<void>( ROOTBIN"tier0.dll", Detour_Log, "Warning" ),
	CDetour<void>( ROOTBIN"tier0.dll", Detour_Log, "DevWarning" ),
	CDetour<void>( ROOTBIN"tier0.dll", Detour_Log, "?DevWarning@@YAXPEBDZZ" ),
	CDetour<void>( ROOTBIN"tier0.dll", Detour_Log, "LoggingSystem_Log" ),
	CDetour<void>( ROOTBIN"tier0.dll", Detour_Log, "LoggingSystem_LogDirect" ),
	CDetour<void>( ROOTBIN"tier0.dll", Detour_Log, "LoggingSystem_LogAssert" ),
	CDetour<void>( ROOTBIN"tier0.dll", Detour_IsChannelEnabled, "LoggingSystem_IsChannelEnabled" ),
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

void __fastcall Detour_UTIL_SayTextFilter(IRecipientFilter &, const char *, CBasePlayerController *, uint64);
void __fastcall Detour_UTIL_SayText2Filter(IRecipientFilter &, CBasePlayerController *, uint64, const char *, const char *, const char *, const char *, const char *);
void __fastcall Detour_Host_Say(CCSPlayerController *, CCommand *, bool, int, const char *);

// both of these are called from Host_Say
DECLARE_DETOUR(
	UTIL_SayTextFilter,
	Detour_UTIL_SayTextFilter,
	GAMEBIN "server.dll",
	"\x48\x89\x5C\x24\x2A\x55\x56\x57\x48\x8D\x6C\x24\x2A\x48\x81\xEC\x2A\x2A\x2A\x2A\x49\x8B\xD8");
DECLARE_DETOUR(
	UTIL_SayText2Filter,
	Detour_UTIL_SayText2Filter,
	GAMEBIN "server.dll",
	"\x48\x89\x5C\x24\x2A\x55\x56\x57\x48\x8D\x6C\x24\x2A\x48\x81\xEC\x2A\x2A\x2A\x2A\x41\x0F\xB6\xF8");

// look for string "\"Console<0>\" say \"%s\"\n"
DECLARE_DETOUR(Host_Say,
	Detour_Host_Say,
	GAMEBIN "server.dll",
	"\x44\x89\x4C\x24\x2A\x44\x88\x44\x24\x2A\x55\x53\x56\x57\x41\x54\x41\x55");

void __fastcall Detour_UTIL_SayTextFilter(IRecipientFilter &filter, const char *pText, CBasePlayerController *pPlayer, uint64 eMessageType)
{
#ifdef _DEBUG
	CEntityIndex slot(-1);
	int entindex = filter.GetRecipientIndex(&slot, 0)->Get();
	CBasePlayerController *target = (CBasePlayerController *)CGameEntitySystem::GetInstance()->GetBaseEntity(entindex);

	if (target)
		Message("[CS2Fixes] Chat from %s to %s: %s\n", pPlayer ? &pPlayer->m_iszPlayerName() : "console", target->m_iszPlayerName(), pText);
#endif

	if (pPlayer)
		return UTIL_SayTextFilter(filter, pText, pPlayer, eMessageType);

	char buf[256];
	V_snprintf(buf, sizeof(buf), "%s%s", " \7CONSOLE:\4", pText + sizeof("Console: "));

	UTIL_SayTextFilter(filter, buf, pPlayer, eMessageType);
}

void __fastcall Detour_UTIL_SayText2Filter(
	IRecipientFilter &filter, 
	CBasePlayerController *pEntity, 
	uint64 eMessageType, 
	const char *msg_name,
	const char *param1, 
	const char *param2, 
	const char *param3, 
	const char *param4)
{
#ifdef _DEBUG
	CEntityIndex slot(-1);
	int entindex = filter.GetRecipientIndex(&slot, 0)->Get() + 1;
	CBasePlayerController *target = (CBasePlayerController *)CGameEntitySystem::GetInstance()->GetBaseEntity(entindex);

	if (target)	
		Message("[CS2Fixes] Chat from %s to %s: %s\n", param1, &target->m_iszPlayerName(), param2);
#endif

	UTIL_SayText2Filter(filter, pEntity, eMessageType, msg_name, param1, param2, param3, param4);
}

WeaponMapEntry_t WeaponMap[] = {
	{"bizon",		  "weapon_bizon",		  1400},
	{"mac10",		  "weapon_mac10",		  1400},
	{"mp7",			"weapon_mp7",			  1700},
	{"mp9",			"weapon_mp9",			  1250},
	{"p90",			"weapon_p90",			  2350},
	{"ump45",		  "weapon_ump45",		  1700},
	{"ak47",			 "weapon_ak47",			2500},
	{"aug",			"weapon_aug",			  3500},
	{"famas",		  "weapon_famas",		  2250},
	{"galilar",		"weapon_galilar",		  2000},
	{"m4a4",			 "weapon_m4a4",			3100},
	{"m4a1_silencer", "weapon_m4a1_silencer", 3100},
	{"m4a1",			 "weapon_m4a1_silencer", 3100},
	{"a1",			"weapon_m4a1_silencer", 3100},
	{"sg556",		  "weapon_sg556",		  3500},
	{"awp",			"weapon_awp",			  4750},
	{"g3sg1",		  "weapon_g3sg1",		  5000},
	{"scar20",		   "weapon_scar20",		5000},
	{"ssg08",		  "weapon_ssg08",		  2500},
	{"mag7",			 "weapon_mag7",			2000},
	{"nova",			 "weapon_nova",			1500},
	{"sawedoff",		 "weapon_sawedoff",		1500},
	{"xm1014",		   "weapon_xm1014",		3000},
	{"m249",			 "weapon_m249",			5750},
	{"negev",		  "weapon_negev",		  5750},
	{"deagle",		   "weapon_deagle",		700 },
	{"elite",		  "weapon_elite",		  800 },
	{"fiveseven",	  "weapon_fiveseven",	  500 },
	{"glock",		  "weapon_glock",		  200 },
	{"hkp2000",		"weapon_hkp2000",		  200 },
	{"p250",			 "weapon_p250",			300 },
	{"tec9",			 "weapon_tec9",			500 },
	{"usp_silencer",	 "weapon_usp_silencer",	200 },
	{"cz75a",		  "weapon_cz75a",		  500 },
	{"revolver",		 "weapon_revolver",		600 },
	{"kevlar",		   "item_kevlar",		  600 },
	{"he",			"weapon_hegrenade",	   300 },
	{"molotov",		"weapon_hegrenade",		850 },
};

void SendConsoleChat(const char *msg, ...)
{
	va_list args;
	va_start(args, msg);

	char buf[256];
	V_vsnprintf(buf, sizeof(buf), msg, args);

	va_end(args);

	CCommand newArgs;
	newArgs.Tokenize(buf);
	Host_Say(nullptr, &newArgs, false, 0, nullptr);
}

void ParseChatCommand(const char *pMessage, CCSPlayerController *pController)
{
	if (!pController)
		return;

	char *token, *pos;

	char *pCommand = strdup(pMessage);
	token = strtok_s(strdup(pCommand) + 1, " ", &pos);

	if (!V_stricmp(token, "map"))
	{
		token = strtok_s(nullptr, " ", &pos);

		SendConsoleChat("[CS2Fixes] Changing map to %s", token);
		Message("[CS2Fixes] Changing map to %s\n", token);
	}
	else if (!V_stricmp(token, "takemoney"))
	{
		token = strtok_s(nullptr, " ", &pos);

		int money = pController->m_pInGameMoneyServices()->m_iAccount();
		pController->m_pInGameMoneyServices()->m_iAccount(money - atoi(token));
	}
	else if (!V_stricmp(token, "setcollision"))
	{
		Collision_Group_t colgroup = (Collision_Group_t)atoi(strtok_s(nullptr, " ", &pos));
		int uid = atoi(strtok_s(nullptr, " ", &pos));

		CBaseEntity *target = CGameEntitySystem::GetInstance()->GetBaseEntity(uid);
		if (target)
			target->m_pCollision()->m_CollisionGroup(colgroup);
	}
	else if (!V_stricmp(token, "setcollisionplayer"))
	{
		Collision_Group_t colgroup = (Collision_Group_t)atoi(strtok_s(nullptr, " ", &pos));
		int uid = atoi(strtok_s(nullptr, " ", &pos));

		CCSPlayerController *targetController = (CCSPlayerController *)CGameEntitySystem::GetInstance()->GetBaseEntity(uid);

		if (targetController)
			targetController->m_hPawn().Get()->m_pCollision()->m_CollisionGroup(colgroup);
	}
	else if (!V_stricmp(token, "message"))
	{
		// Note that the engine will treat this as a player slot number, not an entity index
		int uid = atoi(strtok_s(nullptr, " ", &pos));

		CBasePlayerController *player = (CBasePlayerController *)CGameEntitySystem::GetInstance()->GetBaseEntity(uid + 1);

		if (!player)
			return;

		char message[256];
		V_snprintf(message, sizeof(message), " \7[CS2Fixes]\1 Private message to %s: \5%s", &player->m_iszPlayerName(), pos);

		CRecipientFilter filter;

		filter.AddRecipient(uid);
		filter.MakeReliable();

		UTIL_SayTextFilter(filter, message, nullptr, 0);
	}
	else if (!V_stricmp(token, "setsolid"))
	{
		SolidType_t colgroup = (SolidType_t)atoi(strtok_s(nullptr, " ", &pos));
		int uid = atoi(strtok_s(nullptr, " ", &pos));

		CBaseEntity *target = CGameEntitySystem::GetInstance()->GetBaseEntity(uid);
		if (target)
			target->m_pCollision()->m_nSolidType(colgroup);
	}
	else if (!V_stricmp(token, "setglow"))
	{
		int uid = atoi(strtok_s(nullptr, " ", &pos));

		CBaseModelEntity *target = (CBaseModelEntity*)CGameEntitySystem::GetInstance()->GetBaseEntity(uid);
		if (target)
		{
			target->m_Glow().m_bGlowing(true);
			target->m_Glow().m_fGlowColor(Vector(255, 255, 0));
			target->m_Glow().m_iGlowType(1);
			target->m_Glow().m_nGlowRange(1000);
		}
	}
	else if (!V_stricmp(token, "basevelocity"))
	{
		int uid = atoi(strtok_s(nullptr, " ", &pos));
		float x = atof(strtok_s(nullptr, " ", &pos));
		float y = atof(strtok_s(nullptr, " ", &pos));
		float z = atof(strtok_s(nullptr, " ", &pos));

		CBasePlayerController *target = (CBasePlayerController *)CGameEntitySystem::GetInstance()->GetBaseEntity(uid);
		if (target)
			target->m_hPawn().Get()->m_vecBaseVelocity(Vector(x, y, z));
	}
	else if (!V_stricmp(token, "sethealth"))
	{
		int uid = atoi(strtok_s(nullptr, " ", &pos));
		int health = atoi(strtok_s(nullptr, " ", &pos));

		CBasePlayerController *target = (CBasePlayerController *)CGameEntitySystem::GetInstance()->GetBaseEntity(uid);
		if (target)
			target->m_hPawn().Get()->m_iHealth(health);
	}
	else
	{
		for (int i = 0; i < sizeof(WeaponMap) / sizeof(*WeaponMap); i++)
		{
			WeaponMapEntry_t weaponEntry = WeaponMap[i];

			if (!V_stricmp(token, weaponEntry.command))
			{
				uintptr_t pItemServices = (uintptr_t)pController->m_hPawn().Get<CCSPlayerPawn>()->m_pItemServices();
				int money = pController->m_pInGameMoneyServices()->m_iAccount();
				if (money >= weaponEntry.iPrice)
				{
					pController->m_pInGameMoneyServices()->m_iAccount(money - weaponEntry.iPrice);
					addresses::GiveNamedItem(pItemServices, weaponEntry.szWeaponName, 0, 0, 0, 0);
				}

				break;
			}
		}
	}

	free(pCommand);
}

void __fastcall Detour_Host_Say(CCSPlayerController *pController, CCommand *args, bool teamonly, int unk1, const char *unk2)
{
	const char *pMessage = args->Arg(1);

	if (*pMessage == '!' || *pMessage == '/')
		ParseChatCommand(pMessage, pController);

	if (*pMessage == '/')
		return;

	Host_Say(pController, args, teamonly, unk1, unk2);
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

	UTIL_SayTextFilter.CreateDetour();
	UTIL_SayTextFilter.EnableDetour();

	UTIL_SayText2Filter.CreateDetour();
	UTIL_SayText2Filter.EnableDetour();

	Host_Say.CreateDetour();
	Host_Say.EnableDetour();
}

void CRecipientFilter::Reset(void)
{
	m_bReliable = false;
	m_bInitMessage = false;
}

void CRecipientFilter::MakeReliable(void)
{
	m_bReliable = true;
}

bool CRecipientFilter::IsReliable(void) const
{
	return m_bReliable;
}

bool CRecipientFilter::IsInitMessage(void) const
{
	return m_bInitMessage;
}

int CRecipientFilter::GetRecipientCount(void) const
{
	return m_Recipients.Count();
}

CEntityIndex *CRecipientFilter::GetRecipientIndex(CEntityIndex *pEntIndex, int slot) const
{
	if (slot < 0 || slot >= GetRecipientCount())
		pEntIndex->_index = -1;
	else
		pEntIndex->_index = m_Recipients[slot];

	return pEntIndex;
}

void CRecipientFilter::AddRecipient(int entindex)
{
	// Already in list
	if (m_Recipients.Find(entindex) != m_Recipients.InvalidIndex())
		return;

	m_Recipients.AddToTail(entindex);
}
