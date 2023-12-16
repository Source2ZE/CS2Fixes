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
void ZR_CheckWinConditions(int iTeamNum);
void ZR_Cure(CCSPlayerController *pTargetController);
void ZR_EndRoundAndAddTeamScore(int iTeamNum);
void SetupCTeams();

EZRRoundState g_ZRRoundState = EZRRoundState::ROUND_START;
static int g_iRoundNum = 0;
static int g_iInfectionCountDown = 0;
static bool g_bRespawnEnabled = false;
static CHandle<Z_CBaseEntity> g_hRespawnToggler;
static CHandle<CTeam> g_hTeamCT;
static CHandle<CTeam> g_hTeamT;

// CONVAR_TODO
bool g_bEnableZR = false;
static float g_flMaxZteleDistance = 150.0f;
static bool g_bZteleHuman = false;
static float g_flKnockbackScale = 5.0f;
static int g_iInfectSpawnType = EZRSpawnType::RESPAWN;
static int g_iInfectSpawnTimeMin = 15;
static int g_iInfectSpawnTimeMax = 15;
static int g_iInfectSpawnMZRatio = 7;
static int g_iInfectSpawnMinCount = 2;
static float g_flRespawnDelay = 5.0;
static int g_iDefaultWinnerTeam = CS_TEAM_SPECTATOR;

CON_ZR_CVAR(zr_enable, "Whether to enable ZR features", g_bEnableZR, Bool, false)
CON_ZR_CVAR(zr_ztele_max_distance, "Maximum distance players are allowed to move after starting ztele", g_flMaxZteleDistance, Float32, 150.0f)
CON_ZR_CVAR(zr_ztele_allow_humans, "Whether to allow humans to use ztele", g_bZteleHuman, Bool, false)
CON_ZR_CVAR(zr_knockback_scale, "Global knockback scale", g_flKnockbackScale, Float32, 5.0f)
CON_ZR_CVAR(zr_infect_spawn_type, "Type of Mother Zombies Spawn [0 = MZ spawn where they stand, 1 = MZ get teleported back to spawn on being picked]", g_iInfectSpawnType, Int32, EZRSpawnType::RESPAWN)
CON_ZR_CVAR(zr_infect_spawn_time_min, "Minimum time in which Mother Zombies should be picked, after round start", g_iInfectSpawnTimeMin, Int32, 15)
CON_ZR_CVAR(zr_infect_spawn_time_max, "Maximum time in which Mother Zombies should be picked, after round start", g_iInfectSpawnTimeMax, Int32, 15)
CON_ZR_CVAR(zr_infect_spawn_mz_ratio, "Ratio of all Players to Mother Zombies to be spawned at round start", g_iInfectSpawnMZRatio, Int32, 7)
CON_ZR_CVAR(zr_infect_spawn_mz_min_count, "Minimum amount of Mother Zombies to be spawned at round start", g_iInfectSpawnMinCount, Int32, 2)
CON_ZR_CVAR(zr_respawn_delay, "Time before a zombie is respawned", g_flRespawnDelay, Float32, 5.0)
CON_ZR_CVAR(zr_default_winner_team, "Which team wins when time ran out [1 = Draw, 2 = Zombies, 3 = Humans]", g_iDefaultWinnerTeam, Int32, CS_TEAM_SPECTATOR)

void ZR_OnStartupServer()
{
	g_ZRRoundState = EZRRoundState::ROUND_START;

	// Here we force some cvars that are necessary for the gamemode
	g_pEngineServer2->ServerCommand("mp_give_player_c4 0");
	g_pEngineServer2->ServerCommand("mp_friendlyfire 0");
	g_pEngineServer2->ServerCommand("bot_quota_mode fill"); // Necessary to fix bots kicked/joining infinitely when forced to CT https://github.com/Source2ZE/ZombieReborn/issues/64
	g_pEngineServer2->ServerCommand("mp_ignore_round_win_conditions 1");
	// These disable most of the buy menu for zombies
	g_pEngineServer2->ServerCommand("mp_weapons_allow_pistols 3");
	g_pEngineServer2->ServerCommand("mp_weapons_allow_smgs 3");
	g_pEngineServer2->ServerCommand("mp_weapons_allow_heavy 3");
	g_pEngineServer2->ServerCommand("mp_weapons_allow_rifles 3");

	SetupCTeams();
}

void ZR_RespawnAll()
{
	for (int i = 0; i < gpGlobals->maxClients; i++)
	{
		CCSPlayerController* pController = CCSPlayerController::FromSlot(i);

		if (!pController ||  (pController->m_iTeamNum() != CS_TEAM_CT && pController->m_iTeamNum() != CS_TEAM_T))
			continue;
		pController->Respawn();
	}
}

void ToggleRespawn(bool force = false, bool value = false)
{
	if ((!force && !g_bRespawnEnabled) || (force && value))
	{
		g_bRespawnEnabled = true;
		ZR_RespawnAll();
	}
	else
	{
		g_bRespawnEnabled = false;
		ZR_CheckWinConditions(CS_TEAM_T);
	}
}

void ZR_OnRoundPrestart(IGameEvent* pEvent)
{
	g_ZRRoundState = EZRRoundState::ROUND_START;
	g_iRoundNum++;
	ToggleRespawn(true, true);

	for (int i = 0; i < gpGlobals->maxClients; i++)
	{
		CCSPlayerController* pController = CCSPlayerController::FromSlot(i);

		// Only do this for Ts, ignore CTs and specs
		if (!pController || pController->m_iTeamNum() != CS_TEAM_T)
			continue;

		pController->SwitchTeam(CS_TEAM_CT);
	}
}

void SetupRespawnToggler()
{
	Z_CBaseEntity* relay = CreateEntityByName("logic_relay");

	relay->SetEntityName("zr_toggle_respawn");
	relay->DispatchSpawn();
	g_hRespawnToggler = relay->GetHandle();
}

void SetupCTeams()
{
	CTeam* pTeam = nullptr;
	while (nullptr != (pTeam = (CTeam*)UTIL_FindEntityByClassname(pTeam, "cs_team_manager")))
	{
		if (pTeam->m_iTeamNum() == CS_TEAM_CT)
		{
			g_hTeamCT = pTeam->GetHandle();
		}
		else if (pTeam->m_iTeamNum() == CS_TEAM_T)
		{
			g_hTeamT = pTeam->GetHandle();
		}
	}
}

void ZR_OnRoundStart(IGameEvent* pEvent)
{
	ClientPrintAll(HUD_PRINTTALK, ZR_PREFIX "The game is \x05Humans vs. Zombies\x01, the goal for zombies is to infect all humans by knifing them.");

	SetupRespawnToggler();
	// SetupAmmoReplenish();
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
	pEvent->SetBool("infected", true);

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
	int iMZToInfect = pCandidateControllers.Count() / g_iInfectSpawnMZRatio;
	iMZToInfect = g_iInfectSpawnMinCount > iMZToInfect ? g_iInfectSpawnMinCount : iMZToInfect;

	// get spawn points
	CUtlVector<SpawnPoint*> spawns;
	if (g_iInfectSpawnType == EZRSpawnType::RESPAWN)
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

		CCSPlayerController* pPawn = (CCSPlayerController*)pCandidateControllers[randomindex]->GetPawn();
		if (pPawn && spawns.Count())
		{
			int randomindex = rand() % spawns.Count();
			pPawn->SetAbsOrigin(spawns[randomindex]->GetAbsOrigin());
			pPawn->SetAbsRotation(spawns[randomindex]->GetAbsRotation());
		}
		ZR_InfectMotherZombie(pCandidateControllers[randomindex]);

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
	g_iInfectionCountDown = g_iInfectSpawnTimeMin + (rand() % (g_iInfectSpawnTimeMax - g_iInfectSpawnTimeMin + 1));
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

void ZR_Detour_CEntityIdentity_AcceptInput(CEntityIdentity* pThis, CUtlSymbolLarge* pInputName, CEntityInstance* pActivator, CEntityInstance* pCaller, variant_string_t* value, int nOutputID)
{
	if (!g_hRespawnToggler.IsValid())
		return;

	Z_CBaseEntity* relay = g_hRespawnToggler.Get();
	const char* inputName = pInputName->String();

	Message("pThis: %s\n", pThis->m_designerName.String());
	Message("pInputName: %s\n", inputName);

	// Must be a Trigger input into our zr_toggle_respawn relay
	if (!relay || pThis != relay->m_pEntity || V_strcasecmp(inputName, "Trigger"))
		return;

	ToggleRespawn();
	ClientPrintAll(HUD_PRINTTALK, ZR_PREFIX "Respawning is %s!", g_bRespawnEnabled ? "enabled" : "disabled");
}

void ZR_Hook_ClientPutInServer(CPlayerSlot slot, char const *pszName, int type, uint64 xuid)
{
	CCSPlayerController* pController = CCSPlayerController::FromSlot(slot);
	if (!pController)
		return;
	
	pController->ChangeTeam(g_ZRRoundState == EZRRoundState::POST_INFECTION ? CS_TEAM_T : CS_TEAM_CT);
	if (g_bRespawnEnabled)
		pController->Respawn();
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
	CCSPlayerController *pVictimController = (CCSPlayerController*)pEvent->GetPlayerController("userid");
	if (!pVictimController)
		return;
	CCSPlayerPawn *pVictimPawn = (CCSPlayerPawn*)pVictimController->GetPawn();
	if (!pVictimPawn)
		return;
	
	// a player died from infection
	if (pEvent->GetBool("infected"))
	{
		// pVictimPawn->m_iTeamNum() is T due to team switch, but we need to check CT instead
		ZR_CheckWinConditions(CS_TEAM_CT);
		// don't need to respawn
		return;
	}

	ZR_CheckWinConditions(pVictimPawn->m_iTeamNum());

	// respawn player
	CHandle<CCSPlayerController> handle = pVictimController->GetHandle();
	int iRoundNum = g_iRoundNum;
	new CTimer(g_flRespawnDelay, false, [iRoundNum, handle]()
	{
		CCSPlayerController* pController = (CCSPlayerController*)handle.Get();
		if (iRoundNum != g_iRoundNum || !pController || !g_bRespawnEnabled)
			return -1.0f;
		pController->Respawn();
		return -1.0f;
	});
}

void ZR_OnRoundFreezeEnd(IGameEvent* pEvent)
{
	ZR_StartInitialCountdown();
}

// there is probably a better way to check when time is running out...
void ZR_OnRoundTimeWarning(IGameEvent* pEvent)
{
	int iRoundNum = g_iRoundNum;
	new CTimer(10.0, false, [iRoundNum]()
	{
		if (iRoundNum != g_iRoundNum)
			return -1.0f;
		ZR_EndRoundAndAddTeamScore(g_iDefaultWinnerTeam);
		return -1.0f;
	});
}

// check whether players on a team are all dead
void ZR_CheckWinConditions(int iTeamNum)
{
	if (g_ZRRoundState == EZRRoundState::ROUND_END || (iTeamNum == CS_TEAM_T && g_bRespawnEnabled) || (iTeamNum != CS_TEAM_T && iTeamNum != CS_TEAM_CT))
		return;

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
	// allow the opposite team to win
	iTeamNum = iTeamNum == CS_TEAM_CT ? CS_TEAM_T : CS_TEAM_CT;
	ZR_EndRoundAndAddTeamScore(iTeamNum);
}

void ZR_EndRoundAndAddTeamScore(int iTeamNum)
{	
	CSRoundEndReason iReason = CSRoundEndReason::Draw;
	if (iTeamNum == CS_TEAM_T)
		iReason = CSRoundEndReason::TerroristWin;
	else if (iTeamNum == CS_TEAM_CT)
		iReason = CSRoundEndReason::CTWin;
	// CONVAR_TODO
	// use mp_round_restart_delay here
	g_pGameRules->TerminateRound(5.0f, iReason);
	g_ZRRoundState = EZRRoundState::ROUND_END;
	ToggleRespawn(true, false);

	if (iTeamNum == CS_TEAM_CT)
	{
		if (!g_hTeamCT.IsValid())
		{
			Panic("Cannot find CTeam for CT!\n");
			return;
		}
			g_hTeamCT->m_iScore = g_hTeamCT->m_iScore() + 1;
	}
	else if (iTeamNum == CS_TEAM_T)
	{	
		if (!g_hTeamT.IsValid())
		{
			Panic("Cannot find CTeam for T!\n");
			return;
		}
		g_hTeamT->m_iScore = g_hTeamT->m_iScore() + 1;
	}
}

CON_COMMAND_CHAT(ztele, "teleport to spawn")
{
	// Silently return so the command is completely hidden
	if (!g_bEnableZR)
		return;

	if (!player)
	{
		ClientPrint(player, HUD_PRINTCONSOLE, ZR_PREFIX "You cannot use this command from the server console.");
		return;
	}

	// Check if command is enabled for humans
	if (!g_bZteleHuman && player->m_iTeamNum() == CS_TEAM_CT)
	{
		ClientPrint(player, HUD_PRINTTALK, ZR_PREFIX "You cannot use this command as a human.");
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
		ClientPrint(player, HUD_PRINTTALK, ZR_PREFIX"There are no spawns!");
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
		ClientPrint(player, HUD_PRINTTALK, ZR_PREFIX"You cannot teleport when dead!");
		return;
	}

	//Get initial player position so we can do distance check
	Vector initialpos = pPawn->GetAbsOrigin();

	ClientPrint(player, HUD_PRINTTALK, ZR_PREFIX"Teleporting to spawn in 5 seconds.");

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
			ClientPrint(pPawn->GetController(), HUD_PRINTTALK, ZR_PREFIX "You have been teleported to spawn.");
		}
		else
		{
			ClientPrint(pPawn->GetController(), HUD_PRINTTALK, ZR_PREFIX "Teleport failed! You moved too far.");
			return -1.0f;
		}

		return -1.0f;
	});
}