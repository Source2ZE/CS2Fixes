#include "detours.h"
#include "common.h"
#include "recipientfilters.h"
#include "commands.h"
#include "entity/ccsplayercontroller.h"
#include "entity/ccsplayerpawn.h"
#include "entity/cbasemodelentity.h"
#include "playermanager.h"

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

CUtlMap<uint32, FnChatCommandCallback_t> g_CommandList(0, 0, DefLessFunc(uint32));

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

CON_COMMAND_CHAT(stopsound, "stop weapon sounds")
{
	int iPlayer = player->entindex() - 1;

	ZEPlayer *pPlayer = g_playerManager->GetPlayer(iPlayer);

	// Something has to really go wrong for this to happen
	if (!pPlayer)
	{
		Warning("%d Tried to access a null ZEPlayer!!\n", iPlayer);
		return;
	}

	SentChatToClient(iPlayer, " \7[CS2Fixes]\1 You have toggled weapon effects.");

	pPlayer->ToggleStopSound();
}

CON_COMMAND_CHAT(say, "say something using console")
{
	SendConsoleChat("%s", args.ArgS());
}

CON_COMMAND_CHAT(takemoney, "take your money")
{
	if (!player)
		return;

	int amount = atoi(args[1]);
	int money = player->m_pInGameMoneyServices()->m_iAccount();

	player->m_pInGameMoneyServices()->m_iAccount(money - amount);
}

CON_COMMAND_CHAT(message, "message someone")
{
	if (!player)
		return;

	// Note that the engine will treat this as a player slot number, not an entity index
	int uid = atoi(args[1]);

	CBasePlayerController *target = (CBasePlayerController *)g_pEntitySystem->GetBaseEntity((CEntityIndex)(uid + 1));

	if (!target)
		return;

	// skipping the id and space, it's dumb but w/e
	const char *pMessage = args.ArgS() + V_strlen(args[1]) + 1;

	char buf[256];
	V_snprintf(buf, sizeof(buf), " \7[CS2Fixes]\1 Private message from %s to %s: \5%s", &player->m_iszPlayerName(), &target->m_iszPlayerName(), pMessage);

	CSingleRecipientFilter filter(uid);

	UTIL_SayTextFilter(filter, buf, nullptr, 0);
}

CON_COMMAND_CHAT(sethealth, "set your health")
{
	if (!player)
		return;

	int health = atoi(args[1]);

	player->m_hPawn().Get()->m_iHealth(health);
}

void ParseChatCommand(const char *pMessage, CCSPlayerController *pController)
{
	if (!pController)
		return;

	CCommand args;
	args.Tokenize(pMessage + 1);

	uint16 index = g_CommandList.Find(hash_32_fnv1a_const(args[0]));

	if (g_CommandList.IsValidIndex(index))
	{
		g_CommandList[index](args, pController);
	}
	else
	{
		ParseWeaponCommand(pController, args[0]);
	}
}