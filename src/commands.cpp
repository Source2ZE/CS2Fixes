/**
 * =============================================================================
 * CS2Fixes
 * Copyright (C) 2023-2024 Source2ZE
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

#include "usermessages.pb.h"
#include "detours.h"
#include "common.h"
#include "utlstring.h"
#include "recipientfilters.h"
#include "commands.h"
#include "utils/entity.h"
#include "entity/cbaseentity.h"
#include "entity/ccsweaponbase.h"
#include "entity/ccsplayercontroller.h"
#include "entity/ccsplayerpawn.h"
#include "entity/cbasemodelentity.h"
#include "entity/ccsweaponbase.h"
#include "entity/cparticlesystem.h"
#include "entity/lights.h"
#include "playermanager.h"
#include "adminsystem.h"
#include "ctimer.h"
#include "httpmanager.h"
#include "discord.h"
#include "zombiereborn.h"
#include "networksystem/inetworkmessages.h"
#include "engine/igameeventsystem.h"
#include "tier0/vprof.h"
#undef snprintf
#include "vendor/nlohmann/json.hpp"

#include "tier0/memdbgon.h"

using json = nlohmann::json;

extern IGameEventSystem *g_gameEventSystem;
extern CGameEntitySystem *g_pEntitySystem;
extern IVEngineServer2* g_pEngineServer2;
extern ISteamHTTP* g_http;

bool g_bEnableCommands;
bool g_bEnableAdminCommands;

FAKE_BOOL_CVAR(cs2f_commands_enable, "Whether to enable chat commands", g_bEnableCommands, false, 0)
FAKE_BOOL_CVAR(cs2f_admin_commands_enable, "Whether to enable admin chat commands", g_bEnableAdminCommands, false, 0)

WeaponMapEntry_t WeaponMap[] = {
	{{"bizon"},							"weapon_bizon",			"PP-Bizon",			1400, 26, GEAR_SLOT_RIFLE},
	{{"mac10", "mac"},					"weapon_mac10",			"MAC-10",			1050, 27, GEAR_SLOT_RIFLE},
	{{"mp5sd", "mp5"},					"weapon_mp5sd",			"MP5-SD",			1500, 23, GEAR_SLOT_RIFLE},
	{{"mp7"},							"weapon_mp7",			"MP7",				1500, 23, GEAR_SLOT_RIFLE},
	{{"mp9"},							"weapon_mp9",			"MP9",				1250, 34, GEAR_SLOT_RIFLE},
	{{"p90"},							"weapon_p90",			"P90",				2350, 19, GEAR_SLOT_RIFLE},
	{{"ump45", "ump"},					"weapon_ump45",			"UMP-45",			1200, 24, GEAR_SLOT_RIFLE},
	{{"ak47", "ak"},					"weapon_ak47",			"AK-47",			2700, 7, GEAR_SLOT_RIFLE},
	{{"aug"},							"weapon_aug",			"AUG",				3300, 8, GEAR_SLOT_RIFLE},
	{{"famas"},							"weapon_famas",			"FAMAS",			2050, 10, GEAR_SLOT_RIFLE},
	{{"galilar", "galil"},				"weapon_galilar",		"Galil AR",			1800, 13, GEAR_SLOT_RIFLE},
	{{"m4a4"},							"weapon_m4a1",			"M4A4",				3100, 16, GEAR_SLOT_RIFLE},
	{{"m4a1-s", "m4a1"},				"weapon_m4a1_silencer",	"M4A1-S",			2900, 60, GEAR_SLOT_RIFLE},
	{{"sg553"},							"weapon_sg556",			"SG 553",			3000, 39, GEAR_SLOT_RIFLE},
	{{"awp"},							"weapon_awp",			"AWP",				4750, 9, GEAR_SLOT_RIFLE},
	{{"g3sg1"},							"weapon_g3sg1",			"G3SG1",			5000, 11, GEAR_SLOT_RIFLE},
	{{"scar20", "scar"},				"weapon_scar20",		"SCAR-20",			5000, 38, GEAR_SLOT_RIFLE},
	{{"ssg08", "ssg"},					"weapon_ssg08",			"SSG 08",			1700, 40, GEAR_SLOT_RIFLE},
	{{"mag7", "mag"},					"weapon_mag7",			"MAG-7",			1300, 29, GEAR_SLOT_RIFLE},
	{{"nova"},							"weapon_nova",			"Nova",				1050, 35, GEAR_SLOT_RIFLE},
	{{"sawedoff"},						"weapon_sawedoff",		"Sawed-Off",		1100, 29, GEAR_SLOT_RIFLE},
	{{"xm1014", "xm"},					"weapon_xm1014",		"XM1014",			2000, 25, GEAR_SLOT_RIFLE},
	{{"m249"},							"weapon_m249",			"M249",				5200, 14, GEAR_SLOT_RIFLE},
	{{"negev"},							"weapon_negev",			"Negev",			1700, 28, GEAR_SLOT_RIFLE},
	{{"deagle"},						"weapon_deagle",		"Desert Eagle",		700, 1, GEAR_SLOT_PISTOL},
	{{"dualberettas", "elite"},			"weapon_elite",			"Dual Berettas",	300, 2, GEAR_SLOT_PISTOL},
	{{"fiveseven"},						"weapon_fiveseven",		"Five-SeveN",		500, 3, GEAR_SLOT_PISTOL},
	{{"glock18", "glock"},				"weapon_glock",			"Glock-18",			200, 4, GEAR_SLOT_PISTOL},
	{{"p2000"},							"weapon_hkp2000",		"P2000",			200, 32, GEAR_SLOT_PISTOL},
	{{"p250"},							"weapon_p250",			"P250",				300, 36, GEAR_SLOT_PISTOL},
	{{"tec9"},							"weapon_tec9",			"Tec-9",			500, 30, GEAR_SLOT_PISTOL},
	{{"usp-s", "usp"},					"weapon_usp_silencer",	"USP-S",			200, 61, GEAR_SLOT_PISTOL},
	{{"cz75-auto", "cs75a", "cz"},		"weapon_cz75a",			"CZ75-Auto",		500, 63, GEAR_SLOT_PISTOL},
	{{"r8revolver", "revolver", "r8"},	"weapon_revolver",		"R8 Revolver",		600, 64, GEAR_SLOT_PISTOL},
	{{"hegrenade", "he"},				"weapon_hegrenade",		"HE Grenade",		300, 44, GEAR_SLOT_GRENADES, 1},
	{{"molotov"},						"weapon_molotov",		"Molotov",			400, 46, GEAR_SLOT_GRENADES, 1},
	{{"kevlar"},						"item_kevlar",			"Kevlar Vest",		650, 50, GEAR_SLOT_UTILITY},
};

bool g_bEnableWeapons = false;

FAKE_BOOL_CVAR(cs2f_weapons_enable, "Whether to enable weapon commands", g_bEnableWeapons, false, false)

void ParseWeaponCommand(const CCommand& args, CCSPlayerController* player)
{
	if (!g_bEnableWeapons || !player || !player->m_hPawn())
		return;

	VPROF("ParseWeaponCommand");

	CCSPlayerPawn* pPawn = (CCSPlayerPawn*)player->GetPawn();
	WeaponMapEntry_t weaponEntry;
	bool foundWeapon = false;

	for (int i = 0; i < sizeof(WeaponMap) / sizeof(*WeaponMap); i++)
	{
		if (foundWeapon)
			break;

		weaponEntry = WeaponMap[i];
		const char* command = args[0];

		if (!V_strncmp("c_", command, 2))
			command = command + 2;

		for (std::string alias : weaponEntry.aliases)
		{
			if (!V_stricmp(command, alias.c_str()))
			{
				foundWeapon = true;
				break;
			}
		}
	}

	if (!foundWeapon)
		return;

	if (pPawn->m_iHealth() <= 0 || pPawn->m_iTeamNum != CS_TEAM_CT)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "You can only buy weapons when human.");
		return;
	}

	CCSPlayer_ItemServices* pItemServices = pPawn->m_pItemServices;
	CCSPlayer_WeaponServices* pWeaponServices = pPawn->m_pWeaponServices;

	// it can sometimes be null when player joined on the very first round? 
	if (!pItemServices || !pWeaponServices)
		return;

	int money = player->m_pInGameMoneyServices->m_iAccount;

	if (money < weaponEntry.iPrice)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "You can't afford %s! It costs $%i, you only have $%i", weaponEntry.szWeaponName, weaponEntry.iPrice, money);
		return;
	}

	if (weaponEntry.maxAmount)
	{
		CUtlVector<WeaponPurchaseCount_t>* weaponPurchases = pPawn->m_pActionTrackingServices->m_weaponPurchasesThisRound().m_weaponPurchases;
		bool found = false;
		FOR_EACH_VEC(*weaponPurchases, i)
		{
			WeaponPurchaseCount_t& purchase = (*weaponPurchases)[i];
			if (purchase.m_nItemDefIndex == weaponEntry.iItemDefIndex)
			{
				if (purchase.m_nCount >= weaponEntry.maxAmount)
				{
					ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "You cannot buy any more %s (Max %i)", weaponEntry.szWeaponName, weaponEntry.maxAmount);
					return;
				}
				purchase.m_nCount += 1;
				found = true;
				break;
			}
		}

		if (!found)
		{
			WeaponPurchaseCount_t purchase = {};

			purchase.m_nCount = 1;
			purchase.m_nItemDefIndex = weaponEntry.iItemDefIndex;

			weaponPurchases->AddToTail(purchase);
		}
	}

	CUtlVector<CHandle<CBasePlayerWeapon>>* weapons = pWeaponServices->m_hMyWeapons();

	FOR_EACH_VEC(*weapons, i)
	{
		CBasePlayerWeapon* weapon = (*weapons)[i].Get();

		if (!weapon)
			continue;

		if (weapon->GetWeaponVData()->m_GearSlot() == weaponEntry.iGearSlot && (weaponEntry.iGearSlot == GEAR_SLOT_RIFLE || weaponEntry.iGearSlot == GEAR_SLOT_PISTOL))
		{
			pWeaponServices->DropWeapon(weapon);
			break;
		}
	}

	player->m_pInGameMoneyServices->m_iAccount = money - weaponEntry.iPrice;
	pItemServices->GiveNamedItem(weaponEntry.szClassName);
	ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "You have purchased %s for $%i", weaponEntry.szWeaponName, weaponEntry.iPrice);
}

void WeaponCommandCallback(const CCommandContext& context, const CCommand& args)
{
	CCSPlayerController* pController = nullptr;
	if (context.GetPlayerSlot().Get() != -1)
		pController = (CCSPlayerController*)g_pEntitySystem->GetEntityInstance((CEntityIndex)(context.GetPlayerSlot().Get() + 1));

	// Only allow connected players to run chat commands
	if (pController && !pController->IsConnected())
		return;

	ParseWeaponCommand(args, pController);
}

void RegisterWeaponCommands()
{
	for (int i = 0; i < sizeof(WeaponMap) / sizeof(*WeaponMap); i++)
	{
		WeaponMapEntry_t weaponEntry = WeaponMap[i];

		for (std::string alias : weaponEntry.aliases)
		{
			new CChatCommand(alias.c_str(), ParseWeaponCommand, "- Buys this weapon", ADMFLAG_NONE, CMDFLAG_NOHELP);
			ConCommandRefAbstract ref;

			char cmdName[64];
			V_snprintf(cmdName, sizeof(cmdName), "%s%s", COMMAND_PREFIX, alias.c_str());

			new ConCommand(&ref, cmdName, WeaponCommandCallback, "Buys this weapon", FCVAR_RELEASE | FCVAR_CLIENT_CAN_EXECUTE | FCVAR_LINKED_CONCOMMAND);
		}
	}
}

void ParseChatCommand(const char *pMessage, CCSPlayerController *pController)
{
	if (!pController || !pController->IsConnected())
		return;

	VPROF("ParseChatCommand");

	CCommand args;
	args.Tokenize(pMessage);
	std::string name = args[0];

	for (int i = 0; name[i]; i++)
		name[i] = tolower(name[i]);

	uint16 index = g_CommandList.Find(hash_32_fnv1a_const(name.c_str()));

	if (g_CommandList.IsValidIndex(index))
	{
		(*g_CommandList[index])(args, pController);
	}
}

bool CChatCommand::CheckCommandAccess(CCSPlayerController *pPlayer, uint64 flags)
{
	if (!pPlayer)
		return false;

	int slot = pPlayer->GetPlayerSlot();

	ZEPlayer *pZEPlayer = g_playerManager->GetPlayer(slot);

	if (!pZEPlayer->IsAdminFlagSet(flags))
	{
		ClientPrint(pPlayer, HUD_PRINTTALK, CHAT_PREFIX "You don't have access to this command.");
		return false;
	}

	return true;
}

void ClientPrintAll(int hud_dest, const char *msg, ...)
{
	va_list args;
	va_start(args, msg);

	char buf[256];
	V_vsnprintf(buf, sizeof(buf), msg, args);

	va_end(args);

	INetworkMessageInternal *pNetMsg = g_pNetworkMessages->FindNetworkMessagePartial("TextMsg");
	auto data = pNetMsg->AllocateMessage()->ToPB<CUserMessageTextMsg>();

	data->set_dest(hud_dest);
	data->add_param(buf);

	CRecipientFilter filter;
	filter.AddAllPlayers();

	g_gameEventSystem->PostEventAbstract(-1, false, &filter, pNetMsg, data, 0);

	delete data;

	ConMsg("%s\n", buf);
}

void ClientPrint(CCSPlayerController *player, int hud_dest, const char *msg, ...)
{
	va_list args;
	va_start(args, msg);

	char buf[256];
	V_vsnprintf(buf, sizeof(buf), msg, args);

	va_end(args);

	if (!player || !player->IsConnected() || player->IsBot())
	{
		ConMsg("%s\n", buf);
		return;
	}

	INetworkMessageInternal *pNetMsg = g_pNetworkMessages->FindNetworkMessagePartial("TextMsg");
	auto data = pNetMsg->AllocateMessage()->ToPB<CUserMessageTextMsg>();

	data->set_dest(hud_dest);
	data->add_param(buf);

	CSingleRecipientFilter filter(player->GetPlayerSlot());

	g_gameEventSystem->PostEventAbstract(-1, false, &filter, pNetMsg, data, 0);

	delete data;
}

bool g_bEnableStopSound = false;

FAKE_BOOL_CVAR(cs2f_stopsound_enable, "Whether to enable stopsound", g_bEnableStopSound, false, false)


CON_COMMAND_CHAT(stopsound, "- Toggle weapon sounds")
{
	if (!g_bEnableStopSound)
		return;

	if (!player)
	{
		ClientPrint(player, HUD_PRINTCONSOLE, CHAT_PREFIX "You cannot use this command from the server console.");
		return;
	}

	int iPlayer = player->GetPlayerSlot();
	bool bStopSet = g_playerManager->IsPlayerUsingStopSound(iPlayer);
	bool bSilencedSet = g_playerManager->IsPlayerUsingSilenceSound(iPlayer);

	g_playerManager->SetPlayerStopSound(iPlayer, bSilencedSet);
	g_playerManager->SetPlayerSilenceSound(iPlayer, !bSilencedSet && !bStopSet);

	ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "You have %s weapon sounds.", bSilencedSet ? "disabled" : !bSilencedSet && !bStopSet ? "silenced" : "enabled");
}

CON_COMMAND_CHAT(toggledecals, "- Toggle world decals, if you're into having 10 fps in ZE")
{
	if (!player)
	{
		ClientPrint(player, HUD_PRINTCONSOLE, CHAT_PREFIX "You cannot use this command from the server console.");
		return;
	}

	int iPlayer = player->GetPlayerSlot();
	bool bSet = !g_playerManager->IsPlayerUsingStopDecals(iPlayer);

	g_playerManager->SetPlayerStopDecals(iPlayer, bSet);

	ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "You have %s world decals.", bSet ? "disabled" : "enabled");
}

bool g_bEnableNoShake = false;
FAKE_BOOL_CVAR(cs2f_noshake_enable, "Whether to enable noshake command", g_bEnableNoShake, false, false)
float g_flMaxShakeAmp = -1.0;
FAKE_FLOAT_CVAR(cs2f_maximum_shake_amplitude, "Shaking Amplitude bigger than this will be clamped", g_flMaxShakeAmp, -1.0, false)
CON_COMMAND_CHAT(noshake, "- toggle noshake")
{
	if (!g_bEnableNoShake)
		return;

	if (!player)
	{
		ClientPrint(player, HUD_PRINTCONSOLE, CHAT_PREFIX "You cannot use this command from the server console.");
		return;
	}

	int iPlayer = player->GetPlayerSlot();
	bool bSet = !g_playerManager->IsPlayerUsingNoShake(iPlayer);

	g_playerManager->SetPlayerNoShake(iPlayer, bSet);
	ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "You have %s noshake.", bSet ? "enabled" : "disabled");
}

bool g_bEnableHide = false;
static int g_iDefaultHideDistance = 250;
static int g_iMaxHideDistance = 2000;

FAKE_BOOL_CVAR(cs2f_hide_enable, "Whether to enable hide", g_bEnableHide, false, false)
FAKE_INT_CVAR(cs2f_hide_distance_default, "The default distance for hide", g_iDefaultHideDistance, 250, false)
FAKE_INT_CVAR(cs2f_hide_distance_max, "The max distance for hide", g_iMaxHideDistance, 2000, false)

CON_COMMAND_CHAT(hide, "<distance> - Hide nearby players")
{
	// Silently return so the command is completely hidden
	if (!g_bEnableHide)
		return;

	if (!player)
	{
		ClientPrint(player, HUD_PRINTCONSOLE, CHAT_PREFIX "You cannot use this command from the server console.");
		return;
	}

	int distance;

	if (args.ArgC() < 2)
		distance = g_iDefaultHideDistance;
	else
		distance = V_StringToInt32(args[1], -1);

	if (distance > g_iMaxHideDistance || distance < 0)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "You can only hide players between 0 and %i units away.", g_iMaxHideDistance);
		return;
	}

	ZEPlayer *pZEPlayer = player->GetZEPlayer();

	// Something has to really go wrong for this to happen
	if (!pZEPlayer)
	{
		Warning("%s Tried to access a null ZEPlayer!!\n", player->GetPlayerName());
		return;
	}

	// allows for toggling hide by turning off when hide distance matches.
	if (pZEPlayer->GetHideDistance() == distance)
		distance = 0;

	pZEPlayer->SetHideDistance(distance);

	if (distance == 0)
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Hiding players is now disabled.");
	else
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Now hiding players within %i units.", distance);
}

CON_COMMAND_CHAT(help, "- Display list of commands in console")
{
	std::vector<std::string> rgstrCommands;
	if (!player)
	{
		ClientPrint(player, HUD_PRINTCONSOLE, "The list of all commands is:");

		FOR_EACH_VEC(g_CommandList, i)
		{
			CChatCommand* cmd = g_CommandList[i];

			if (!cmd->IsCommandFlagSet(CMDFLAG_NOHELP))
				rgstrCommands.push_back(std::string("c_") + cmd->GetName() + " " + cmd->GetDescription());
		}
	}
	else
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "The list of all available commands will be shown in console.");
		ClientPrint(player, HUD_PRINTCONSOLE, "The list of all commands you can use is:");

		ZEPlayer* pZEPlayer = player->GetZEPlayer();

		FOR_EACH_VEC(g_CommandList, i)
		{
			CChatCommand* cmd = g_CommandList[i];
			uint64 flags = cmd->GetAdminFlags();

			if (pZEPlayer->IsAdminFlagSet(flags) && !cmd->IsCommandFlagSet(CMDFLAG_NOHELP))
				rgstrCommands.push_back(std::string("!") + cmd->GetName() + " " + cmd->GetDescription());
		}
	}

	std::sort(rgstrCommands.begin(), rgstrCommands.end());

	for (const auto& strCommand : rgstrCommands)
		ClientPrint(player, HUD_PRINTCONSOLE, strCommand.c_str());

	if (player)
		ClientPrint(player, HUD_PRINTCONSOLE, "! can be replaced with / for a silent chat command, or c_ for console usage");
}

CON_COMMAND_CHAT(spec, "[name] - Spectate another player or join spectators")
{
	if (!player)
	{
		ClientPrint(player, HUD_PRINTCONSOLE, CHAT_PREFIX "You cannot use this command from the server console.");
		return;
	}

	if (args.ArgC() < 2)
	{
		if (player->m_iTeamNum() == CS_TEAM_SPECTATOR)
		{
			ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Already spectating.");
		}
		else
		{
			player->SwitchTeam(CS_TEAM_SPECTATOR);
			ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Moved to spectators.");
		}
		return;
	}

	int iCommandPlayer = player ? player->GetPlayerSlot() : -1;
	int iNumClients = 0;
	int pSlot[MAXPLAYERS];

	if (!g_playerManager->CanTargetPlayers(player, args[1], iNumClients, pSlot, NO_MULTIPLE | NO_SELF | NO_DEAD | NO_SPECTATOR))
		return;

	CCSPlayerController* pTarget = CCSPlayerController::FromSlot(pSlot[0]);

	if (!pTarget)
		return;

	if (player->m_iTeamNum() != CS_TEAM_SPECTATOR)
		player->SwitchTeam(CS_TEAM_SPECTATOR);

	// 1 frame delay as observer services will be null on same frame as spectator team switch
	CHandle<CCSPlayerController> hPlayer = player->GetHandle();
	CHandle<CCSPlayerController> hTarget = pTarget->GetHandle();
	new CTimer(0.0f, false, false, [hPlayer, hTarget](){
		CCSPlayerController* pPlayer = hPlayer.Get();
		CCSPlayerController* pTargetPlayer = hTarget.Get();
		if (!pPlayer || !pTargetPlayer)
			return -1.0f;
		CPlayer_ObserverServices* pObserverServices = pPlayer->GetPawn()->m_pObserverServices();
		if (!pObserverServices)
			return -1.0f;
		pObserverServices->m_iObserverMode.Set(OBS_MODE_IN_EYE);
		pObserverServices->m_iObserverLastMode.Set(OBS_MODE_ROAMING);
		pObserverServices->m_hObserverTarget.Set(pTargetPlayer->GetPawn());
		ClientPrint(pPlayer, HUD_PRINTTALK, CHAT_PREFIX "Spectating player %s.", pTargetPlayer->GetPlayerName());
		return -1.0f;
	});
}

CON_COMMAND_CHAT(getpos, "- Get your position and angles")
{
	if (!player)
		return;

	Vector vecAbsOrigin = player->GetPawn()->GetAbsOrigin();
	QAngle angRotation = player->GetPawn()->GetAbsRotation();

	ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "setpos %f %f %f;setang %f %f %f", vecAbsOrigin.x, vecAbsOrigin.y, vecAbsOrigin.z, angRotation.x, angRotation.y, angRotation.z);
	ClientPrint(player, HUD_PRINTCONSOLE, "setpos %f %f %f;setang %f %f %f", vecAbsOrigin.x, vecAbsOrigin.y, vecAbsOrigin.z, angRotation.x, angRotation.y, angRotation.z);
}

CON_COMMAND_CHAT(info, "<name> - Get a player's information")
{
	if (args.ArgC() < 2)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Usage: !info <name>");
		return;
	}

	int iNumClients = 0;
	int pSlots[MAXPLAYERS];
	ETargetType nType;

	if (!g_playerManager->CanTargetPlayers(player, args[1], iNumClients, pSlots, NO_BOT | NO_IMMUNITY, nType))
		return;

	ZEPlayer* pPlayer = player ? player->GetZEPlayer() : nullptr;
	bool bIsAdmin = pPlayer ? pPlayer->IsAdminFlagSet(ADMFLAG_GENERIC) : true;

	for (int i = 0; i < iNumClients; i++)
	{
		CCSPlayerController* pTarget = CCSPlayerController::FromSlot(pSlots[i]);
		ZEPlayer* zpTarget = pTarget->GetZEPlayer();
		
		ClientPrint(player, HUD_PRINTCONSOLE, "%s", pTarget->GetPlayerName());
		ClientPrint(player, HUD_PRINTCONSOLE, "\tUser ID: %i", g_pEngineServer2->GetPlayerUserId(pTarget->GetPlayerSlot()).Get());

		if (zpTarget->IsAuthenticated())
			ClientPrint(player, HUD_PRINTCONSOLE, "\tSteam64 ID: %llu", zpTarget->GetSteamId64());
		else
			ClientPrint(player, HUD_PRINTCONSOLE, "\tSteam64 ID: %llu (Unauthenticated)", zpTarget->GetUnauthenticatedSteamId());

		if (bIsAdmin)
			ClientPrint(player, HUD_PRINTCONSOLE, "\tIP Address: %s", zpTarget->GetIpAddress());
	}

	ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Printed matching player%s information to console.", (iNumClients == 1) ? "'s" : "s'");
}

CON_COMMAND_CHAT(showteam, "<name> - Get a player's current team")
{
	if (args.ArgC() < 2)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Usage: !showteam <name>");
		return;
	}

	int iNumClients = 0;
	int pSlots[MAXPLAYERS];

	if (!g_playerManager->CanTargetPlayers(player, args[1], iNumClients, pSlots, NO_MULTIPLE | NO_IMMUNITY))
		return;

	CCSPlayerController* pTarget = CCSPlayerController::FromSlot(pSlots[0]);

	switch (pTarget->m_iTeamNum())
	{
		case CS_TEAM_SPECTATOR:
			ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "%s is a\x08 spectator\x01.", pTarget->GetPlayerName());
			break;
		case CS_TEAM_T:
			ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "%s is a\x09 terrorist\x01.", pTarget->GetPlayerName());
			break;
		case CS_TEAM_CT:
			ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "%s is a\x0B counter-terrorist\x01.", pTarget->GetPlayerName());
			break;
		default:
			ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "%s is not on a team.", pTarget->GetPlayerName());
	}
}

#if _DEBUG
CON_COMMAND_CHAT(myuid, "- Test")
{
	if (!player)
		return;

	int iPlayer = player->GetPlayerSlot();

	ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Your userid is %i, slot: %i, retrieved slot: %i", g_pEngineServer2->GetPlayerUserId(iPlayer).Get(), iPlayer, g_playerManager->GetSlotFromUserId(g_pEngineServer2->GetPlayerUserId(iPlayer).Get()));
}

CON_COMMAND_CHAT(myhandle, "- Test")
{
	if (!player)
		return;

	int entry = player->GetHandle().GetEntryIndex();
	int serial = player->GetHandle().GetSerialNumber();

	ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "entry index: %d    serial number: %d", entry, serial);
}

CON_COMMAND_CHAT(fl, "- Flashlight")
{
	if (!player)
		return;

	CCSPlayerPawn *pPawn = (CCSPlayerPawn *)player->GetPawn();

	auto ptr = pPawn->m_pMovementServices->m_nButtons().m_pButtonStates();

	Vector origin = pPawn->GetAbsOrigin();
	Vector forward;
	AngleVectors(pPawn->m_angEyeAngles(), &forward);

	origin.z += 64.0f;
	origin += forward * 54.0f; // The minimum distance such that an awp wouldn't block the light

	CBarnLight *pLight = CreateEntityByName<CBarnLight>("light_barn");

	pLight->m_bEnabled = true;
	pLight->m_Color->SetColor(255, 255, 255, 255);
	pLight->m_flBrightness = 1.0f;
	pLight->m_flRange = 2048.0f;
	pLight->m_flSoftX = 1.0f;
	pLight->m_flSoftY = 1.0f;
	pLight->m_flSkirt = 0.5f;
	pLight->m_flSkirtNear = 1.0f;
	pLight->m_vSizeParams->Init(45.0f, 45.0f, 0.03f);
	pLight->m_nCastShadows = 1;
	pLight->m_nDirectLight = 3;
	pLight->Teleport(&origin, &pPawn->m_angEyeAngles(), nullptr);

	// Have to use keyvalues for this since the schema prop is a resource handle
	CEntityKeyValues *pKeyValues = new CEntityKeyValues();
	pKeyValues->SetString("lightcookie", "materials/effects/lightcookies/flashlight.vtex");

	pLight->DispatchSpawn(pKeyValues);

	variant_t val("!player");
	pLight->AcceptInput("SetParent", &val);
	variant_t val2("clip_limit");
	pLight->AcceptInput("SetParentAttachmentMaintainOffset", &val2);
}

CON_COMMAND_CHAT(message, "<id> <message> - Message someone")
{
	if (!player)
		return;

	// Note that the engine will treat this as a player slot number, not an entity index
	int uid = atoi(args[1]);

	CCSPlayerController* pTarget = CCSPlayerController::FromSlot(uid);

	if (!pTarget)
		return;

	// skipping the id and space, it's dumb but w/e
	const char *pMessage = args.ArgS() + V_strlen(args[1]) + 1;

	ClientPrint(pTarget, HUD_PRINTTALK, CHAT_PREFIX "Private message from %s to %s: \5%s", player->GetPlayerName(), pTarget->GetPlayerName(), pMessage);
}

CON_COMMAND_CHAT(say, "<message> - Say something using console")
{
	ClientPrintAll(HUD_PRINTTALK, "%s", args.ArgS());
}

CON_COMMAND_CHAT(takemoney, "<amount> - Take your money")
{
	if (!player)
		return;

	int amount = atoi(args[1]);
	int money = player->m_pInGameMoneyServices->m_iAccount;

	player->m_pInGameMoneyServices->m_iAccount = money - amount;
}

CON_COMMAND_CHAT(sethealth, "<health> - Set your health")
{
	if (!player)
		return;

	int health = atoi(args[1]);

	CBaseEntity *pEnt = (CBaseEntity *)player->GetPawn();

	pEnt->m_iHealth = health;

	ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Your health is now %d", health);
}

CON_COMMAND_CHAT(test_target, "<name> [blocked flag] [...] - Test string targetting")
{
	if (!player)
		return;

	if (args.ArgC() < 2)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Usage: !test_target <name> [blocked flag] [...]");
		return;
	}

	uint64 iBlockedFlags = NO_TARGET_BLOCKS;
	for (int i = 1; i < args.ArgC(); i++)
	{
		if (!V_stricmp(args[i], "NO_RANDOM"))
			iBlockedFlags |= NO_RANDOM;
		else if (!V_stricmp(args[i], "NO_MULTIPLE"))
			iBlockedFlags |= NO_MULTIPLE;
		else if (!V_stricmp(args[i], "NO_SELF"))
			iBlockedFlags |= NO_SELF;
		else if (!V_stricmp(args[i], "NO_BOT"))
			iBlockedFlags |= NO_BOT;
		else if (!V_stricmp(args[i], "NO_HUMAN"))
			iBlockedFlags |= NO_HUMAN;
		else if (!V_stricmp(args[i], "NO_UNAUTHENTICATED"))
			iBlockedFlags |= NO_UNAUTHENTICATED;
		else if (!V_stricmp(args[i], "NO_DEAD"))
			iBlockedFlags |= NO_DEAD;
		else if (!V_stricmp(args[i], "NO_ALIVE"))
			iBlockedFlags |= NO_ALIVE;
		else if (!V_stricmp(args[i], "NO_TERRORIST"))
			iBlockedFlags |= NO_TERRORIST;
		else if (!V_stricmp(args[i], "NO_COUNTER_TERRORIST"))
			iBlockedFlags |= NO_COUNTER_TERRORIST;
		else if (!V_stricmp(args[i], "NO_SPECTATOR"))
			iBlockedFlags |= NO_SPECTATOR;
		else if (!V_stricmp(args[i], "NO_IMMUNITY"))
			iBlockedFlags |= NO_IMMUNITY;
	}

	int iNumClients = 0;
	int pSlots[MAXPLAYERS];

	if (!g_playerManager->CanTargetPlayers(player, args[1], iNumClients, pSlots, iBlockedFlags))
		return;

	for (int i = 0; i < iNumClients; i++)
	{
		CCSPlayerController* pTarget = CCSPlayerController::FromSlot(pSlots[i]);
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Targeting %s", pTarget->GetPlayerName());
		Message("Targeting %s\n", pTarget->GetPlayerName());
	}
}

CON_COMMAND_CHAT(setorigin, "<vector> - Set your origin")
{
	if (!player)
		return;

	CBasePlayerPawn *pPawn = player->GetPawn();
	Vector vecNewOrigin;
	V_StringToVector(args.ArgS(), vecNewOrigin);

	pPawn->Teleport(&vecNewOrigin, nullptr, nullptr);

	ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Your origin is now %f %f %f", vecNewOrigin.x, vecNewOrigin.y, vecNewOrigin.z);
}

CON_COMMAND_CHAT(particle, "- Spawn a particle")
{
	if (!player)
		return;

	Vector vecAbsOrigin = player->GetPawn()->GetAbsOrigin();
	vecAbsOrigin.z += 64.0f;

	CParticleSystem *particle = CreateEntityByName<CParticleSystem>("info_particle_system");

	particle->m_bStartActive(true);
	particle->m_iszEffectName(args[1]);
	particle->Teleport(&vecAbsOrigin, nullptr, nullptr);

	particle->DispatchSpawn();

	ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "You have spawned a particle with effect name: %s", particle->m_iszEffectName().String());
	Message("You have spawned a particle with effect name: %s\n", particle->m_iszEffectName().String());
}

CON_COMMAND_CHAT(particle_kv, "- Spawn a particle but using keyvalues to spawn")
{
	if (!player)
		return;

	Vector vecAbsOrigin = player->GetPawn()->GetAbsOrigin();
	vecAbsOrigin.z += 64.0f;

	CParticleSystem *particle = CreateEntityByName<CParticleSystem>("info_particle_system");

	CEntityKeyValues *pKeyValues = new CEntityKeyValues();

	pKeyValues->SetString("effect_name", args[1]);
	pKeyValues->SetBool("start_active", true);
	pKeyValues->SetVector("origin", vecAbsOrigin);

	particle->DispatchSpawn(pKeyValues);

	ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "You have spawned a particle using keyvalues with effect name: %s", particle->m_iszEffectName().String());
	Message("You have spawned a particle using keyvalues with effect name: %s\n", particle->m_iszEffectName().String());
}

CON_COMMAND_CHAT(dispatch_particle, "- Test")
{
	if (!player)
		return;

	CRecipientFilter filter;
	filter.AddAllPlayers();

	player->GetPawn()->DispatchParticle(args[1], &filter);
}

CON_COMMAND_CHAT(emitsound, "- Emit a sound from the entity under crosshair")
{
	if (!player)
		return;

	CBaseEntity *pEntity = UTIL_FindPickerEntity(player);

	if (!pEntity)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "No entity found");
		return;
	}

	pEntity->EmitSound(args[1]);

	ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Playing %s on %s", args[1], pEntity->GetClassname());
	Message("Playing %s on %s", args[1], pEntity->GetClassname());
}

CON_COMMAND_CHAT(getstats, "- Get your stats")
{
	if (!player)
		return;

	CSMatchStats_t *stats = &player->m_pActionTrackingServices->m_matchStats();

	ClientPrint(player, HUD_PRINTCENTER, 
		"Kills: %i\n"
		"Deaths: %i\n"
		"Assists: %i\n"
		"Damage: %i"
		, stats->m_iKills.Get(), stats->m_iDeaths.Get(), stats->m_iAssists.Get(), stats->m_iDamage.Get());

	ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Kills: %d", stats->m_iKills.Get());
	ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Deaths: %d", stats->m_iDeaths.Get());
	ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Assists: %d", stats->m_iAssists.Get());
	ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Damage: %d", stats->m_iDamage.Get());
}

CON_COMMAND_CHAT(setkills, "- Set your kills")
{
	if (!player)
		return;

	player->m_pActionTrackingServices->m_matchStats().m_iKills = atoi(args[1]);

	ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "You have set your kills to %d.", atoi(args[1]));
}

CON_COMMAND_CHAT(setcollisiongroup, "<group> - Set a player's collision group")
{
	int iNumClients = 0;
	int pSlots[MAXPLAYERS];

	if (!g_playerManager->CanTargetPlayers(player, args[1], iNumClients, pSlots, NO_TARGET_BLOCKS))
		return;

	for (int i = 0; i < iNumClients; i++)
	{
		CCSPlayerController* pTarget = CCSPlayerController::FromSlot(pSlots[i]);
		uint8 group = atoi(args[2]);
		uint8 oldgroup = pTarget->m_hPawn->m_pCollision->m_CollisionGroup;

		pTarget->m_hPawn->m_pCollision->m_CollisionGroup = group;
		pTarget->m_hPawn->m_pCollision->m_collisionAttribute().m_nCollisionGroup = group;
		pTarget->GetPawn()->CollisionRulesChanged();

		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Setting collision group on %s from %d to %d.", pTarget->GetPlayerName(), oldgroup, group);
	}
}

CON_COMMAND_CHAT(setsolidtype, "<solidtype> - Set a player's solid type")
{
	int iNumClients = 0;
	int pSlots[MAXPLAYERS];

	if (!g_playerManager->CanTargetPlayers(player, args[1], iNumClients, pSlots, NO_TARGET_BLOCKS))
		return;

	for (int i = 0; i < iNumClients; i++)
	{
		CCSPlayerController* pTarget = CCSPlayerController::FromSlot(pSlots[i]);

		uint8 type = atoi(args[2]);
		uint8 oldtype = pTarget->m_hPawn->m_pCollision->m_nSolidType;

		pTarget->m_hPawn->m_pCollision->m_nSolidType = (SolidType_t)type;
		pTarget->GetPawn()->CollisionRulesChanged();

		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Setting solid type on %s from %d to %d.", pTarget->GetPlayerName(), oldtype, type);
	}
}

CON_COMMAND_CHAT(setinteraction, "<flags> - Set a player's interaction flags")
{
	int iNumClients = 0;
	int pSlots[MAXPLAYERS];

	if (!g_playerManager->CanTargetPlayers(player, args[1], iNumClients, pSlots, NO_TARGET_BLOCKS))
		return;

	for (int i = 0; i < iNumClients; i++)
	{
		CCSPlayerController* pTarget = CCSPlayerController::FromSlot(pSlots[i]);

		uint64 oldInteractAs = pTarget->m_hPawn->m_pCollision->m_collisionAttribute().m_nInteractsAs;
		uint64 newInteract = oldInteractAs | ((uint64)1 << 53);

		pTarget->m_hPawn->m_pCollision->m_collisionAttribute().m_nInteractsAs = newInteract;
		pTarget->m_hPawn->m_pCollision->m_collisionAttribute().m_nInteractsExclude = newInteract;
		pTarget->GetPawn()->CollisionRulesChanged();

		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Setting interaction flags on %s from %llx to %llx.", pTarget->GetPlayerName(), oldInteractAs, newInteract);
	}
}

void HttpCallback(HTTPRequestHandle request, json response)
{
	ClientPrintAll(HUD_PRINTTALK, response.dump().c_str());
}

CON_COMMAND_CHAT(http, "<get/post> <url> [content] - Test an HTTP request")
{
	if (!g_http)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Steam HTTP interface is not available!");
		return;
	}
	if (args.ArgC() < 3)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Usage: !http <get/post> <url> [content]");
		return;
	}

	if (!V_strcmp(args[1], "get"))
		g_HTTPManager.GET(args[2], &HttpCallback);
	else if (!V_strcmp(args[1], "post"))
		g_HTTPManager.POST(args[2], args[3], &HttpCallback);
}

CON_COMMAND_CHAT(discordbot, "<bot> <message> - Send a message to a discord webhook")
{
	if (args.ArgC() < 3)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Usage: !discord <bot> <message>");
		return;
	}

	g_pDiscordBotManager->PostDiscordMessage(args[1], args[2]);
}
#endif // _DEBUG
