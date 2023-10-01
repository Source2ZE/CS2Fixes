#include "detours.h"
#include "common.h"
#include "recipientfilters.h"
#include "commands.h"
#include "entity/ccsplayercontroller.h"
#include "entity/ccsplayerpawn.h"
#include "entity/cbasemodelentity.h"

#include "tier0/memdbgon.h"

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

void ParseWeaponCommand(CCSPlayerController *pController, const char *pszWeaponName)
{
	for (int i = 0; i < sizeof(WeaponMap) / sizeof(*WeaponMap); i++)
	{
		WeaponMapEntry_t weaponEntry = WeaponMap[i];

		if (!V_stricmp(pszWeaponName, weaponEntry.command))
		{
			void *pItemServices = (void*)pController->m_hPawn().Get()->m_pItemServices();
			int money = pController->m_pInGameMoneyServices()->m_iAccount();
			if (money >= weaponEntry.iPrice)
			{
				pController->m_pInGameMoneyServices()->m_iAccount(money - weaponEntry.iPrice);
				addresses::GiveNamedItem(pItemServices, weaponEntry.szWeaponName, nullptr, nullptr, nullptr, nullptr);
			}

			break;
		}
	}
}

typedef void (*FnChatCommandCallback_t)(const CCommand &args, CCSPlayerController *player);

CUtlMap<const char *, FnChatCommandCallback_t> g_CommandList(0, 0, DefLessFunc(const char *));

#define CON_COMMAND_CHAT(name, callback) \
	g_CommandList.Insert(#name, &callback);

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

void SentChatToClient(int index, const char *msg, ...)
{
	va_list args;
	va_start(args, msg);

	char buf[256];
	V_vsnprintf(buf, sizeof(buf), msg, args);

	va_end(args);

	CSingleRecipientFilter filter(index);

	UTIL_SayTextFilter(filter, buf, nullptr, 0);
}

void say_chat_callback(const CCommand &args, CCSPlayerController *player)
{
	SendConsoleChat(" \1%s", args.GetCommandString());
}

void takemoney_chat_callback(const CCommand &args, CCSPlayerController *player)
{
	if (!player)
		return;

	int amount = atoi(args[0]);
	int money = player->m_pInGameMoneyServices()->m_iAccount();

	player->m_pInGameMoneyServices()->m_iAccount(money - amount);
}

void message_chat_callback(const CCommand &args, CCSPlayerController *player)
{
	// Note that the engine will treat this as a player slot number, not an entity index
	int uid = atoi(args[0]);

	CBasePlayerController *target = (CBasePlayerController *)g_pEntitySystem->GetBaseEntity((CEntityIndex)(uid + 1));

	if (!target)
		return;

	char message[256];
	V_snprintf(message, sizeof(message), " \7[CS2Fixes]\1 Private message to %s: \5%s", &player->m_iszPlayerName(), args[1]);

	CSingleRecipientFilter filter(uid);

	UTIL_SayTextFilter(filter, message, nullptr, 0);
}

void sethealth_chat_callback(const CCommand &args, CCSPlayerController *player)
{
	if (!player)
		return;

	int health = atoi(args[0]);

	player->m_hPawn().Get()->m_iHealth(health);
}

void RegisterChatCommands()
{
	g_CommandList.RemoveAll();

	CON_COMMAND_CHAT(say, say_chat_callback);
	CON_COMMAND_CHAT(takemoney, takemoney_chat_callback);
	CON_COMMAND_CHAT(message, message_chat_callback);
	CON_COMMAND_CHAT(sethealth, sethealth_chat_callback);
}

void ParseChatCommand(const char *pMessage, CCSPlayerController *pController)
{
	if (!pController)
		return;

	char *token, *next;

	char *pCommand = strdup(pMessage);
	token = strtok_s(strdup(pCommand) + 1, " ", &next);

	auto p = g_CommandList.Find(token);

	if (g_CommandList.IsValidIndex(p))
	{
		CCommandContext context(CT_NO_TARGET, 0);
		CCommand args;
		args.Tokenize(next);
		g_CommandList[p](args, pController);
	}
	else
	{
		ParseWeaponCommand(pController, token);
	}

	free(pCommand);
}