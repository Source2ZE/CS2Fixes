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
#include "utils/entity.h"
#include "playermanager.h"
#include "ctimer.h"
#include "eventlistener.h"
#include "zombiereborn.h"
#include "entity/cgamerules.h"
#include "utils/entity.h"
#include "entity/services.h"
#include "entity/cteam.h"

#include "tier0/memdbgon.h"

extern CEntitySystem* g_pEntitySystem;
extern IVEngineServer2* g_pEngineServer2;
extern CGlobalVars* gpGlobals;
extern CCSGameRules* g_pGameRules;
extern IGameEventManager2* g_gameEventManager;

void ZR_Infect(CCSPlayerController *pAttackerController, CCSPlayerController *pVictimController, bool bBroadcast);
void ZR_CheckWinConditions(bool bCheckCT);
void ZR_Cure(CCSPlayerController *pTargetController);

EZRRoundState g_ZRRoundState = EZRRoundState::ROUND_START;
static int g_iRoundNum = 0;
static int g_iInfectionCountDown = 0;


// CONVAR_TODO
bool g_bEnableZR = false;
static float g_flMaxZteleDistance = 150.0f;
static bool g_bZteleHuman = false;
static float g_flKnockbackScale = 5.0f;
static int g_flInfectSpawnType = EZRSpawnType::RESPAWN;
static int g_flInfectSpawnTimeMin = 15;
static int g_flInfectSpawnTimeMax = 15;
static int g_flInfectSpawnMZRatio = 7;
static int g_flInfectSpawnMinCount = 2;

CON_ZR_CVAR(zr_enable, "Whether to enable ZR features", g_bEnableZR, Bool, false)
CON_ZR_CVAR(zr_ztele_max_distance, "Maximum distance players are allowed to move after starting ztele", g_flMaxZteleDistance, Float32, 150.0f)
CON_ZR_CVAR(zr_ztele_allow_humans, "Whether to allow humans to use ztele", g_bZteleHuman, Bool, false)
CON_ZR_CVAR(zr_knockback_scale, "Global knockback scale", g_flKnockbackScale, Float32, 5.0f)
CON_ZR_CVAR(zr_infect_spawn_type, "Type of Mother Zombies Spawn [0 = MZ spawn where they stand, 1 = MZ get teleported back to spawn on being picked]", g_flInfectSpawnType, Int32, EZRSpawnType::RESPAWN)
CON_ZR_CVAR(zr_infect_spawn_time_min, "Minimum time in which Mother Zombies should be picked, after round start", g_flInfectSpawnTimeMin, Int32, 15)
CON_ZR_CVAR(zr_infect_spawn_time_max, "Maximum time in which Mother Zombies should be picked, after round start", g_flInfectSpawnTimeMax, Int32, 15)
CON_ZR_CVAR(zr_infect_spawn_mz_ratio, "Ratio of all Players to Mother Zombies to be spawned at round start", g_flInfectSpawnMZRatio, Int32, 7)
CON_ZR_CVAR(zr_infect_spawn_mz_min_count, "Minimum amount of Mother Zombies to be spawned at round start", g_flInfectSpawnMinCount, Int32, 2)

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
	// g_pEngineServer2->ServerCommand("mp_weapons_allow_pistols 3");
	// g_pEngineServer2->ServerCommand("mp_weapons_allow_smgs 3");
	// g_pEngineServer2->ServerCommand("mp_weapons_allow_heavy 3");
	// g_pEngineServer2->ServerCommand("mp_weapons_allow_rifles 3");
	g_pEngineServer2->ServerCommand("mp_give_player_c4 0");
	g_pEngineServer2->ServerCommand("mp_friendlyfire 0");
	// Legacy Lua respawn stuff, do not use these, we should handle respawning ourselves now that we can
	g_pEngineServer2->ServerCommand("mp_respawn_on_death_t 1");
	g_pEngineServer2->ServerCommand("mp_respawn_on_death_ct 1");
	//g_pEngineServer2->ServerCommand("bot_quota_mode fill");
	//g_pEngineServer2->ServerCommand("mp_ignore_round_win_conditions 1");
}

void ZR_OnRoundPrestart(IGameEvent* pEvent)
{
	g_ZRRoundState = EZRRoundState::ROUND_START;
	g_iRoundNum++;

	g_pEngineServer2->ServerCommand("mp_respawn_on_death_t 1");
	g_pEngineServer2->ServerCommand("mp_respawn_on_death_ct 1");
}

void ZR_OnRoundStart(IGameEvent* pEvent)
{
	ClientPrintAll(HUD_PRINTTALK, ZR_PREFIX "The game is \x05Humans vs. Zombies\x01, the goal for zombies is to infect all humans by knifing them.");

	// SetUpAllHumanClasses();
	// SetupRespawnToggler();
	// SetupAmmoReplenish();

	// g_ZR_ROUND_STARTED = true;
}

void ZR_OnPlayerSpawn(IGameEvent* pEvent)
{
	CCSPlayerController* pController = (CCSPlayerController*)pEvent->GetPlayerController("userid");

	if (pController)
	{
		// delay infection a bit
		int iRoundNum = g_iRoundNum;
		bool bInfect = g_ZRRoundState == EZRRoundState::POST_INFECTION;

		CHandle<CCSPlayerController> handle = pController->GetHandle();
		new CTimer(0.05f, false, [iRoundNum, handle, bInfect]()
		{
			CCSPlayerController* pController = (CCSPlayerController*)handle.Get();
			if (iRoundNum != g_iRoundNum || !pController)
				return -1.0f;
			if(bInfect)
				ZR_Infect(pController, pController, true);
			else
				ZR_Cure(pController);
			return -1.0f;
		});
	}
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

void ZR_FakePlayerDeath(CCSPlayerController *pAttackerController, CCSPlayerController *pVictimController, const char *szWeapon, bool bDontBroadcast)
{
	IGameEvent *pEvent = g_gameEventManager->CreateEvent("player_death");

	if (!pEvent)
		return;
	// SetPlayer functions are swapped, need to remove the cast once fixed
	pEvent->SetPlayer("userid", (CEntityInstance*)pVictimController->GetPlayerSlot());
	pEvent->SetPlayer("attacker", (CEntityInstance*)pAttackerController->GetPlayerSlot());
	pEvent->SetInt("assister", 65535);
	pEvent->SetInt("assister_pawn", -1);
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

	g_gameEventManager->FireEvent(pEvent, bDontBroadcast);

}

void ZR_StripAndGiveKnife(CCSPlayerPawn *pPawn)
{
	CCSPlayer_ItemServices *pItemServices = pPawn->m_pItemServices();

	// it can sometimes be null when player joined on the very first round? 
	if (!pItemServices)
		return;

	// need to add a check for map item and drop them here
	// In csgo, map-spawned weapons have hammerid, whereas purchased weapons don't

	pItemServices->StripPlayerWeapons();
	pItemServices->GiveNamedItem("weapon_knife");
}

void ZR_Cure(CCSPlayerController *pTargetController)
{
	if (pTargetController->m_iTeamNum() == CS_TEAM_T)
		pTargetController->SwitchTeam(CS_TEAM_CT);

	CCSPlayerPawn *pTargetPawn = (CCSPlayerPawn*)pTargetController->GetPawn();
	if (!pTargetPawn)
		return;

	// set model/health/etc here
	//hardcode everything for now
	pTargetPawn->m_iMaxHealth = 100;
	pTargetPawn->m_iHealth = 100;

	pTargetPawn->SetModel("characters/models/ctm_sas/ctm_sas.vmdl");
}

void ZR_Infect(CCSPlayerController *pAttackerController, CCSPlayerController *pVictimController, bool bDontBroadcast)
{
	if (pVictimController->m_iTeamNum() == CS_TEAM_CT)
		pVictimController->SwitchTeam(CS_TEAM_T);

	ZR_FakePlayerDeath(pAttackerController, pVictimController, "knife", bDontBroadcast); // or any other killicon

	ZR_CheckWinConditions(true);

	CCSPlayerPawn *pVictimPawn = (CCSPlayerPawn*)pVictimController->GetPawn();
	if (!pVictimPawn)
		return;

	ZR_StripAndGiveKnife(pVictimPawn);
	// set model/health/etc here
	//hardcode everything for now
	pVictimPawn->m_iMaxHealth = 10000;
	pVictimPawn->m_iHealth = 10000;

	pVictimPawn->SetModel("characters/models/tm_phoenix/tm_phoenix.vmdl");
}


void ZR_InfectMotherZombie(CCSPlayerController *pVictimController)
{
	pVictimController->SwitchTeam(CS_TEAM_T);

	CCSPlayerPawn *pVictimPawn = (CCSPlayerPawn*)pVictimController->GetPawn();
	if (!pVictimPawn)
		return;

	ZR_StripAndGiveKnife(pVictimPawn);
	// set model/health/etc here
	//hardcode everything for now
	pVictimPawn->m_iMaxHealth = 10000;
	pVictimPawn->m_iHealth = 10000;

	pVictimPawn->SetModel("characters/models/tm_phoenix/tm_phoenix.vmdl");
}

void ZR_InitialInfection()
{
	// grab player count and mz infection candidates
	CUtlVector<CCSPlayerController*> pCandidateControllers;
	for (int i = 0; i < gpGlobals->maxClients; i++)
	{
		CCSPlayerController* pController = CCSPlayerController::FromSlot(i);
		if (!pController || pController->m_iTeamNum() != CS_TEAM_CT)
			continue;

		CCSPlayerController* pPawn = (CCSPlayerController*)pController->GetPawn();
		if (!pPawn || !pPawn->IsAlive())
			continue;

		pCandidateControllers.AddToTail(pController);
	}

	// calculate the num of mz to infect
	int iMZToInfect = pCandidateControllers.Count() / g_flInfectSpawnMZRatio;
	iMZToInfect = g_flInfectSpawnMinCount > iMZToInfect ? g_flInfectSpawnMinCount : iMZToInfect;

	// get spawn points
	CUtlVector<SpawnPoint*> spawns;
	if (g_flInfectSpawnType == EZRSpawnType::RESPAWN)
	{
		SpawnPoint* spawn = nullptr;
		while (nullptr != (spawn = (SpawnPoint*)UTIL_FindEntityByClassname(spawn, "info_player_*")))
		{
			if (spawn->m_bEnabled())
				spawns.AddToTail(spawn);
		}
	}

	// infect
	while (pCandidateControllers.Count() > 0 && iMZToInfect > 0)
	{
		int randomindex = rand() % pCandidateControllers.Count();
		ZR_InfectMotherZombie(pCandidateControllers[randomindex]);

		CCSPlayerController* pPawn = (CCSPlayerController*)pCandidateControllers[randomindex]->GetPawn();
		if (pPawn && spawns.Count())
		{
			int randomindex = rand() % spawns.Count();
			pPawn->SetAbsOrigin(spawns[randomindex]->GetAbsOrigin());
			pPawn->SetAbsRotation(spawns[randomindex]->GetAbsRotation());
		}

		pCandidateControllers.FastRemove(randomindex);
		iMZToInfect--;
	}
	ClientPrintAll(HUD_PRINTCENTER, "First infection has started!");
	ClientPrintAll(HUD_PRINTTALK, ZR_PREFIX "First infection has started! Good luck, survivors!");
	g_ZRRoundState = EZRRoundState::POST_INFECTION;
}

void ZR_StartInitialCountdown()
{
	int iRoundNum = g_iRoundNum;
	g_iInfectionCountDown = g_flInfectSpawnTimeMin + (rand() % (g_flInfectSpawnTimeMax - g_flInfectSpawnTimeMin + 1));
	new CTimer(1.0f, false, [iRoundNum]()
	{
		if (iRoundNum != g_iRoundNum)
			return -1.0f;
		if (g_iInfectionCountDown <= 0)
		{
			ZR_InitialInfection();
			return -1.0f;
		}

		if (g_iInfectionCountDown <= 60)
		{
			ClientPrintAll(HUD_PRINTCENTER, "First infection in \7%i second(s)\1!", g_iInfectionCountDown);
			if (g_iInfectionCountDown % 5 == 0)
				ClientPrintAll(HUD_PRINTTALK, ZR_PREFIX "First infection in \7%i second\1!", g_iInfectionCountDown);
		}
		g_iInfectionCountDown--;

		return 1.0f;
	});
}

bool ZR_Detour_TakeDamageOld(CCSPlayerPawn *pVictimPawn, CTakeDamageInfo *pInfo)
{
	CCSPlayerPawn* pAttackerPawn = (CCSPlayerPawn*)pInfo->m_hAttacker.Get();

	if (!(pAttackerPawn && pVictimPawn && pVictimPawn->IsPawn() && pVictimPawn->IsPawn()))
		return false;

	CCSPlayerController *pAttackerController = CCSPlayerController::FromPawn(pAttackerPawn);
	CCSPlayerController *pVictimController = CCSPlayerController::FromPawn(pVictimPawn);
	const char *pszAbilityClass = pInfo->m_hAbility.Get() ? pInfo->m_hAbility.Get()->GetClassname() : "";
	if (pAttackerPawn->m_iTeamNum() == CS_TEAM_T && pVictimPawn->m_iTeamNum() == CS_TEAM_CT && !V_strncmp(pszAbilityClass, "weapon_knife", 12))
	{
		ZR_Infect(pAttackerController, pVictimController, false);
		return true; // nullify the damage
	}

	// grenade and molotov knockback
	if (pAttackerPawn->m_iTeamNum() == CS_TEAM_CT && pVictimPawn->m_iTeamNum() == CS_TEAM_T)
	{
		CBaseEntity *pInflictor = pInfo->m_hInflictor.Get();
		const char *pszInflictorClass = pInflictor ? pInflictor->GetClassname() : "";
		if (!V_strncmp(pszInflictorClass, "hegrenade_projectile", 9) || !V_strncmp(pszInflictorClass, "inferno", 7))
			ZR_ApplyKnockbackExplosion((Z_CBaseEntity*)pInflictor, (CCSPlayerPawn*)pVictimPawn, (int)pInfo->m_flDamage);
	}
	return false;
}

// return false to prevent player from picking it up
bool ZR_Detour_CCSPlayer_WeaponServices_CanUse(CCSPlayer_WeaponServices *pWeaponServices, CBasePlayerWeapon* pPlayerWeapon)
{
	CCSPlayerPawn *pPawn = pWeaponServices->__m_pChainEntity();
	if (pPawn && pPawn->m_iTeamNum() == CS_TEAM_T && V_strncmp(pPlayerWeapon->GetClassname(), "weapon_knife", 12))
		return false;

	// doesn't guarantee the player will pick the weapon up, it just allows the main detour to continue to run
	return true;
}

void ZR_OnPlayerHurt(IGameEvent* pEvent)
{
	CCSPlayerController *pAttackerController = (CCSPlayerController*)pEvent->GetPlayerController("attacker");
	CCSPlayerController *pVictimController = (CCSPlayerController*)pEvent->GetPlayerController("userid");
	const char* szWeapon = pEvent->GetString("weapon");
	int iDmgHealth = pEvent->GetInt("dmg_health");


	// grenade and molotov knockbacks are handled by TakeDamage detours
	if (!pAttackerController || !pVictimController || !V_strncmp(szWeapon, "inferno", 7) || !V_strncmp(szWeapon, "hegrenade", 9))
		return;

	if (pAttackerController->m_iTeamNum() == CS_TEAM_CT && pVictimController->m_iTeamNum() == CS_TEAM_T)
		ZR_ApplyKnockback((CCSPlayerPawn*)pAttackerController->GetPawn(), (CCSPlayerPawn*)pVictimController->GetPawn(), iDmgHealth, szWeapon);
}

void ZR_OnPlayerDeath(IGameEvent* pEvent)
{
	if (g_ZRRoundState == EZRRoundState::ROUND_START)
		return;

	CCSPlayerController *pVictimController = (CCSPlayerController*)pEvent->GetPlayerController("userid");
	if (!pVictimController)
		return;
	CCSPlayerPawn *pVictimPawn = (CCSPlayerPawn*)pVictimController->GetPawn();
	if (!pVictimPawn)
		return;
	
	ZR_CheckWinConditions(pVictimPawn->m_iTeamNum() == CS_TEAM_CT);
}

void ZR_OnRoundFreezeEnd(IGameEvent* pEvent)
{
	ZR_StartInitialCountdown();
}

// check whether players on a team are all dead
void ZR_CheckWinConditions(bool bCheckCT)
{
	if (g_ZRRoundState == EZRRoundState::ROUND_END)
		return;
		
	int iTeamNum = bCheckCT ? CS_TEAM_CT : CS_TEAM_T;

	// loop through each player, return early if both team has alive player
	CCSPlayerPawn* pPawn = nullptr;
	while (nullptr != (pPawn = (CCSPlayerPawn*)UTIL_FindEntityByClassname(pPawn, "player")))
	{
		if (!pPawn->IsAlive())
			continue;
		
		if (pPawn->m_iTeamNum() == iTeamNum)
			return;
	}

	// didn't return early, all players on one or both teams are dead.
	// 8: CT win; 9: T wins; 10: draw
	g_pGameRules->TerminateRound(5.0f, bCheckCT ? CSRoundEndReason::TerroristWin : CSRoundEndReason::CTWin);
	g_ZRRoundState = EZRRoundState::ROUND_END;

	g_pEngineServer2->ServerCommand("mp_respawn_on_death_t 0");
	g_pEngineServer2->ServerCommand("mp_respawn_on_death_ct 0");

	// increment team score
	iTeamNum = bCheckCT ? CS_TEAM_T : CS_TEAM_CT; //?????

	// so uhh.. I don't know how to get cteam the proper way..
	CTeam* pTeam = nullptr;
	while (nullptr != (pTeam = (CTeam*)UTIL_FindEntityByClassname(pTeam, "cs_team_manager")))
	{
		if (pTeam->m_iTeamNum() == iTeamNum)
		{
			pTeam->m_iScore = pTeam->m_iScore() + 1;
			//Msg("%d, %d\n", pTeam->m_iTeamNum(), pTeam->m_iScore());
			return;
		}
	}
}

CON_COMMAND_CHAT(ztele, "teleport to spawn")
{
	// Silently return so the command is completely hidden
	if (!g_bEnableZR)
		return;

	if (!player)
	{
		ClientPrint(player, HUD_PRINTCONSOLE, CHAT_PREFIX "You cannot use this command from the server console.");
		return;
	}

	// Check if command is enabled for humans
	if (!g_bZteleHuman && player->m_iTeamNum() == CS_TEAM_CT)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "You cannot use this command as a human.");
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
			CBasePlayerPawn* pPawn = handle.Get();

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