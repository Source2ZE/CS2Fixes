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

#include "commands.h"
#include "playermanager.h"
#include "ctimer.h"
#include "eventlistener.h"
#include "zombiereborn.h"
#include "entity/cgamerules.h"

#include "tier0/memdbgon.h"

extern CEntitySystem* g_pEntitySystem;
extern IVEngineServer2* g_pEngineServer2;
extern CGlobalVars* gpGlobals;
extern CCSGameRules* g_pGameRules;
extern IGameEventManager2* g_gameEventManager;

void ZR_Infect(CCSPlayerController *pAttackerController, CCSPlayerController *pVictimController, bool bBroadcast);

EZRRoundState g_ZRRoundState = EZRRoundState::ROUND_START;
bool g_bEnableZR = false;

CON_COMMAND_F(zr_enable, "Whether to enable ZR features", FCVAR_LINKED_CONCOMMAND | FCVAR_SPONLY)
{
	if (args.ArgC() < 2)
		Msg("%s %i\n", args[0], g_bEnableZR);
	else
		g_bEnableZR = V_StringToBool(args[1], false);
}

CON_COMMAND_CHAT(zspawn, "respawn yourself")
{
	// Silently return so the command is completely hidden
	if (!g_bEnableZR)
		return;

	if (!player)
	{
		ClientPrint(player, HUD_PRINTCONSOLE, CHAT_PREFIX "You cannot use this command from the server console.");
		return;
	}

	CCSPlayerPawn *pPawn = (CCSPlayerPawn*)player->GetPawn();
	if (!pPawn)
	{
		ClientPrint(player, HUD_PRINTCONSOLE, CHAT_PREFIX "Invalid Pawn.");
		return;
	}
	ClientPrint(player, HUD_PRINTCONSOLE, CHAT_PREFIX "Classname: %s", pPawn->GetClassname());
	if (pPawn->IsAlive())
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX"You must be dead to respawn!");
		return;
	}

	player->Respawn();
	pPawn->Respawn();
}

void SetUpAllHumanClasses()
{
	for (int i = 0; i < gpGlobals->maxClients; i++)
	{
		ZEPlayer* pPlayer = g_playerManager->GetPlayer(i);

		//if (pPlayer)
		//	InjectPlayerClass(PickRandomHumanDefaultClass(), pPlayer);
	}
}

void ZR_OnStartupServer()
{
	g_ZRRoundState = EZRRoundState::ROUND_START;
	// CONVAR_TODO
	// Here we force some cvars that are essential for the scripts to work
	g_pEngineServer2->ServerCommand("mp_weapons_allow_pistols 3");
	g_pEngineServer2->ServerCommand("mp_weapons_allow_smgs 3");
	g_pEngineServer2->ServerCommand("mp_weapons_allow_heavy 3");
	g_pEngineServer2->ServerCommand("mp_weapons_allow_rifles 3");
	g_pEngineServer2->ServerCommand("mp_give_player_c4 0");
	g_pEngineServer2->ServerCommand("mp_friendlyfire 0");
	// Legacy Lua respawn stuff, do not use these, we should handle respawning ourselves now that we can
	//g_pEngineServer2->ServerCommand("mp_respawn_on_death_t 1");
	//g_pEngineServer2->ServerCommand("mp_respawn_on_death_ct 1");
	//g_pEngineServer2->ServerCommand("bot_quota_mode fill");
	//g_pEngineServer2->ServerCommand("mp_ignore_round_win_conditions 1");
}

void ZR_OnRoundPrestart(IGameEvent* pEvent)
{
	g_ZRRoundState = EZRRoundState::ROUND_START;
}

void ZR_OnRoundStart(IGameEvent* pEvent)
{
	ClientPrintAll(HUD_PRINTTALK, ZR_PREFIX "The game is \x05Humans vs. Zombies\x01, the goal for zombies is to infect all humans by knifing them.");

	new CTimer(1.0f, false, []()
	{
		g_ZRRoundState = EZRRoundState::POST_INFECTION;
		return 1.0f;
	});

	// SetUpAllHumanClasses();
	// SetupRespawnToggler();
	// SetupAmmoReplenish();

	// g_ZR_ROUND_STARTED = true;
}

void ZR_OnPlayerSpawn(IGameEvent* pEvent)
{
	CCSPlayerController* pController = (CCSPlayerController*)pEvent->GetPlayerController("userid");

	if (pController && g_ZRRoundState == EZRRoundState::POST_INFECTION && pController->m_iTeamNum == CS_TEAM_CT)
		ZR_Infect(pController, pController, false); //set health and probably model doesn't work here
}

// CONVAR_TODO
float g_flKnockbackScale = 5.0f;
CON_COMMAND_F(zr_knockback_scale, "Global knockback scale", FCVAR_LINKED_CONCOMMAND | FCVAR_SPONLY)
{
	if (args.ArgC() < 2)
		Msg("%s %f\n", args[0], g_flKnockbackScale);
	else
		g_flKnockbackScale = V_StringToFloat32(args[1], 5.0f);
}

// Still need to implement weapon config
void ZR_ApplyKnockback(CCSPlayerPawn *pHuman, CCSPlayerPawn *pVictim, int iDamage, const char *szWeapon)
{
	Vector vecKnockback;
	AngleVectors(pHuman->m_angEyeAngles(), &vecKnockback);
	vecKnockback *= (iDamage * g_flKnockbackScale);
	//Message("%f %f %f\n",vecKnockback.x, vecKnockback.y, vecKnockback.z);
	pVictim->m_vecBaseVelocity = pVictim->m_vecBaseVelocity() + vecKnockback;
}

void ZR_ApplyKnockbackExplosion(Z_CBaseEntity *pProjectile, CCSPlayerPawn *pVictim, int iDamage)
{
	Vector vecDisplacement = pVictim->GetAbsOrigin() - pProjectile->GetAbsOrigin();
	vecDisplacement.z += 36;
	VectorNormalize(vecDisplacement);
	Vector vecKnockback = vecDisplacement;
	vecKnockback *= (iDamage * g_flKnockbackScale);
	pVictim->m_vecBaseVelocity = pVictim->m_vecBaseVelocity() + vecKnockback;
}

void ZR_FakePlayerDeath(CCSPlayerController *pAttackerController, CCSPlayerController *pVictimController, const char *szWeapon)
{
	IGameEvent *pEvent = g_gameEventManager->CreateEvent("player_death");

	if (!pEvent)
		return;
	//SetPlayer functions are swapped, need to remove the cast once fixed
	pEvent->SetPlayer("userid", (CEntityInstance*)pVictimController->GetPlayerSlot());
	pEvent->SetPlayer("attacker", (CEntityInstance*)pAttackerController->GetPlayerSlot());
	pEvent->SetString("weapon", szWeapon);

	Message("\n--------------------------------\n"
		"userid: %i\n"
		"userid_pawn: %i\n"
		"attacker: %i\n"
		"attacker_pawn: %i\n"
		"weapon: %s\n"
		"--------------------------------\n",
		pEvent->GetInt("userid"),
		pEvent->GetInt("userid_pawn"),
		pEvent->GetInt("attacker"),
		pEvent->GetInt("attacker_pawn"),
		pEvent->GetString("weapon"));

	g_gameEventManager->FireEvent(pEvent, false);

}

void ZR_Infect(CCSPlayerController *pAttackerController, CCSPlayerController *pVictimController, bool bBroadcast)
{
	if (bBroadcast)
		ZR_FakePlayerDeath(pAttackerController, pVictimController, "knife"); // or any other killicon
	pVictimController->SwitchTeam(CS_TEAM_T);

	CCSPlayerPawn *pVictimPawn = (CCSPlayerPawn*)pVictimController->GetPawn();
	if (!pVictimPawn)
		return;
	//set model/health/etc here
	
	pVictimPawn->m_iMaxHealth = 10000;
	pVictimPawn->m_iHealth = 10000;
}


void ZR_InfectMotherZombie(CCSPlayerController *pVictimController)
{
	ZR_FakePlayerDeath(pVictimController, pVictimController, "prop_exploding_barrel"); // or any other killicon
	pVictimController->SwitchTeam(CS_TEAM_T);

	//set model/health/etc here
}


bool ZR_OnTakeDamageDetour(CCSPlayerPawn *pAttackerPawn, CCSPlayerPawn *pVictimPawn, CTakeDamageInfo *pInfo)
{
	CCSPlayerController *pAttackerController = CCSPlayerController::FromPawn(pAttackerPawn);
	CCSPlayerController *pVictimController = CCSPlayerController::FromPawn(pVictimPawn);

	if (pAttackerPawn->m_iTeamNum() == CS_TEAM_T && pVictimPawn->m_iTeamNum() == CS_TEAM_CT)
	{
		ZR_Infect(pAttackerController, pVictimController, true);
		return true; // nullify the damage
	}

	//grenade and molotov knockback
	if (pAttackerPawn->m_iTeamNum() == CS_TEAM_CT && pVictimPawn->m_iTeamNum() == CS_TEAM_T)
	{
		CBaseEntity *pInflictor = pInfo->m_hInflictor.Get();
		const char *pszInflictorClass = pInflictor ? pInflictor->GetClassname() : "";
		if (V_strncmp(pszInflictorClass, "hegrenade", 9) || V_strncmp(pszInflictorClass, "inferno", 7))
			ZR_ApplyKnockbackExplosion((Z_CBaseEntity*)pInflictor, (CCSPlayerPawn*)pVictimPawn, (int)pInfo->m_flDamage);
	}
	return false;
}

void ZR_OnPlayerHurt(IGameEvent* pEvent)
{
	CCSPlayerController *pAttackerController = (CCSPlayerController*)pEvent->GetPlayerController("attacker");
	CCSPlayerController *pVictimController = (CCSPlayerController*)pEvent->GetPlayerController("userid");
	const char* szWeapon = pEvent->GetString("weapon");
	int iDmgHealth = pEvent->GetInt("dmg_health");


	//grenade and molotov knockbacks are handled by TakeDamage detours
	if (!pAttackerController || !pVictimController || strcmp(szWeapon, "") == 0 || strcmp(szWeapon, "inferno") == 0 || strcmp(szWeapon, "hegrenade") == 0)
		return;

	if (pAttackerController->m_iTeamNum() == CS_TEAM_CT && pVictimController->m_iTeamNum() == CS_TEAM_T)
		ZR_ApplyKnockback((CCSPlayerPawn*)pAttackerController->GetPawn(), (CCSPlayerPawn*)pVictimController->GetPawn(), iDmgHealth, szWeapon);

	//if (pAttacker->m_iTeamNum == CS_TEAM_CT && pVictim->m_iTeamNum == CS_TEAM_T)
	//	Message("lol");
	//	//Knockback_Apply(pAttacker, pVictim, iDmgHealth, szWeapon);
	//else if (szWeapon == "knife" && pAttacker->m_iTeamNum == CS_TEAM_T && pVictim->m_iTeamNum == CS_TEAM_CT)
	//	Message("lol");
	//	//Infect(pAttacker, pVictim, true, iHealth == 0);
}

void ZR_OnPlayerDeath(IGameEvent* pEvent)
{
	// To T TeamSwitch happening in ZR_OnPlayerSpawn

	/*ZEPlayer* pPlayer = g_playerManager->GetPlayerFromUserId(pEvent->GetInt("userid"));

	if (pPlayer && !pPlayer->IsInfected())
	{
		CCSPlayerController* pController = (CCSPlayerController*)pEvent->GetPlayerController("userid");
		pController->SwitchTeam(CS_TEAM_T);
	}*/
}