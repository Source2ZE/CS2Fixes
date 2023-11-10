/**
 * =============================================================================
 * CS2Fixes
 * Copyright (C) 2023 Source2ZE
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
#include "playermanager.h"
#include "adminsystem.h"
#include "ctimer.h"
#include "httpmanager.h"
#undef snprintf
#include "vendor/nlohmann/json.hpp"

#include "tier0/memdbgon.h"

using json = nlohmann::json;

extern CEntitySystem *g_pEntitySystem;
extern IVEngineServer2* g_pEngineServer2;
extern ISteamHTTP* g_http;

WeaponMapEntry_t WeaponMap[] = {
	{"bizon",		  "weapon_bizon",			 1400, 26},
	{"mac10",		  "weapon_mac10",			 1400, 27},
	{"mp7",			"weapon_mp7",				 1700, 23},
	{"mp9",			"weapon_mp9",				 1250, 34},
	{"p90",			"weapon_p90",				 2350, 19},
	{"ump45",		  "weapon_ump45",			 1700, 24},
	{"ak47",			 "weapon_ak47",			 2500, 7},
	{"aug",			"weapon_aug",				 3500, 8},
	{"famas",		  "weapon_famas",			 2250, 10},
	{"galilar",		"weapon_galilar",			 2000, 13},
	{"m4a4",			 "weapon_m4a1",			 3100, 16},
	{"m4a1",			 "weapon_m4a1_silencer", 3100, 60},
	{"sg556",		  "weapon_sg556",			 3500, 39},
	{"awp",			"weapon_awp",				 4750, 9},
	{"g3sg1",		  "weapon_g3sg1",			 5000, 11},
	{"scar20",		   "weapon_scar20",			 5000, 38},
	{"ssg08",		  "weapon_ssg08",			 2500, 40},
	{"mag7",			 "weapon_mag7",			 2000, 29},
	{"nova",			 "weapon_nova",			 1500, 35},
	{"sawedoff",		 "weapon_sawedoff",		 1500, 29},
	{"xm1014",		   "weapon_xm1014",			 3000, 25},
	{"m249",			 "weapon_m249",			 5750, 14},
	{"negev",		  "weapon_negev",			 5750, 28},
	{"deagle",		   "weapon_deagle",			 700 , 1},
	{"elite",		  "weapon_elite",			 800 , 2},
	{"fiveseven",	  "weapon_fiveseven",		 500 , 3},
	{"glock",		  "weapon_glock",			 200 , 4},
	{"hkp2000",		"weapon_hkp2000",			 200 , 32},
	{"p250",			 "weapon_p250",			 300 , 36},
	{"tec9",			 "weapon_tec9",			 500 , 30},
	{"usp_silencer",	 "weapon_usp_silencer",	 200 , 61},
	{"cz75a",		  "weapon_cz75a",			 500 , 63},
	{"revolver",		 "weapon_revolver",		 600 , 64},
	{"he",			"weapon_hegrenade",			 300 , 44, 1},
	{"molotov",		"weapon_molotov",			 850 , 46, 1},
	{"knife",		"weapon_knife",				 0	 , 42},	// default CT knife
	{"kevlar",		   "item_kevlar",			 600 , 50},
};

void ParseWeaponCommand(CCSPlayerController *pController, const char *pszWeaponName)
{
	if (!pController || !pController->m_hPawn())
		return;

	CCSPlayerPawn* pPawn = (CCSPlayerPawn*)pController->GetPawn();

	for (int i = 0; i < sizeof(WeaponMap) / sizeof(*WeaponMap); i++)
	{
		WeaponMapEntry_t weaponEntry = WeaponMap[i];

		if (!V_stricmp(pszWeaponName, weaponEntry.command))
		{
			if (pController->m_hPawn()->m_iHealth() <= 0) {
				ClientPrint(pController, HUD_PRINTTALK, CHAT_PREFIX"You can only buy weapons when alive.");
				return;
			}
			CCSPlayer_ItemServices *pItemServices = pPawn->m_pItemServices;
			int money = pController->m_pInGameMoneyServices->m_iAccount;
			if (money >= weaponEntry.iPrice)
			{
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
								ClientPrint(pController, HUD_PRINTTALK, CHAT_PREFIX"You cannot use !%s anymore(Max %i)", weaponEntry.command, weaponEntry.maxAmount);
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

				pController->m_pInGameMoneyServices->m_iAccount = money - weaponEntry.iPrice;
				pItemServices->GiveNamedItem(weaponEntry.szWeaponName);
			}

			break;
		}
	}
}

void ParseChatCommand(const char *pMessage, CCSPlayerController *pController)
{
	if (!pController || !pController->IsConnected())
		return;

	CCommand args;
	args.Tokenize(pMessage + 1);

	uint16 index = g_CommandList.Find(hash_32_fnv1a_const(args[0]));

	if (g_CommandList.IsValidIndex(index))
	{
		(*g_CommandList[index])(args, pController);
	}
	else
	{
		ParseWeaponCommand(pController, args[0]);
	}
}

bool CChatCommand::CheckCommandAccess(CBasePlayerController *pPlayer, uint64 flags)
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

	addresses::UTIL_ClientPrintAll(hud_dest, buf, nullptr, nullptr, nullptr, nullptr);
	ConMsg("%s\n", buf);
}

void ClientPrint(CBasePlayerController *player, int hud_dest, const char *msg, ...)
{
	va_list args;
	va_start(args, msg);

	char buf[256];
	V_vsnprintf(buf, sizeof(buf), msg, args);

	va_end(args);

	if (player)
		addresses::ClientPrint(player, hud_dest, buf, nullptr, nullptr, nullptr, nullptr);
	else
		ConMsg("%s\n", buf);
}

CON_COMMAND_CHAT(stopsound, "toggle weapon sounds")
{
	if (!player)
		return;

	int iPlayer = player->GetPlayerSlot();
	bool bStopSet = g_playerManager->IsPlayerUsingStopSound(iPlayer);
	bool bSilencedSet = g_playerManager->IsPlayerUsingSilenceSound(iPlayer);

	g_playerManager->SetPlayerStopSound(iPlayer, bSilencedSet);
	g_playerManager->SetPlayerSilenceSound(iPlayer, !bSilencedSet && !bStopSet);

	ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "You have %s weapon sounds.", bSilencedSet ? "disabled" : !bSilencedSet && !bStopSet ? "silenced" : "enabled");
}

CON_COMMAND_CHAT(toggledecals, "toggle world decals, if you're into having 10 fps in ZE")
{
	if (!player)
		return;

	int iPlayer = player->GetPlayerSlot();
	bool bSet = !g_playerManager->IsPlayerUsingStopDecals(iPlayer);

	g_playerManager->SetPlayerStopDecals(iPlayer, bSet);

	ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "You have %s world decals.", bSet ? "disabled" : "enabled");
}

CON_COMMAND_CHAT(myuid, "test")
{
	if (!player)
		return;

	int iPlayer = player->GetPlayerSlot();

	ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Your userid is %i, slot: %i, retrieved slot: %i", g_pEngineServer2->GetPlayerUserId(iPlayer).Get(), iPlayer, g_playerManager->GetSlotFromUserId(g_pEngineServer2->GetPlayerUserId(iPlayer).Get()));
}

// CONVAR_TODO
static constexpr float g_flMaxZteleDistance = 150.0f;

CON_COMMAND_CHAT(ztele, "teleport to spawn")
{
	if (!player)
	{
		ClientPrint(player, HUD_PRINTCONSOLE, CHAT_PREFIX "You cannot use this command from the server console.");
		return;
	}

	//Count spawnpoints (info_player_counterterrorist & info_player_terrorist)
	SpawnPoint* spawn = nullptr;
	CUtlVector<SpawnPoint*> spawns;
	while (nullptr != (spawn = (SpawnPoint*)UTIL_FindEntityByClassname(spawn, "info_player_*")))
	{
		if (spawn->m_bEnabled())
			spawns.AddToTail(spawn);
	}

	// Let's be real here, this should NEVER happen
	// But I ran into this when I switched to the real FindEntityByClassname and forgot to insert a *
	if (spawns.Count() == 0)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX"There are no spawns!");
		Panic("ztele used while there are no spawns!\n");
		return;
	}

	//Pick and get position of random spawnpoint
	int randomindex = rand() % spawns.Count();
	Vector spawnpos = spawns[randomindex]->GetAbsOrigin();

	//Here's where the mess starts
	CBasePlayerPawn* pPawn = player->GetPawn();

	if (!pPawn)
		return;

	if (!pPawn->IsAlive())
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX"You cannot teleport when dead!");
		return;
	}

	//Get initial player position so we can do distance check
	Vector initialpos = pPawn->GetAbsOrigin();

	ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX"Teleporting to spawn in 5 seconds.");

	CHandle<CBasePlayerPawn> handle = pPawn->GetHandle();

	new CTimer(5.0f, false, [spawnpos, handle, initialpos]()
	{
		CBasePlayerPawn *pPawn = handle.Get();

		if (!pPawn)
			return -1.0f;

		Vector endpos = pPawn->GetAbsOrigin();

		if (initialpos.DistTo(endpos) < g_flMaxZteleDistance)
		{
			pPawn->SetAbsOrigin(spawnpos);
			ClientPrint(pPawn->GetController(), HUD_PRINTTALK, CHAT_PREFIX "You have been teleported to spawn.");
		}
		else
		{
			ClientPrint(pPawn->GetController(), HUD_PRINTTALK, CHAT_PREFIX "Teleport failed! You moved too far.");
			return -1.0f;
		}
		
		return -1.0f;
	});
}

// CONVAR_TODO
static constexpr int g_iDefaultHideDistance = 250;
static constexpr int g_iMaxHideDistance = 2000;

CON_COMMAND_CHAT(hide, "hides nearby teammates")
{
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

	int iPlayer = player->GetPlayerSlot();

	ZEPlayer *pZEPlayer = g_playerManager->GetPlayer(iPlayer);

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
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Hiding teammates is now disabled.");
	else
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Now hiding teammates within %i units.", distance);
}


#if _DEBUG
CON_COMMAND_CHAT(message, "message someone")
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

	char buf[256];
	V_snprintf(buf, sizeof(buf), CHAT_PREFIX"Private message from %s to %s: \5%s", player->GetPlayerName(), pTarget->GetPlayerName(), pMessage);

	CSingleRecipientFilter filter(uid);

	UTIL_SayTextFilter(filter, buf, nullptr, 0);
}

CON_COMMAND_CHAT(say, "say something using console")
{
	ClientPrintAll(HUD_PRINTTALK, "%s", args.ArgS());
}

CON_COMMAND_CHAT(takemoney, "take your money")
{
	if (!player)
		return;

	int amount = atoi(args[1]);
	int money = player->m_pInGameMoneyServices->m_iAccount;

	player->m_pInGameMoneyServices->m_iAccount = money - amount;
}

CON_COMMAND_CHAT(sethealth, "set your health")
{
	if (!player)
		return;

	int health = atoi(args[1]);

	Z_CBaseEntity *pEnt = (Z_CBaseEntity *)player->GetPawn();

	pEnt->m_iHealth = health;

	ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX"Your health is now %d", health);
}

CON_COMMAND_CHAT(test_target, "test string targetting")
{
	if (!player)
		return;

	int iCommandPlayer = player->GetPlayerSlot();
	int iNumClients = 0;
	int pSlots[MAXPLAYERS];

	g_playerManager->TargetPlayerString(iCommandPlayer, args[1], iNumClients, pSlots);

	for (int i = 0; i < iNumClients; i++)
	{
		CCSPlayerController* pTarget = CCSPlayerController::FromSlot(pSlots[i]);

		if (!pTarget)
			continue;

		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX"Targeting %s", pTarget->GetPlayerName());
		Message("Targeting %s\n", pTarget->GetPlayerName());
	}
}

CON_COMMAND_CHAT(getorigin, "get your origin")
{
	if (!player)
		return;

	Vector vecAbsOrigin = player->GetPawn()->GetAbsOrigin();

	ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX"Your origin is %f %f %f", vecAbsOrigin.x, vecAbsOrigin.y, vecAbsOrigin.z);
}

CON_COMMAND_CHAT(setorigin, "set your origin")
{
	if (!player)
		return;

	CBasePlayerPawn *pPawn = player->GetPawn();
	Vector vecNewOrigin;
	V_StringToVector(args.ArgS(), vecNewOrigin);

	pPawn->SetAbsOrigin(vecNewOrigin);

	ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX"Your origin is now %f %f %f", vecNewOrigin.x, vecNewOrigin.y, vecNewOrigin.z);
}

CON_COMMAND_CHAT(getstats, "get your stats")
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

	ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX"Kills: %d", stats->m_iKills.Get());
	ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX"Deaths: %d", stats->m_iDeaths.Get());
	ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX"Assists: %d", stats->m_iAssists.Get());
	ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX"Damage: %d", stats->m_iDamage.Get());
}

CON_COMMAND_CHAT(setkills, "set your kills")
{
	if (!player)
		return;

	player->m_pActionTrackingServices->m_matchStats().m_iKills = atoi(args[1]);

	ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX"You have set your kills to %d.", atoi(args[1]));
}

CON_COMMAND_CHAT(setcollisiongroup, "set a player's collision group")
{
	int iNumClients = 0;
	int pSlots[MAXPLAYERS];

	g_playerManager->TargetPlayerString(player->GetPlayerSlot(), args[1], iNumClients, pSlots);

	for (int i = 0; i < iNumClients; i++)
	{
		CCSPlayerController* pTarget = CCSPlayerController::FromSlot(pSlots[i]);

		if (!pTarget)
			continue;

		uint8 group = atoi(args[2]);
		uint8 oldgroup = pTarget->m_hPawn->m_pCollision->m_CollisionGroup;

		pTarget->m_hPawn->m_pCollision->m_CollisionGroup = group;
		pTarget->m_hPawn->m_pCollision->m_collisionAttribute().m_nCollisionGroup = group;
		pTarget->GetPawn()->CollisionRulesChanged();

		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Setting collision group on %s from %d to %d.", pTarget->GetPlayerName(), oldgroup, group);
	}
}

CON_COMMAND_CHAT(setsolidtype, "set a player's solid type")
{
	int iNumClients = 0;
	int pSlots[MAXPLAYERS];

	g_playerManager->TargetPlayerString(player->GetPlayerSlot(), args[1], iNumClients, pSlots);

	for (int i = 0; i < iNumClients; i++)
	{
		CCSPlayerController* pTarget = CCSPlayerController::FromSlot(pSlots[i]);

		if (!pTarget)
			continue;

		uint8 type = atoi(args[2]);
		uint8 oldtype = pTarget->m_hPawn->m_pCollision->m_nSolidType;

		pTarget->m_hPawn->m_pCollision->m_nSolidType = (SolidType_t)type;
		pTarget->GetPawn()->CollisionRulesChanged();

		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Setting solid type on %s from %d to %d.", pTarget->GetPlayerName(), oldtype, type);
	}
}

CON_COMMAND_CHAT(setinteraction, "set a player's interaction flags")
{
	int iNumClients = 0;
	int pSlots[MAXPLAYERS];

	g_playerManager->TargetPlayerString(player->GetPlayerSlot(), args[1], iNumClients, pSlots);

	for (int i = 0; i < iNumClients; i++)
	{
		CCSPlayerController* pTarget = CCSPlayerController::FromSlot(pSlots[i]);

		if (!pTarget)
			continue;

		uint64 oldInteractAs = pTarget->m_hPawn->m_pCollision->m_collisionAttribute().m_nInteractsAs;
		uint64 newInteract = oldInteractAs | ((uint64)1 << 53);

		pTarget->m_hPawn->m_pCollision->m_collisionAttribute().m_nInteractsAs = newInteract;
		pTarget->m_hPawn->m_pCollision->m_collisionAttribute().m_nInteractsExclude = newInteract;
		pTarget->GetPawn()->CollisionRulesChanged();

		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Setting interaction flags on %s from %llx to %llx.", pTarget->GetPlayerName(), oldInteractAs, newInteract);
	}
}

void HttpCallback(HTTPRequestHandle request, char* response)
{
	// Test serializing to JSON
	json data = json::parse(response, nullptr, false);

	if (data.is_discarded())
	{
		Message("Failed parsing JSON!\n");
		return;
	}

	ClientPrintAll(HUD_PRINTTALK, data.dump().c_str());
}

CON_COMMAND_CHAT(http, "test an HTTP request")
{
	if (!g_http)
	{
		if (player)
			ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Steam HTTP interface is not available!");
		else
			Message("Steam HTTP interface is not available!\n");

		return;
	}
	if (args.ArgC() < 3)
	{
		if (player)
			ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Usage: !http <get/post> <url> [content]");
		else
			Message("Usage: !http <get/post> <url> [content]\n");

		return;
	}

	if (strcmp(args[1], "get") == 0)
		g_HTTPManager.GET(args[2], &HttpCallback);
	else if (strcmp(args[1], "post") == 0)
		g_HTTPManager.POST(args[2], args[3], &HttpCallback);
}
#endif // _DEBUG

// Lookup a weapon classname in the weapon map and "initialize" it.
// Both m_bInitialized and m_iItemDefinitionIndex need to be set for a weapon to be pickable and not crash clients,
// and m_iItemDefinitionIndex needs to be the correct ID from weapons.vdata so the gun behaves as it should.
void FixWeapon(CCSWeaponBase *pWeapon)
{
	// Weapon could be already initialized with the correct data from GiveNamedItem, in that case we don't need to do anything
	if (!pWeapon || pWeapon->m_AttributeManager().m_Item().m_bInitialized())
		return;

	const char *pszClassName = pWeapon->m_pEntity->m_designerName.String();

	for (int i = 0; i < sizeof(WeaponMap) / sizeof(*WeaponMap); i++)
	{
		if (!V_stricmp(WeaponMap[i].szWeaponName, pszClassName))
		{
			DevMsg("Fixing a %s with index = %d and initialized = %d\n", pszClassName,
				pWeapon->m_AttributeManager().m_Item().m_iItemDefinitionIndex(),
				pWeapon->m_AttributeManager().m_Item().m_bInitialized());

			pWeapon->m_AttributeManager().m_Item().m_bInitialized = true;
			pWeapon->m_AttributeManager().m_Item().m_iItemDefinitionIndex = WeaponMap[i].iItemDefIndex;
		}
	}
}
