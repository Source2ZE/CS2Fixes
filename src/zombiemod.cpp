/**
 * =============================================================================
 * CS2Fixes
 * Copyright (C) 2023-2025 Source2ZE
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

#include "commands.h"
#include "ctimer.h"
#include "customio.h"
#include "engine/igameeventsystem.h"
#include "entity/cgamerules.h"
#include "entity/cparticlesystem.h"
#include "entity/cteam.h"
#include "entity/services.h"
#include "eventlistener.h"
#include "hud_manager.h"
#include "leader.h"
#include "networksystem/inetworkmessages.h"
#include "playermanager.h"
#include "recipientfilters.h"
#include "serversideclient.h"
#include "tier0/vprof.h"
#include "user_preferences.h"
#include "utils/entity.h"
#include "vendor/nlohmann/json.hpp"
#include "zombiemod.h"
#include <fstream>
#include "sphereentity.h"

#include "tier0/memdbgon.h"

void ZM_Infect(CCSPlayerController* pAttackerController, CCSPlayerController* pVictimController, bool bDontBroadcast);
void ZM_Cure(CCSPlayerController* pTargetController);
void ZM_EndRound(int iTeamNum);
void ZM_FinishRound(int iTeamNum);
void ZM_SetupCTeams();
bool ZM_IsTeamAlive(int iTeamNum);

EZMRoundState g_ZMRoundState = EZMRoundState::ROUND_START;
static bool g_bRespawnEnabled = true;
static CHandle<CBaseEntity> g_hRespawnToggler;
static CHandle<CTeam> g_hTeamCT;
static CHandle<CTeam> g_hTeamT;

extern CZRPlayerClassManager* g_pZRPlayerClassManager;
extern ZRWeaponConfig* g_pZRWeaponConfig;
extern ZRHitgroupConfig* g_pZRHitgroupConfig;
extern std::vector<ZEPlayerHandle> g_MotherZombies;

CConVar<bool> g_cvarZMEnable("zm_enable", (FCVAR_REPLICATED | FCVAR_SPONLY | FCVAR_NOTIFY), "ZombieMod enabled or not.", false, ConVarZMEnableChange);
CConVar<CUtlString> g_cvarZMVersion("zm_version", (FCVAR_REPLICATED | FCVAR_SPONLY | FCVAR_NOTIFY), "ZombieMod version", "4.2.1");
CConVar<CUtlString> g_cvarZMHumanWinOverlayParticle("zm_human_win_overlay_particle", FCVAR_NONE, "Screenspace particle to display when human win", "");
CConVar<CUtlString> g_cvarZMZombieWinOverlayParticle("zm_zombie_win_overlay_particle", FCVAR_NONE, "Screenspace particle to display when zombie win", "");
CConVar<int> g_cvarZMInfectSpawnType("zm_infect_spawn_type", FCVAR_NONE, "Type of Mother Zombies Spawn [0 = MZ spawn where they stand, 1 = MZ get teleported back to spawn on being picked]", (int)EZMSpawnType::ZM_RESPAWN, true, 0, true, 1);
CConVar<bool> g_cvarZMInfectSpawnWarning("zm_infect_spawn_warning", FCVAR_NONE, "Whether to warn players of zombies spawning between humans", true);
CConVar<float> g_cvarZMKnockbackScale("zm_knockback_scale", FCVAR_NONE, "Global knockback scale", 5.0f);
CConVar<float> g_cvarZMMoanInterval("zm_sounds_moan_interval", FCVAR_NONE, "How often in seconds should zombies moan", 30.0f, true, 0.0f, false, 0.0f);
CConVar<bool> g_cvarZMInfectShake("zm_infect_shake", FCVAR_NONE, "Whether to shake a player's view on infect", true);
CConVar<float> g_cvarZMInfectShakeAmplitude("zm_infect_shake_amp", FCVAR_NONE, "Amplitude of shaking effect", 15.0f, true, 0.0f, true, 16.0f);
CConVar<float> g_cvarZMInfectShakeFrequency("zm_infect_shake_frequency", FCVAR_NONE, "Frequency of shaking effect", 2.0f, true, 0.0f, false, 0.0f);
CConVar<float> g_cvarZMInfectShakeDuration("zm_infect_shake_duration", FCVAR_NONE, "Duration of shaking effect", 5.0f, true, 0.0f, false, 0.0f);
CConVar<int> g_cvarZMInfectSpawnMZRatio("zm_infect_spawn_mz_ratio", FCVAR_NONE, "Ratio of all Players to Mother Zombies to be spawned at round start", 7, true, 1, true, 64);
CConVar<int> g_cvarZMInfectSpawnMinCount("zm_infect_spawn_mz_min_count", FCVAR_NONE, "Minimum amount of Mother Zombies to be spawned at round start", 1, true, 0, false, 0);
CConVar<int> g_cvarZMMZImmunityReduction("zm_mz_immunity_reduction", FCVAR_NONE, "How much mz immunity to reduce for each player per round (0-100)", 20, true, 0, true, 100);
CConVar<float> g_cvarZMRespawnDelay("zm_respawn_delay", FCVAR_NONE, "Time before a zombie is automatically respawned, -1 disables this. Note that maps can still manually respawn at any time", 5.0f, true, -1.0f, false, 0.0f);
CConVar<int> g_cvarZMInfectSpawnTimeMin("zm_infect_spawn_time_min", FCVAR_NONE, "Minimum time in which Mother Zombies should be picked, after round start", 15, true, 0, false, 0);
CConVar<int> g_cvarZMInfectSpawnTimeMax("zm_infect_spawn_time_max", FCVAR_NONE, "Maximum time in which Mother Zombies should be picked, after round start", 15, true, 1, false, 0);
CConVar<int> g_cvarZMGroanChance("zm_sounds_groan_chance", FCVAR_NONE, "How likely should a zombie groan whenever they take damage (1 / N)", 5, true, 1, false, 0);
CConVar<bool> g_cvarZMNapalmGrenades("zm_napalm_enable", FCVAR_NONE, "Whether to use napalm grenades", true);
CConVar<float> g_cvarZMNapalmDuration("zm_napalm_burn_duration", FCVAR_NONE, "How long in seconds should zombies burn from napalm grenades", 5.0f, true, 0.0f, false, 0.0f);
CConVar<float> g_cvarZMNapalmFullDamage("zm_napalm_full_damage", FCVAR_NONE, "The amount of damage needed to apply full burn duration for napalm grenades (max grenade damage is 99)", 50.0f, true, 0.0f, true, 99.0f);
CConVar<float> g_cvarZMDamageCashScale("zm_damage_cash_scale", FCVAR_NONE, "Multiplier on cash given when damaging zombies (0.0 = disabled)", 0.0f, true, 0.0f, false, 100.0f);
CConVar<int> g_cvarZMDefaultWinnerTeam("zm_default_winner_team", FCVAR_NONE, "Which team wins when time ran out [1 = Draw, 2 = Zombies, 3 = Humans]", CS_TEAM_SPECTATOR, true, 1, true, 3);
CConVar<bool> g_cvarZMZteleHuman("zm_ztele_allow_humans", FCVAR_NONE, "Whether to allow humans to use zmtele", false);
CConVar<bool> g_cvarZMZteleZombie("zm_ztele_allow_zombies", FCVAR_NONE, "Whether to allow zombies to use zmtele", false);
CConVar<float> g_cvarZMMaxZteleDistance("zm_ztele_max_distance", FCVAR_NONE, "Maximum distance players are allowed to move after starting ztele", 150.0f, true, 0.0f, false, 0.0f);
CConVar<bool> g_cvarZMTeleHumanAfter("zm_ztele_human_after", FCVAR_NONE, "Allow humans to use ZTele after the mother zombie has spawned. 0 means always allowed.", true);
CConVar<bool> g_cvarZMTeleHumanBefore("zm_ztele_human_before", FCVAR_NONE, "Allow humans to use ZTele before the mother zombie has spawned.", true);
CConVar<float> g_cvarZMTeleDelayZombie("zm_ztele_delay_zombie", FCVAR_NONE, "Time between using ZTele command and teleportation for zombies. [Dependency: zm_ztele_allow_zombies]", 3.0f, true, 1.0f, true, 100.0f);
CConVar<float> g_cvarZMTeleDelayHuman("zm_ztele_delay_human", FCVAR_NONE, "Time between using ZTele command and teleportation for humans. [Dependency: zm_ztele_allow_humans & zm_ztele_human_(before)/(after)]", 3.0f, true, 1.0f, true, 100.0f);
CConVar<int> g_cvarZMTeleMaxZombie("zm_ztele_max_zombie", FCVAR_NONE, "Max number of times a zombie is allowed to use ZTele per round. [Dependency: zm_ztele_allow_zombies]", 3, true, 0, true, 30);
CConVar<int> g_cvarZMTeleMaxHuman("zm_ztele_max_human", FCVAR_NONE, "Max number of times a human is allowed to use ZTele per round. [Dependency: zm_ztele_allow_humans & zm_ztele_human_(before)/(after)]", 1, true, 0, true, 30);
CConVar<bool> g_cvarZMInfiniteAmmo("zm_infinite_ammo", FCVAR_NONE, "Whether or not ammo gets set to 999 in the mag after first reload.", false);
CConVar<int> g_cvarZMInfiniteAmmoTotal("zm_infinite_ammo_total", FCVAR_NONE, "The total amount of actual bullets to count as 'infinite'.", 9999, true, 999, true, 99999);
CConVar<bool> g_cvarZMUserPresToFile("zm_user_prefs_to_file", FCVAR_NONE, "Whether to save user prefs to a file if cs2f_user_prefs_api is not set [Folder on server needs to be read/writeable by the game and is game/csgo/addons/cs2fixes/data]", true);

CConVar<bool> g_cvarZMRunWithBots("zm_run_with_bots", FCVAR_NONE, "When true, the server will end rounds as normal when only bots exist in the server.", false);

CConVar<bool> g_cvarZMFreezeGrenades("zm_freeze_grenades", FCVAR_NONE, "When enabled, decoy grenades freeze people in the freeze radius.", false);
CConVar<int> g_cvarZMFreezeRadius("zm_freeze_radius", FCVAR_NONE, "When freeze grenades are enabled, what radius to freeze zombies.", 200, true, 1, true, 1000);
CConVar<float> g_cvarZMFreezeTime("zm_freeze_time", FCVAR_NONE, "When freeze grenades are enabled, how long to freeze them.", 4.0f, true, 1.0f, true, 60.0f);
CConVar<bool> g_cvarZMFreezeLaser("zm_freeze_laser_radius", FCVAR_NONE, "When freeze grenades are enabled, whether or not to show the laser around the radius.", true);
CConVar<bool> g_cvarZMInfectCountdown("zm_infect_countdown", FCVAR_NONE, "When enabled, displays a message every 5 sections showing remaining time before initial infection.", true);

CConVar<int> g_cvarZMInfectSpawnMinCountReq("zm_infect_min_count_req", FCVAR_NONE, "Minimum amount of Players required to spawn Mother Zombies at round start", 2, true, 0, false, 0);

void ZM_Precache(IEntityResourceManifest* pResourceManifest)
{
	g_pZRPlayerClassManager->LoadPlayerClass(EZRGameMode::GAMEMODE_ZM);
	g_pZRPlayerClassManager->PrecacheModels(pResourceManifest);

	pResourceManifest->AddResource(g_cvarZMHumanWinOverlayParticle.Get().String());
	pResourceManifest->AddResource(g_cvarZMZombieWinOverlayParticle.Get().String());

	pResourceManifest->AddResource("soundevents/soundevents_zr.vsndevts");

	// ZMBIO ASSETS freeze grenade ice cube, shown on frozen zombies (see ZM_DecoyExploded)
	pResourceManifest->AddResource("weapons/models/net4all/freeze_grenade/ice_cube/ice_cube.vmdl");
}

void ZM_OnLevelInit()
{
	g_ZMRoundState = EZMRoundState::ROUND_START;

	// Delay one tick to override any .cfg's
	CTimer::Create(0.02f, TIMERFLAG_MAP, []() {
		// Here we force some cvars that are necessary for the gamemode
		g_pEngineServer2->ServerCommand("mp_give_player_c4 0");
		g_pEngineServer2->ServerCommand("mp_friendlyfire 0");
		g_pEngineServer2->ServerCommand("mp_ignore_round_win_conditions 1");
		// Necessary to fix bots kicked/joining infinitely when forced to CT https://github.com/Source2ZE/ZombieReborn/issues/64
		g_pEngineServer2->ServerCommand("bot_quota_mode fill");
		g_pEngineServer2->ServerCommand("mp_autoteambalance 0");

		return -1.0f;
	});

	g_pZRWeaponConfig->LoadWeaponConfig(EZRGameMode::GAMEMODE_ZM);
	g_pZRHitgroupConfig->LoadHitgroupConfig(EZRGameMode::GAMEMODE_ZM);
	ZM_SetupCTeams();
}

void ZM_RespawnAll()
{
	if (!GetGlobals())
		return;

	for (int i = 0; i < GetGlobals()->maxClients; i++)
	{
		CCSPlayerController* pController = CCSPlayerController::FromSlot(i);

		if (!pController || pController->m_bIsHLTV || (pController->m_iTeamNum() != CS_TEAM_CT && pController->m_iTeamNum() != CS_TEAM_T))
			continue;

		ZEPlayer* pPlayer = pController->GetZEPlayer();
		if (pPlayer)
		{
			pPlayer->SetHumanTeleUsages(0);
			pPlayer->SetZombieTeleUsages(0);
			pPlayer->SetHitsFromZombies(0);
			pPlayer->SetFrozen(false);
		}

		pController->Respawn();

	}
}

void ZM_ToggleRespawn(bool force = false, bool value = false)
{
	if ((!force && !g_bRespawnEnabled) || (force && value))
	{
		g_bRespawnEnabled = true;
		ZM_RespawnAll();
	}
	else
	{
		g_bRespawnEnabled = false;
		ZM_CheckTeamWinConditions(CS_TEAM_CT);
	}
}

void ZM_OnRoundPrestart(IGameEvent* pEvent)
{
	// Gamerules may not be available earlier, so easiest to just enforce this here
	if (g_pGameRules)
	{
		g_pGameRules->m_iMaxNumCTs = 64;
		g_pGameRules->m_iMaxNumTerrorists = 64;
	}

	g_ZMRoundState = EZMRoundState::ROUND_START;
	ZM_ToggleRespawn(true, true);

	if (!GetGlobals())
		return;

	for (int i = 0; i < GetGlobals()->maxClients; i++)
	{
		CCSPlayerController* pController = CCSPlayerController::FromSlot(i);

		if (!pController || pController->m_bIsHLTV)
			continue;

		// Only do this for Ts, ignore CTs and specs
		if (pController->m_iTeamNum() == CS_TEAM_T)
			pController->SwitchTeam(CS_TEAM_CT);

		CCSPlayerPawn* pPawn = pController->GetPlayerPawn();

		// Prevent damage that occurs between now and when the round restart is finished
		// Somehow CT filtered nukes can apply damage during the round restart (all within CCSGameRules::RestartRound)
		// And if everyone was a zombie at this moment, they will all die and trigger ANOTHER round restart which breaks everything
		if (pPawn)
			pPawn->m_bTakesDamage = false;
	}

	g_MotherZombies.clear();
}

void ZM_SetupRespawnToggler()
{
	CBaseEntity* relay = CreateEntityByName("logic_relay");
	CEntityKeyValues* pKeyValues = new CEntityKeyValues();

	pKeyValues->SetString("targetname", "zr_toggle_respawn");
	relay->DispatchSpawn(pKeyValues);
	g_hRespawnToggler = relay->GetHandle();
}

void ZM_SetupCTeams()
{
	CTeam* pTeam = nullptr;
	while (nullptr != (pTeam = (CTeam*)UTIL_FindEntityByClassname(pTeam, "cs_team_manager")))
		if (pTeam->m_iTeamNum() == CS_TEAM_CT)
			g_hTeamCT = pTeam->GetHandle();
		else if (pTeam->m_iTeamNum() == CS_TEAM_T)
			g_hTeamT = pTeam->GetHandle();
}

void ZM_OnRoundStart(IGameEvent* pEvent)
{
	ZM_SetupRespawnToggler();
	ClientPrintAll(HUD_PRINTTALK, ZM_PREFIX "The game is \x05Humans vs. Zombies\x01, the goal for zombies is to infect all humans by knifing them.");

	if (g_cvarZMInfectSpawnWarning.Get() && g_cvarZMInfectSpawnType.Get() == (int)EZMSpawnType::ZM_IN_PLACE)
		ClientPrintAll(HUD_PRINTTALK, ZM_PREFIX "Classic spawn is enabled! Zombies will be \x07spawning between humans\x01!");

	if (!GetGlobals())
		return;

	for (int i = 0; i < GetGlobals()->maxClients; i++)
	{
		CCSPlayerController* pController = CCSPlayerController::FromSlot(i);

		if (!pController)
			continue;

		ZEPlayer* pPlayer = pController->GetZEPlayer();
		if (pPlayer)
		{
			pPlayer->SetHumanTeleUsages(0);
			pPlayer->SetZombieTeleUsages(0);
			pPlayer->SetHitsFromZombies(0);
			pPlayer->SetFrozen(false);
		}

		CCSPlayerPawn* pPawn = pController->GetPlayerPawn();

		// Now we can enable damage back
		if (pPawn)
			pPawn->m_bTakesDamage = true;
	}
}

void ZM_OnPlayerSpawn(CCSPlayerController* pController)
{
	// delay infection a bit
	bool bInfect = g_ZMRoundState == EZMRoundState::POST_INFECTION;

	// We're infecting this guy with a delay, disable all damage as they have 100 hp until then
	// also set team immediately in case the spawn teleport is team filtered
	if (bInfect)
	{
		pController->GetPawn()->m_bTakesDamage(false);
		pController->SwitchTeam(CS_TEAM_T);
	}
	else
	{
		pController->SwitchTeam(CS_TEAM_CT);
	}

	CHandle<CCSPlayerController> handle = pController->GetHandle();
	CTimer::Create(0.05f, TIMERFLAG_MAP | TIMERFLAG_ROUND, [handle, bInfect]() {
		CCSPlayerController* pController = (CCSPlayerController*)handle.Get();
		if (!pController)
			return -1.0f;
		if (bInfect)
			ZM_Infect(pController, pController, true);
		else
			ZM_Cure(pController);
		return -1.0f;
	});
}

void ZM_ApplyKnockback(CCSPlayerPawn* pHuman, CCSPlayerPawn* pVictim, float flDamage, const char* szWeapon, int hitgroup, float classknockback)
{
	std::shared_ptr<ZRWeapon> pWeapon = g_pZRWeaponConfig->FindWeapon(szWeapon);
	std::shared_ptr<ZRHitgroup> pHitgroup = g_pZRHitgroupConfig->FindHitgroupIndex(hitgroup);
	// player shouldn't be able to pick up that weapon in the first place, but just in case
	if (!pWeapon)
		return;
	float flWeaponKnockbackScale = pWeapon->flKnockback;
	float flHitgroupKnockbackScale = 1.0f;

	if (pHitgroup)
		flHitgroupKnockbackScale = pHitgroup->flKnockback;

	Vector vecKnockback;
	AngleVectors(pHuman->m_angEyeAngles(), &vecKnockback);
	vecKnockback *= (flDamage * g_cvarZMKnockbackScale.Get() * flWeaponKnockbackScale * flHitgroupKnockbackScale * classknockback);
	pVictim->m_vecAbsVelocity = pVictim->m_vecAbsVelocity() + vecKnockback;
}

void ZM_ApplyKnockbackExplosion(CBaseEntity* pProjectile, CCSPlayerPawn* pVictim, int iDamage, bool bMolotov)
{
	std::shared_ptr<ZRWeapon> pWeapon = g_pZRWeaponConfig->FindWeapon(pProjectile->GetClassname());
	if (!pWeapon)
		return;
	float flWeaponKnockbackScale = pWeapon->flKnockback;

	Vector vecDisplacement = pVictim->GetAbsOrigin() - pProjectile->GetAbsOrigin();
	vecDisplacement.z += 36;
	VectorNormalize(vecDisplacement);
	Vector vecKnockback = vecDisplacement;

	if (bMolotov)
		vecKnockback.z = 0;

	vecKnockback *= (iDamage * g_cvarZMKnockbackScale.Get() * flWeaponKnockbackScale);
	pVictim->m_vecAbsVelocity = pVictim->m_vecAbsVelocity() + vecKnockback;
}

void ZM_FakePlayerDeath(CCSPlayerController* pAttackerController, CCSPlayerController* pVictimController, const char* szWeapon, bool bDontBroadcast)
{
	if (!pVictimController->m_bPawnIsAlive())
		return;

	IGameEvent* pEvent = g_gameEventManager->CreateEvent("player_death");

	if (!pEvent)
		return;

	pEvent->SetPlayer("userid", pVictimController->GetPlayerSlot());
	pEvent->SetPlayer("attacker", pAttackerController->GetPlayerSlot());
	pEvent->SetInt("assister", 65535);
	pEvent->SetInt("assister_pawn", -1);
	pEvent->SetString("weapon", szWeapon);
	pEvent->SetBool("infected", true);

	g_gameEventManager->FireEvent(pEvent, bDontBroadcast);
}

void ZM_StripAndGiveKnife(CCSPlayerPawn* pPawn)
{
	CCSPlayer_ItemServices* pItemServices = pPawn->m_pItemServices();
	CCSPlayer_WeaponServices* pWeaponServices = pPawn->m_pWeaponServices();

	// it can sometimes be null when player joined on the very first round?
	if (!pItemServices || !pWeaponServices)
		return;

	pPawn->DropMapWeapons();
	pItemServices->StripPlayerWeapons(true);

	if (pPawn->m_iTeamNum == CS_TEAM_T)
	{
		pItemServices->GiveNamedItem("weapon_knife_t");
	}
	else if (pPawn->m_iTeamNum == CS_TEAM_CT)
	{
		pItemServices->GiveNamedItem("weapon_knife");

		ConVarRefAbstract mp_free_armor("mp_free_armor");

		if (mp_free_armor.GetInt() == 1 || g_cvarFreeArmor.GetInt() == 1)
			pItemServices->GiveNamedItem("item_kevlar");
		else if (mp_free_armor.GetInt() == 2 || g_cvarFreeArmor.GetInt() == 2)
			pItemServices->GiveNamedItem("item_assaultsuit");
	}

	CUtlVector<CHandle<CBasePlayerWeapon>>* weapons = pWeaponServices->m_hMyWeapons();

	FOR_EACH_VEC(*weapons, i)
	{
		CBasePlayerWeapon* pWeapon = (*weapons)[i].Get();

		if (pWeapon && pWeapon->GetWeaponVData()->m_GearSlot() == GEAR_SLOT_KNIFE)
		{
			// Normally this isn't necessary, but there's a small window if infected right after throwing a grenade where this is needed
			pWeaponServices->SelectItem(pWeapon);
			break;
		}
	}
}

void ZM_Cure(CCSPlayerController* pTargetController)
{
	if (pTargetController->m_iTeamNum() == CS_TEAM_T)
		pTargetController->SwitchTeam(CS_TEAM_CT);

	ZEPlayer* pZEPlayer = pTargetController->GetZEPlayer();

	if (pZEPlayer)
		pZEPlayer->SetInfectState(false);

	CCSPlayerPawn* pTargetPawn = (CCSPlayerPawn*)pTargetController->GetPawn();
	if (!pTargetPawn)
		return;

	g_pZRPlayerClassManager->ApplyPreferredOrDefaultHumanClass(pTargetPawn);
}

float ZM_MoanTimer(ZEPlayerHandle hPlayer)
{
	if (!hPlayer.IsValid())
		return -1.f;

	if (!hPlayer.Get()->IsInfected())
		return -1.f;

	CCSPlayerPawn* pPawn = CCSPlayerController::FromSlot(hPlayer.GetPlayerSlot())->GetPlayerPawn();

	if (!pPawn || pPawn->m_iTeamNum == CS_TEAM_CT)
		return -1.f;

	// This guy is dead but still infected, and corpses are quiet
	if (!pPawn->IsAlive())
		return g_cvarZMMoanInterval.Get();

	pPawn->EmitSound("zr.amb.zombie_voice_idle");

	return g_cvarZMMoanInterval.Get();
}

void ZM_InfectShake(CCSPlayerController* pController)
{
	if (!pController || !pController->IsConnected() || pController->IsBot() || !g_cvarZMInfectShake.Get())
		return;

	INetworkMessageInternal* pNetMsg = g_pNetworkMessages->FindNetworkMessagePartial("Shake");

	auto data = pNetMsg->AllocateMessage()->ToPB<CUserMessageShake>();

	data->set_duration(g_cvarZMInfectShakeDuration.Get());
	data->set_frequency(g_cvarZMInfectShakeFrequency.Get());
	data->set_amplitude(g_cvarZMInfectShakeAmplitude.Get());
	data->set_command(0);

	CSingleRecipientFilter filter(pController->GetPlayerSlot());
	g_gameEventSystem->PostEventAbstract(-1, false, &filter, pNetMsg, data, 0);

	delete data;
}

std::vector<SpawnPoint*> ZM_GetSpawns()
{
	std::vector<SpawnPoint*> spawns;

	if (!g_pGameRules)
		return spawns;

	auto ctSpawns = g_pGameRules->m_CTSpawnPoints();
	auto tSpawns = g_pGameRules->m_TerroristSpawnPoints();

	FOR_EACH_VEC(*ctSpawns, i)
	{
		SpawnPoint* pSpawn = (*ctSpawns)[i].Get();

		if (pSpawn)
			spawns.push_back(pSpawn);
	}

	FOR_EACH_VEC(*tSpawns, i)
	{
		SpawnPoint* pSpawn = (*tSpawns)[i].Get();

		if (pSpawn)
			spawns.push_back(pSpawn);
	}

	if (!spawns.size())
		Panic("There are no spawns!\n");

	return spawns;
}

void ZM_Infect(CCSPlayerController* pAttackerController, CCSPlayerController* pVictimController, bool bDontBroadcast)
{
	// This can be null if the victim disconnected right before getting hit AND someone joined in their place immediately, thus replacing the controller
	if (!pVictimController)
		return;

	if (pVictimController->m_iTeamNum() == CS_TEAM_CT)
		pVictimController->SwitchTeam(CS_TEAM_T);

	ZM_CheckTeamWinConditions(CS_TEAM_T);

	ZM_FakePlayerDeath(pAttackerController, pVictimController, "knife", bDontBroadcast); // or any other killicon

	CCSPlayerPawn* pVictimPawn = (CCSPlayerPawn*)pVictimController->GetPawn();
	if (!pVictimPawn)
		return;

	ZEPlayer* pAttacker = pAttackerController->GetZEPlayer();
	if (pAttacker)
	{
		int health = 0;
		std::shared_ptr<ZRClass> activeClass = pAttacker->GetActiveZRClass();
		if (activeClass && activeClass->iTeam == CS_TEAM_T)
			health = static_pointer_cast<ZRZombieClass>(activeClass)->iHealthOnZombification;

		if (health > 0)
		{
			CCSPlayerPawn* pAttackerPawn = (CCSPlayerPawn*)pAttackerController->GetPawn();
			if (!pAttackerPawn)
				return;

			if (pAttackerPawn->m_iHealth() < pAttackerPawn->m_iMaxHealth())
			{
				int iHealth = pAttackerPawn->m_iHealth() + health;
				pAttackerPawn->m_iHealth = pAttackerPawn->m_iMaxHealth() < iHealth ? pAttackerPawn->m_iMaxHealth() : iHealth;
			}
		}
	}

	// We disabled damage due to the delayed infection, restore
	pVictimPawn->m_bTakesDamage(true);

	pVictimPawn->EmitSound("zr.amb.scream");

	ZM_StripAndGiveKnife(pVictimPawn);

	g_pZRPlayerClassManager->ApplyPreferredOrDefaultZombieClass(pVictimPawn);

	ZM_InfectShake(pVictimController);

	ZEPlayer* pZEPlayer = pVictimController->GetZEPlayer();

	if (pZEPlayer && !pZEPlayer->IsInfected())
	{
		pZEPlayer->SetInfectState(true);

		ZEPlayerHandle hPlayer = pZEPlayer->GetHandle();
		CTimer::Create(rand() % (int)g_cvarZMMoanInterval.Get(), TIMERFLAG_MAP | TIMERFLAG_ROUND, [hPlayer]() { return ZM_MoanTimer(hPlayer); });
	}
}

void ZM_InfectMotherZombie(CCSPlayerController* pVictimController, std::vector<SpawnPoint*> spawns)
{
	CCSPlayerPawn* pVictimPawn = (CCSPlayerPawn*)pVictimController->GetPawn();
	if (!pVictimPawn)
		return;

	pVictimController->SwitchTeam(CS_TEAM_T);

	ZM_FakePlayerDeath(pVictimController, pVictimController, "knife", true); // not sent to clients

	ZM_StripAndGiveKnife(pVictimPawn);

	// pick random spawn point
	if (g_cvarZMInfectSpawnType.Get() == (int)EZMSpawnType::ZM_RESPAWN)
	{
		int randomindex = rand() % spawns.size();
		Vector origin = spawns[randomindex]->GetAbsOrigin();
		QAngle rotation = spawns[randomindex]->GetAbsRotation();

		pVictimPawn->Teleport(&origin, &rotation, &vec3_origin);
	}

	pVictimPawn->EmitSound("zr.amb.scream");

	std::shared_ptr<ZRZombieClass> pClass = g_pZRPlayerClassManager->GetZombieClass("MotherZombie");
	if (pClass)
		g_pZRPlayerClassManager->ApplyZombieClass(pClass, pVictimPawn);
	else
		g_pZRPlayerClassManager->ApplyPreferredOrDefaultZombieClass(pVictimPawn);

	ZM_InfectShake(pVictimController);

	ZEPlayer* pZEPlayer = pVictimController->GetZEPlayer();

	pZEPlayer->SetInfectState(true);

	ZEPlayerHandle hPlayer = pZEPlayer->GetHandle();
	CTimer::Create(rand() % (int)g_cvarZMMoanInterval.Get(), TIMERFLAG_MAP | TIMERFLAG_ROUND, [hPlayer]() { return ZM_MoanTimer(hPlayer); });

	g_MotherZombies.push_back(hPlayer);
}

// make players who've been picked as MZ recently less likely to be picked again
// store a variable in ZEPlayer, which gets initialized with value 100 if they are picked to be a mother zombie
// the value represents a % chance of the player being skipped next time they are picked to be a mother zombie
// If the player is skipped, next random player is picked to be mother zombie (and same skip chance logic applies to him)
// the variable gets decreased by 20 every round
void ZM_InitialInfection()
{
	if (!GetGlobals())
		return;

	// mz infection candidates
	std::vector<CCSPlayerController*> vecCandidateControllers;
	for (int i = 0; i < GetGlobals()->maxClients; i++)
	{
		CCSPlayerController* pController = CCSPlayerController::FromSlot(i);
		if (!pController || !pController->IsConnected() || pController->m_iTeamNum() != CS_TEAM_CT)
			continue;

		CCSPlayerPawn* pPawn = (CCSPlayerPawn*)pController->GetPawn();
		if (!pPawn || !pPawn->IsAlive())
			continue;

		vecCandidateControllers.push_back(pController);
	}

	// the num of mz to infect
	int iMZToInfect = vecCandidateControllers.size() / g_cvarZMInfectSpawnMZRatio.Get();
	iMZToInfect = g_cvarZMInfectSpawnMinCount.Get() > iMZToInfect ? g_cvarZMInfectSpawnMinCount.Get() : iMZToInfect;
	bool vecIsMZ[MAXPLAYERS] = {false};

	if (vecCandidateControllers.size() < g_cvarZMInfectSpawnMinCountReq.Get())
		iMZToInfect = 0;

	// get spawn points
	std::vector<SpawnPoint*> spawns = ZM_GetSpawns();
	if (g_cvarZMInfectSpawnType.Get() == (int)EZRSpawnType::RESPAWN && !spawns.size())
	{
		ClientPrintAll(HUD_PRINTTALK, ZM_PREFIX "There are no spawns!");
		return;
	}

	// infect
	int iFailSafeCounter = 0;
	while (iMZToInfect > 0)
	{
		// If we somehow don't have enough mother zombies after going through the players 5 times,
		if (iFailSafeCounter >= 5)
		{
			for (int i = 0; i < vecCandidateControllers.size(); i++)
			{
				// at 5, reset everyone's immunity but mother zombies from this and last round
				// at 6, reset everyone's immunity but mother zombies from this round
				ZEPlayer* pPlayer = vecCandidateControllers[i]->GetZEPlayer();
				if (pPlayer->GetImmunity() < 100 || (iFailSafeCounter >= 6 && !vecIsMZ[i]))
					pPlayer->SetImmunity(0);
			}
		}

		// a list of player who survived the previous mz roll of this round
		std::vector<CCSPlayerController*> vecSurvivorControllers;
		for (CCSPlayerController* pController : vecCandidateControllers)
		{
			// don't even bother with picked mz or player with 100 immunity
			ZEPlayer* pPlayer = pController->GetZEPlayer();
			if (pPlayer && pPlayer->GetImmunity() < 100)
				vecSurvivorControllers.push_back(pController);
		}

		// no enough human even after triggering fail safe
		if (iFailSafeCounter >= 6 && vecSurvivorControllers.size() == 0)
			break;

		while (vecSurvivorControllers.size() > 0 && iMZToInfect > 0)
		{
			int randomindex = rand() % vecSurvivorControllers.size();

			CCSPlayerController* pController = (CCSPlayerController*)vecSurvivorControllers[randomindex];
			CCSPlayerPawn* pPawn = (CCSPlayerPawn*)pController->GetPawn();
			ZEPlayer* pPlayer = vecSurvivorControllers[randomindex]->GetZEPlayer();
			// roll for immunity
			if (rand() % 100 < pPlayer->GetImmunity())
			{
				vecSurvivorControllers.erase(vecSurvivorControllers.begin() + randomindex);
				continue;
			}

			ZM_InfectMotherZombie(pController, spawns);
			pPlayer->SetImmunity(100);
			vecIsMZ[pPlayer->GetPlayerSlot().Get()] = true;

			iMZToInfect--;
		}
		iFailSafeCounter++;
	}

	// reduce everyone's immunity except mz
	for (int i = 0; i < GetGlobals()->maxClients; i++)
	{
		ZEPlayer* pPlayer = g_playerManager->GetPlayer(i);
		if (!pPlayer || vecIsMZ[i])
			continue;

		pPlayer->SetImmunity(pPlayer->GetImmunity() - g_cvarZMMZImmunityReduction.Get());
	}

	if (g_cvarZMRespawnDelay.Get() < 0.0f)
		g_bRespawnEnabled = false;

	SendHudMessageAll(4, EHudPriority::InfectionCountdown, "First infection has started!");
	ClientPrintAll(HUD_PRINTTALK, ZM_PREFIX "First infection has started! Good luck, survivors!");
	g_ZMRoundState = EZMRoundState::POST_INFECTION;
}

void ZM_StartInitialCountdown()
{
	if (g_cvarZMInfectSpawnTimeMin.Get() > g_cvarZMInfectSpawnTimeMax.Get())
		g_cvarZMInfectSpawnTimeMin.Set(g_cvarZMInfectSpawnTimeMax.Get());

	int iRand = rand();
	auto iSecondsElapsed = std::make_shared<int>(0);
	CTimer::Create(0.0f, TIMERFLAG_MAP | TIMERFLAG_ROUND, [iRand, iSecondsElapsed]() {
		if (g_ZMRoundState != EZMRoundState::ROUND_START)
			return -1.0f;

		int g_iInfectionCountDown = g_cvarZMInfectSpawnTimeMin.Get() + (iRand % (g_cvarZMInfectSpawnTimeMax.Get() - g_cvarZMInfectSpawnTimeMin.Get() + 1));
		g_iInfectionCountDown -= *iSecondsElapsed;

		if (g_iInfectionCountDown <= 0)
		{
			ZM_InitialInfection();
			return -1.0f;
		}

		if (g_cvarZMInfectCountdown.Get() && g_iInfectionCountDown <= 60)
		{
			char classicSpawnMsg[256];

			if (g_cvarZMInfectSpawnWarning.Get() && g_cvarZMInfectSpawnType.Get() == (int)EZMSpawnType::ZM_IN_PLACE)
				V_snprintf(classicSpawnMsg, sizeof(classicSpawnMsg), "<span color='#940000'>WARNING: </span><span color='#FF3333'>Zombies will spawn between humans!</span><br>\u00A0<br>");
			else
				V_snprintf(classicSpawnMsg, sizeof(classicSpawnMsg), "");

			SendHudMessageAll(2, EHudPriority::InfectionCountdown, "%sFirst infection in <span color='#00FF00'>%i %s</span>!", classicSpawnMsg, g_iInfectionCountDown, g_iInfectionCountDown == 1 ? "second" : "seconds");

			if (g_iInfectionCountDown % 5 == 0)
				ClientPrintAll(HUD_PRINTTALK, "%sFirst infection in \7%i %s\1!", ZM_PREFIX, g_iInfectionCountDown, g_iInfectionCountDown == 1 ? "second" : "seconds");
		}
		(*iSecondsElapsed)++;

		return 1.0f;
	});
}

bool ZM_Hook_OnTakeDamage_Alive(CTakeDamageInfo* pInfo, CCSPlayerPawn* pVictimPawn, bool bNotAlive)
{
	CCSPlayerPawn* pAttackerPawn = (CCSPlayerPawn*)pInfo->m_hAttacker.Get();

	if (!(pAttackerPawn && pVictimPawn && pAttackerPawn->IsPawn() && pVictimPawn->IsPawn()))
		return false;
	
	CCSPlayerController* pAttackerController = CCSPlayerController::FromPawn(pAttackerPawn);
	CCSPlayerController* pVictimController = CCSPlayerController::FromPawn(pVictimPawn);

	if (bNotAlive)
	{
		const char* pszAbilityClass = pInfo->m_hAbility.Get() ? pInfo->m_hAbility.Get()->GetClassname() : "";
		if (pAttackerPawn->m_iTeamNum() == CS_TEAM_T && pVictimPawn->m_iTeamNum() == CS_TEAM_CT && !V_strncmp(pszAbilityClass, "weapon_knife", 12))
		{
			char msg[256];

			int times = 1;
			ZEPlayer* pPlayer = pVictimController->GetZEPlayer();
			if (pPlayer)
			{
				std::shared_ptr<ZRClass> activeClass = pPlayer->GetActiveZRClass();

				if (activeClass && activeClass->iTeam == CS_TEAM_CT)
					times = static_pointer_cast<ZRHumanClass>(activeClass)->iArmor;

				if (times > 1) // If armor is set.
				{
					int hits = pPlayer->GetHitsFromZombies();
					hits++;
					pPlayer->SetHitsFromZombies(hits);
					if (hits >= times)
					{
						// If hit more times than the armour then zombify
						ZM_Infect(pAttackerController, pVictimController, false);
					}
					return true; // nullify the damage
				}
			}

			ZM_Infect(pAttackerController, pVictimController, false);
			return true; // nullify the damage
		}
		return false;
	}
	else
	{
		if (g_cvarZMGroanChance.Get() && pVictimPawn->m_iTeamNum() == CS_TEAM_T && (rand() % g_cvarZMGroanChance.Get()) == 1)
			pVictimPawn->EmitSound("zr.amb.zombie_pain");

		// grenade and molotov knockback
		if (pAttackerPawn->m_iTeamNum() == CS_TEAM_CT && pVictimPawn->m_iTeamNum() == CS_TEAM_T)
		{
			CEntityInstance* pInflictor = pInfo->m_hInflictor.Get();
			const char* pszInflictorClass = pInflictor ? pInflictor->GetClassname() : "";
			// inflictor class from grenade damage is actually hegrenade_projectile
			bool bGrenade = V_strncmp(pszInflictorClass, "hegrenade", 9) == 0;
			bool bInferno = V_strncmp(pszInflictorClass, "inferno", 7) == 0;

			if (g_cvarZMNapalmGrenades.Get() && bGrenade)
			{
				// Scale burn duration by damage, so nades from farther away burn zombies for less time
				float flDuration = (pInfo->m_flDamage / g_cvarZMNapalmFullDamage.Get()) * g_cvarZMNapalmDuration.Get();
				flDuration = clamp(flDuration, 0.0f, g_cvarZMNapalmDuration.Get());

				// Can't use the same inflictor here as it'll end up calling this again each burn damage tick
				// DMG_BURN makes loud noises so use DMG_FALL instead which is completely silent
				IgnitePawn(pVictimPawn, flDuration, pAttackerPawn, pAttackerPawn, nullptr, DMG_FALL);
			}

			if (bGrenade || bInferno)
				ZM_ApplyKnockbackExplosion((CBaseEntity*)pInflictor, (CCSPlayerPawn*)pVictimPawn, (int)pInfo->m_flDamage, bInferno);
		}
		return false;
	}
}

// can prevent purchasing and picking it up
AcquireResult ZM_Detour_CCSPlayer_ItemServices_CanAcquire(CCSPlayer_ItemServices* pItemServices, CEconItemView* pEconItem)
{
	CCSPlayerPawn* pPawn = pItemServices->GetPawn();

	if (!pPawn)
		return AcquireResult::Allowed;

	const WeaponInfo_t* pWeaponInfo = FindWeaponInfoByItemDefIndex(pEconItem->m_iItemDefinitionIndex);

	if (!pWeaponInfo)
		return AcquireResult::Allowed;

	if (pPawn->m_iTeamNum() == CS_TEAM_T && !CCSPlayer_ItemServices::IsAwsProcessing() && V_strncmp(pWeaponInfo->m_pClass, "weapon_knife", 12) && V_strncmp(pWeaponInfo->m_pClass, "weapon_c4", 9))
		return AcquireResult::NotAllowedByTeam;
	if (pPawn->m_iTeamNum() == CS_TEAM_CT && !g_pZRWeaponConfig->FindWeapon(pWeaponInfo->m_pClass))
		return AcquireResult::NotAllowedByProhibition;

	// doesn't guarantee the player will acquire the weapon, it just allows the original function to run
	return AcquireResult::Allowed;
}

bool ZM_Detour_CEntityIdentity_AcceptInput(CEntityIdentity* pThis, CUtlSymbolLarge* pInputName, CEntityInstance* pActivator, CEntityInstance* pCaller, variant_t* value)
{
	const char* inputName = pInputName->String();

	if (!V_strcasecmp(pThis->GetClassname(), "info_map_parameters") && !V_strcasecmp(inputName, "FireWinCondition"))
	{
		switch (value->Get<int>())
		{
			case CSRoundEndReason::TargetBombed:
			case CSRoundEndReason::VIPKilled:
			case CSRoundEndReason::TerroristsEscaped:
			case CSRoundEndReason::TerroristWin:
			case CSRoundEndReason::HostagesNotRescued:
			case CSRoundEndReason::VIPNotEscaped:
			case CSRoundEndReason::CTSurrender:
			case CSRoundEndReason::TerroristsPlanted:
				ZM_FinishRound(CS_TEAM_T);
				return true;
			case CSRoundEndReason::VIPEscaped:
			case CSRoundEndReason::CTStoppedEscape:
			case CSRoundEndReason::TerroristsStopped:
			case CSRoundEndReason::BombDefused:
			case CSRoundEndReason::CTWin:
			case CSRoundEndReason::HostagesRescued:
			case CSRoundEndReason::TargetSaved:
			case CSRoundEndReason::TerroristsNotEscaped:
			case CSRoundEndReason::TerroristsSurrender:
			case CSRoundEndReason::CTsReachedHostage:
				ZM_FinishRound(CS_TEAM_CT);
				return true;
			// This would allow maps to mess with timeleft, so we're actually just going to block it entirely
			case CSRoundEndReason::GameStart:
				return false;
			case CSRoundEndReason::Draw:
			default:
				ZM_FinishRound(CS_TEAM_NONE);
				return true;
		}
	}

	if (!g_hRespawnToggler.IsValid())
		return true;

	CBaseEntity* relay = g_hRespawnToggler.Get();

	// Must be an input into our zr_toggle_respawn relay
	if (!relay || pThis != relay->m_pEntity)
		return true;

	if (!V_strcasecmp(inputName, "Trigger"))
		ZM_ToggleRespawn();
	else if (!V_strcasecmp(inputName, "Enable") && !g_bRespawnEnabled)
		ZM_ToggleRespawn(true, true);
	else if (!V_strcasecmp(inputName, "Disable") && g_bRespawnEnabled)
		ZM_ToggleRespawn(true, false);
	else
		return true;

	ClientPrintAll(HUD_PRINTTALK, ZM_PREFIX "Respawning is %s!", g_bRespawnEnabled ? "enabled" : "disabled");
	return true;
}

void ZM_SpawnPlayer(CCSPlayerController* pController)
{
	pController->ChangeTeam(g_ZMRoundState == EZMRoundState::POST_INFECTION ? CS_TEAM_T : CS_TEAM_CT);

	// Make sure the round ends if spawning into an empty server
	if (!ZM_IsTeamAlive(CS_TEAM_CT) && !ZM_IsTeamAlive(CS_TEAM_T) && g_ZMRoundState != EZMRoundState::ROUND_END)
	{
		if (!g_pGameRules)
			return;

		g_pGameRules->TerminateRound(1.0f, CSRoundEndReason::GameStart);
		g_ZMRoundState = EZMRoundState::ROUND_END;
		return;
	}

	CHandle<CCSPlayerController> handle = pController->GetHandle();
	CTimer::Create(2.0f, TIMERFLAG_MAP | TIMERFLAG_ROUND, [handle]() {
		CCSPlayerController* pController = (CCSPlayerController*)handle.Get();
		if (!pController || !g_bRespawnEnabled || pController->m_iTeamNum < CS_TEAM_T)
			return -1.0f;
		pController->Respawn();
		return -1.0f;
	});
}

void ZM_Hook_ClientPutInServer(CPlayerSlot slot, char const* pszName, int type, uint64 xuid)
{
	CCSPlayerController* pController = CCSPlayerController::FromSlot(slot);
	if (!pController)
		return;

	ZEPlayer* pPlayer = pController->GetZEPlayer();
	if (pPlayer)
	{
		pPlayer->SetHitsFromZombies(0);
		pPlayer->SetHumanTeleUsages(0);
		pPlayer->SetZombieTeleUsages(0);
		pPlayer->SetFrozen(false);
	}

	ZM_SpawnPlayer(pController);
}

void ZM_Hook_ClientCommand_JoinTeam(CPlayerSlot slot, const CCommand& args)
{
	CCSPlayerController* pController = CCSPlayerController::FromSlot(slot);
	if (!pController)
		return;

	CCSPlayerPawn* pPawn = (CCSPlayerPawn*)pController->GetPawn();
	if (pPawn && pPawn->IsAlive())
		pPawn->CommitSuicide(false, true);

	if (args.ArgC() >= 2 && !V_strcmp(args.Arg(1), "1"))
		pController->SwitchTeam(CS_TEAM_SPECTATOR);
	else if (pController->m_iTeamNum == CS_TEAM_SPECTATOR)
		ZM_SpawnPlayer(pController);
}

void ZM_OnPlayerTakeDamage(CCSPlayerPawn* pVictimPawn, const CTakeDamageInfo* pInfo, const float damage)
{
	// bullet & knife only
	if ((!(pInfo->m_bitsDamageType & DMG_BULLET) && !(pInfo->m_bitsDamageType & DMG_SLASH)) || !pInfo->m_pTrace || !pInfo->m_pTrace->m_pHitbox)
		return;

	const auto pVictimController = reinterpret_cast<CCSPlayerController*>(pVictimPawn->GetController());
	if (!pVictimController || !pVictimController->IsConnected())
		return;

	if (!pInfo->m_AttackerInfo.m_bIsPawn)
		return;

	const auto pKillerPawn = pInfo->m_AttackerInfo.m_hAttackerPawn.Get();
	if (!pKillerPawn || !pKillerPawn->IsPawn()) // I don't know why this maybe non-pawn entity??
		return;

	const auto pAbility = pInfo->m_hAbility.Get();
	if (!pAbility)
		return;

	const char* pszWeapon = pAbility->GetClassname();

	if (!V_strncasecmp(pszWeapon, "weapon_", 7))
		pszWeapon = reinterpret_cast<CBasePlayerWeapon*>(pAbility)->GetWeaponClassname();

	if (pKillerPawn->m_iTeamNum() == CS_TEAM_CT && pVictimPawn->m_iTeamNum() == CS_TEAM_T)
	{
		auto flClassKnockback = 1.0f;
		float flCashScale = g_cvarZMDamageCashScale.Get();

		if (flCashScale > 0)
		{
			const auto pKillerController = pKillerPawn->GetOriginalController();
			int money = pKillerController->m_pInGameMoneyServices->m_iAccount;
			pKillerController->m_pInGameMoneyServices->m_iAccount = money + (damage * flCashScale);
		}

		if (pVictimController->GetZEPlayer())
		{
			std::shared_ptr<ZRClass> activeClass = pVictimController->GetZEPlayer()->GetActiveZRClass();

			if (activeClass && activeClass->iTeam == CS_TEAM_T)
				flClassKnockback = static_pointer_cast<ZRZombieClass>(activeClass)->flKnockback;
		}

		ZM_ApplyKnockback(pKillerPawn, pVictimPawn, damage, pszWeapon, pInfo->m_pTrace->m_pHitbox->m_nGroupId, flClassKnockback);
	}
}

void ZM_OnPlayerDeath(IGameEvent* pEvent)
{
	// fake player_death, don't need to respawn or check win condition
	if (pEvent->GetBool("infected"))
		return;

	CCSPlayerController* pVictimController = (CCSPlayerController*)pEvent->GetPlayerController("userid");
	if (!pVictimController)
		return;
	CCSPlayerPawn* pVictimPawn = (CCSPlayerPawn*)pVictimController->GetPawn();
	if (!pVictimPawn)
		return;

	ZM_CheckTeamWinConditions(pVictimPawn->m_iTeamNum() == CS_TEAM_T ? CS_TEAM_CT : CS_TEAM_T);

	if (pVictimPawn->m_iTeamNum() == CS_TEAM_T && g_ZMRoundState == EZMRoundState::POST_INFECTION)
		pVictimPawn->EmitSound("zr.amb.zombie_die");

	// respawn player
	CHandle<CCSPlayerController> handle = pVictimController->GetHandle();
	CTimer::Create(g_cvarZMRespawnDelay.Get() < 0.0f ? 2.0f : g_cvarZMRespawnDelay.Get(), TIMERFLAG_MAP | TIMERFLAG_ROUND, [handle]() {
		CCSPlayerController* pController = (CCSPlayerController*)handle.Get();
		if (!pController || !g_bRespawnEnabled || pController->m_iTeamNum < CS_TEAM_T)
			return -1.0f;
		pController->Respawn();
		return -1.0f;
	});
}

void ZM_OnRoundFreezeEnd(IGameEvent* pEvent)
{
	ZM_StartInitialCountdown();
}

// there is probably a better way to check when time is running out...
void ZM_OnRoundTimeWarning(IGameEvent* pEvent)
{
	CTimer::Create(10.0, TIMERFLAG_MAP | TIMERFLAG_ROUND, []() {
		if (g_ZMRoundState == EZMRoundState::ROUND_END)
			return -1.0f;
		ZM_EndRound(g_cvarZMDefaultWinnerTeam.Get());
		return -1.0f;
	});
}

// check whether players on a team are all dead
bool ZM_IsTeamAlive(int iTeamNum)
{
	CCSPlayerPawn* pPawn = nullptr;
	while (nullptr != (pPawn = (CCSPlayerPawn*)UTIL_FindEntityByClassname(pPawn, "player")))
	{
		if (!pPawn->IsAlive())
			continue;

		if (pPawn->m_iTeamNum() == iTeamNum)
			return true;
	}
	return false;
}

// check whether a team has won the round, if so, end the round and incre score
bool ZM_CheckTeamWinConditions(int iTeamNum)
{
	if (g_ZMRoundState == EZMRoundState::ROUND_END || (iTeamNum == CS_TEAM_CT && g_bRespawnEnabled) || (iTeamNum != CS_TEAM_T && iTeamNum != CS_TEAM_CT))
		return false;

	// check the opposite team
	if (ZM_IsTeamAlive(iTeamNum == CS_TEAM_CT ? CS_TEAM_T : CS_TEAM_CT))
		return false;

	// allow the team to win
	ZM_EndRound(iTeamNum);

	return true;
}

void ZM_EndRound(int iTeamNum)
{
	bool bServerIdle = true;

	if (!GetGlobals() || !g_pGameRules)
		return;

	for (int i = 0; i < GetGlobals()->maxClients; i++)
	{
		ZEPlayer* pPlayer = g_playerManager->GetPlayer(i);

		if (g_cvarZMRunWithBots.Get())
		{
			// Don't skip any players, even bots.
			if (!pPlayer || (!(pPlayer->IsConnected() && pPlayer->IsInGame()) && !pPlayer->IsFakeClient()))
				continue;
		}
		else
		{
			// Only checks for connected real players for activity.
			if (!pPlayer || !pPlayer->IsConnected() || !pPlayer->IsInGame() || pPlayer->IsFakeClient())
				continue;
		}

		bServerIdle = false;
		break;
	}

	// Don't end rounds while the server is idling
	if (bServerIdle)
		return;

	CSRoundEndReason iReason;
	switch (iTeamNum)
	{
		case CS_TEAM_T:
			iReason = CSRoundEndReason::TerroristWin;
			break;
		case CS_TEAM_CT:
			iReason = CSRoundEndReason::CTWin;
			break;
		default:
			iReason = CSRoundEndReason::Draw;
			break;
	}

	static ConVarRefAbstract mp_round_restart_delay("mp_round_restart_delay");
	float flRestartDelay = mp_round_restart_delay.GetFloat();

	g_pGameRules->TerminateRound(flRestartDelay, iReason);
	ZM_FinishRound(iTeamNum);
}

void ZM_FinishRound(int iTeamNum)
{
	g_ZMRoundState = EZMRoundState::ROUND_END;
	ZM_ToggleRespawn(true, false);

	if (iTeamNum == CS_TEAM_CT)
	{
		if (!g_hTeamCT.Get())
		{
			Panic("Cannot find CTeam for CT!\n");
			return;
		}
		g_hTeamCT->m_iScore = g_hTeamCT->m_iScore() + 1;
		if (g_cvarZMHumanWinOverlayParticle.Get().Length() != 0)
		{
			CRecipientFilter filter;
			filter.AddAllPlayers();
			g_hTeamCT->DispatchParticle(g_cvarZMHumanWinOverlayParticle.Get().String(), &filter, PATTACH_MAIN_VIEW);
		}
	}
	else if (iTeamNum == CS_TEAM_T)
	{
		if (!g_hTeamT.Get())
		{
			Panic("Cannot find CTeam for T!\n");
			return;
		}
		g_hTeamT->m_iScore = g_hTeamT->m_iScore() + 1;
		if (g_cvarZMZombieWinOverlayParticle.Get().Length() != 0)
		{
			CRecipientFilter filter;
			filter.AddAllPlayers();
			g_hTeamT->DispatchParticle(g_cvarZMZombieWinOverlayParticle.Get().String(), &filter, PATTACH_MAIN_VIEW);
		}
	}
}

void ZM_CCSPlayer_WeaponServices_EquipWeapon(CCSPlayer_WeaponServices* pWeaponServices, CBasePlayerWeapon* pWeapon)
{
	if (g_cvarZMInfiniteAmmo.Get())
	{
		if (pWeapon && pWeapon->GetWeaponVData() && (pWeapon->GetWeaponVData()->m_GearSlot() == GEAR_SLOT_RIFLE || pWeapon->GetWeaponVData()->m_GearSlot() == GEAR_SLOT_PISTOL))
		{
			int ammo = g_cvarZMInfiniteAmmoTotal.Get();
			pWeapon->GetWeaponVData()->m_nPrimaryReserveAmmoMax() = ammo;
			pWeapon->GetWeaponVData()->m_iMaxClip1() = ammo;
			std::string sAmmo = std::to_string(ammo);
			pWeapon->AcceptInput("SetReserveAmmoAmount", sAmmo.c_str()); // 999 will be automatically clamped to the weapons m_nPrimaryReserveAmmoMax
		}
	}
}

void ZM_PostEventAbstract_SosStartSoundEvent(const uint64* pClients, CNetMessagePB<CMsgSosStartSoundEvent>* pMsg)
{
	static uint32 screamHash;
	static std::set<uint32> soundEventHashes;

	ExecuteOnce(
		screamHash = GetSoundEventHash("zr.amb.scream");
		soundEventHashes.insert(GetSoundEventHash("zr.amb.zombie_die"));
		soundEventHashes.insert(GetSoundEventHash("zr.amb.zombie_pain"));
		soundEventHashes.insert(GetSoundEventHash("zr.amb.zombie_voice_idle")););

	// Filter out people with zsounds disabled from hearing this sound
	if (pMsg->soundevent_hash() == screamHash)
		*(uint64*)pClients &= g_playerManager->GetZSoundsInfectMask();
	else if (soundEventHashes.contains(pMsg->soundevent_hash()))
		*(uint64*)pClients &= g_playerManager->GetZSoundsMask();
}

static void ZM_DrawLaserBetween(const std::vector<Vector>& startPos,
							 const std::vector<Vector>& endPos,
							 float durationSeconds)
{
	const size_t count = std::min(startPos.size(), endPos.size());

	for (size_t i = 0; i < count; ++i)
	{
		auto* beam = CreateEntityByName<CBeam>("env_beam");
		CEntityKeyValues* pKeyValues = new CEntityKeyValues();
		Color col(0, 255, 255);
		pKeyValues->SetString("rendercolor", "0 255 255");
		pKeyValues->SetString("width", "3.0");
		pKeyValues->SetInt("renderamt", 255);
		pKeyValues->SetInt("HDRColorScale", 3);
		pKeyValues->SetInt("spawnflags", 1);
		beam->m_clrRender() = col;
		beam->m_fWidth() = 3.0f;
		beam->m_bTurnedOff() = false;
		beam->m_nBeamFlags() = 512;
		beam->DispatchSpawn(pKeyValues);
		beam->Teleport(&startPos[i], nullptr, nullptr);
		beam->m_vecEndPos() = endPos[i];


		CTimer::Create(durationSeconds, TIMERFLAG_MAP | TIMERFLAG_ROUND, [beam]() {
			if (beam)
				addresses::UTIL_Remove(beam); // or UTIL_Remove(beam)

			return -1.0f;
		});
	}
	
}

void ZM_CheckFrozenPlayers()
{
	if (!g_cvarZMFreezeGrenades.Get() || g_cvarZMFreezeTime.Get() < 1 || g_cvarZMFreezeRadius.Get() < 1)
		return;

	for (int i = 0; i < GetGlobals()->maxClients; i++)
	{
		ZEPlayer* pPlayer = g_playerManager->GetPlayer(i);
		CCSPlayerController* pController = CCSPlayerController::FromSlot(i);

		// If not player, alive or zombie then ignore.
		if (!pPlayer || !pController || (!pPlayer->IsFakeClient() && !pPlayer->IsConnected()) || !pPlayer->IsInGame() || !pController->IsAlive() || pController->m_iTeamNum() != CS_TEAM_T)
			continue;

		if (pPlayer->GetFrozen())
		{
			ZM_FreezePlayer(pPlayer, pController, true);
		}
	}
}

void ZM_FreezePlayer(ZEPlayer* pPlayer, CCSPlayerController* pController, bool freeze)
{
	auto velocity = pController->m_vecAbsVelocity();
	CCSPlayerPawn* pPawn = (CCSPlayerPawn*)pController->GetPawn();
	if (!pPawn)
		return;

	if (freeze)
	{
		velocity.x = -999999;
		velocity.y = -999999;
		velocity.z = -999999;

		pPawn->SetAbsVelocity(velocity);
		pPawn->m_vecAbsVelocity() = velocity;
		pPawn->m_flFriction(999999.0f);
		pPawn->SetMoveType(MoveType_t::MOVETYPE_NONE);
		pPawn->m_bTakesDamage(false);
	}
	else
	{
		velocity.x = 1;
		velocity.y = 1;
		velocity.z = 1;

		pPawn->SetAbsVelocity(velocity);
		pPawn->m_vecAbsVelocity() = velocity;
		pPawn->m_flFriction(1.0f);
		pPawn->SetMoveType(MoveType_t::MOVETYPE_WALK);
		pPawn->m_bTakesDamage(true);
	}
}

void ZM_DecoyExploded(IGameEvent* pEvent)
{
	
	if (!g_cvarZMFreezeGrenades.Get() || g_cvarZMFreezeTime.Get() < 1 || g_cvarZMFreezeRadius.Get() < 1)
		return;

	float x = pEvent->GetFloat("x");
	float y = pEvent->GetFloat("y");
	float z = pEvent->GetFloat("z");

	Vector vec(x, y, z);
	SphereEntity sphereEntity(vec, g_cvarZMFreezeRadius.Get());

	if (g_cvarZMFreezeLaser.Get())
		ZM_DrawLaserBetween(sphereEntity.CircleInnerPoints(), sphereEntity.CircleOuterPoints(), g_cvarZMFreezeTime.Get());

	auto decoy = g_pEntitySystem->GetEntityInstance(pEvent->GetEntityIndex("entityid"));

	// The ZMBIO freeze sound is handled from EconomyShopPlugin (EventDecoyStarted) instead of
	// here - guessing at the ZMBIO addon's compiled sound event name is much faster to iterate
	// on from C# (just rebuild the plugin) than from here (full CS2Fixes rebuild + redeploy).

	addresses::UTIL_Remove(decoy);


	for (int i = 0; i < GetGlobals()->maxClients; i++)
	{
		ZEPlayer* pPlayer = g_playerManager->GetPlayer(i);
		CCSPlayerController* pController = CCSPlayerController::FromSlot(i);

		if (!pPlayer || !pController)
			continue;

		// If not player, alive or zombie then ignore.
		if ((!pPlayer->IsFakeClient() && !pPlayer->IsConnected()) || !pPlayer->IsInGame() || !pController->IsAlive() || pController->m_iTeamNum() != CS_TEAM_T)
			continue;

		auto pPawn = pController->GetPawn();
		if (!pPawn)
			continue;

		auto origin = pPawn->GetAbsOrigin();
		if (sphereEntity.CollidesWithPoint(origin))
		{
			pPlayer->SetFrozen(true);

			// Visual: encase the frozen zombie in the ZMBIO ice cube prop (follows them while
			// frozen), removed again the moment they thaw out.
			CBaseModelEntity* pIceCube = CreateEntityByName<CBaseModelEntity>("prop_dynamic");
			CEntityKeyValues* pIceCubeKeyValues = new CEntityKeyValues();
			pIceCubeKeyValues->SetString("model", "weapons/models/net4all/freeze_grenade/ice_cube/ice_cube.vmdl");
			pIceCube->DispatchSpawn(pIceCubeKeyValues);
			pIceCube->Teleport(&origin, nullptr, nullptr);
			pIceCube->AcceptInput("FollowEntity", "!activator", pPawn);

			CHandle<CBaseModelEntity> hIceCube(pIceCube);

			CTimer::Create(g_cvarZMFreezeTime.Get(), TIMERFLAG_MAP | TIMERFLAG_ROUND, [pPlayer, pController, hIceCube]() {
				if (pPlayer && pController)
				{
					pPlayer->SetFrozen(false);
					ZM_FreezePlayer(pPlayer, pController, false);
				}

				CBaseModelEntity* pIceCube = hIceCube.Get();
				if (pIceCube)
				{
					// Shatter into the model's own fracture pieces instead of just vanishing
					pIceCube->AcceptInput("Break");

					CTimer::Create(0.3f, TIMERFLAG_MAP | TIMERFLAG_ROUND, [hIceCube]() {
						CBaseModelEntity* pIceCube = hIceCube.Get();
						if (pIceCube)
							pIceCube->Remove();

						return -1.0f;
					});
				}

				return -1.0f;
			});
		}

	}
}

CON_COMMAND_CHAT(zmsounds, "- Toggle zombie sounds")
{
	if (!player)
	{
		ClientPrint(player, HUD_PRINTCONSOLE, ZM_PREFIX "You cannot use this command from the server console.");
		return;
	}

	int iPlayer = player->GetPlayerSlot();
	EZSoundsType state = g_playerManager->GetPlayerZSoundsMode(iPlayer);

	if (state == EZSoundsType::ON)
	{
		state = EZSoundsType::INFECT_ONLY;
		ClientPrint(player, HUD_PRINTTALK, ZM_PREFIX "Zombie sounds\x04 enabled\x10 (Infect sounds only).");
	}
	else if (state == EZSoundsType::INFECT_ONLY)
	{
		state = EZSoundsType::OFF;
		ClientPrint(player, HUD_PRINTTALK, ZM_PREFIX "Zombie sounds\x07 disabled.");
	}
	else if (state == EZSoundsType::OFF)
	{
		state = EZSoundsType::ON;
		ClientPrint(player, HUD_PRINTTALK, ZM_PREFIX "Zombie sounds\x04 enabled.");
	}

	g_playerManager->SetPlayerZSounds(iPlayer, state);
}

void ConVarZMEnableChange(CConVar<bool>* cvar, CSplitScreenSlot nSlot, const bool* pNewValue, const bool* pOldValue)
{
	const char* message = "Cannot enable ZombieMod and Zombie:Reborn enabled at the same time.\n";

	if (!V_strcmp(cvar->GetName(), g_cvarZMEnable.GetName()))
	{
		if (*pNewValue == true && g_cvarEnableZR.Get())
		{
			cvar->Set(false);
			Message(message);
			return;
		}
	}

	if (!V_strcmp(cvar->GetName(), g_cvarEnableZR.GetName()))
	{
		if (*pNewValue == true && g_cvarZMEnable.Get())
		{
			cvar->Set(false);
			Message(message);
			return;
		}
	}
}

CON_COMMAND_CHAT(zmtele, "- Teleport to spawn")
{
	// Silently return so the command is completely hidden
	if (!g_cvarZMEnable.Get())
		return;

	if (!player)
	{
		ClientPrint(player, HUD_PRINTCONSOLE, ZM_PREFIX "You cannot use this command from the server console.");
		return;
	}

	// Check if command is enabled for humans
	if (!g_cvarZMZteleHuman.Get() && player->m_iTeamNum() == CS_TEAM_CT)
	{
		ClientPrint(player, HUD_PRINTTALK, ZM_PREFIX "You cannot use this command as a human.");
		return;
	}

	// Check if command is enabled for zombies
	if (!g_cvarZMZteleZombie.Get() && player->m_iTeamNum() == CS_TEAM_T)
	{
		ClientPrint(player, HUD_PRINTTALK, ZM_PREFIX "You cannot use this command as a zombie.");
		return;
	}

	if (g_cvarZMZteleHuman.Get() && player->m_iTeamNum() == CS_TEAM_CT)
	{
		if (g_cvarZMTeleHumanBefore.Get() && !g_cvarZMTeleHumanAfter.Get() && g_ZMRoundState == EZMRoundState::POST_INFECTION)
		{
			ClientPrint(player, HUD_PRINTTALK, ZM_PREFIX "You can only use this command as a human before a zombie has been chosen.");
			return;
		}
		if (!g_cvarZMTeleHumanBefore.Get() && g_cvarZMTeleHumanAfter.Get() && g_ZMRoundState != EZMRoundState::POST_INFECTION)
		{
			ClientPrint(player, HUD_PRINTTALK, ZM_PREFIX "You can only use this command as a human after a zombie has been chosen.");
			return;
		}
	}

	std::vector<SpawnPoint*> spawns = ZM_GetSpawns();
	if (!spawns.size())
	{
		ClientPrint(player, HUD_PRINTTALK, ZM_PREFIX "There are no spawns!");
		return;
	}

	// Pick and get random spawnpoint
	int randomindex = rand() % spawns.size();
	CHandle<SpawnPoint> spawnHandle = spawns[randomindex]->GetHandle();

	// Here's where the mess starts
	CBasePlayerPawn* pPawn = player->GetPawn();

	if (!pPawn)
		return;

	if (!pPawn->IsAlive())
	{
		ClientPrint(player, HUD_PRINTTALK, ZM_PREFIX "You cannot teleport when dead!");
		return;
	}

	int iSlot = player->GetPlayerSlot();
	ZEPlayer* pPlayer = g_playerManager->GetPlayer(iSlot);
	if (!pPlayer)
	{
		Message("Engine failed to find ZEPlayer for ztele");
		return;
	}

	if (player->m_iTeamNum() == CS_TEAM_CT && pPlayer->GetHumanTeleUsages() >= g_cvarZMTeleMaxHuman.Get())
	{
		ClientPrint(player, HUD_PRINTTALK, ZM_PREFIX "You have already teleported this round more than the maximum allowed number of times for a human (%d)!", g_cvarZMTeleMaxHuman.Get());
		return;
	}

	if (player->m_iTeamNum() == CS_TEAM_T && pPlayer->GetZombieTeleUsages() >= g_cvarZMTeleMaxZombie.Get())
	{
		ClientPrint(player, HUD_PRINTTALK, ZM_PREFIX "You have already teleported this round more than the maximum allowed number of times for a zombie (%d)!", g_cvarZMTeleMaxZombie.Get());
		return;
	}

	// Get initial player position so we can do distance check
	Vector initialpos = pPawn->GetAbsOrigin();

	float timeToTele = 0.0f;

	if (!pPlayer->IsInfected())
	{
		pPlayer->SetHumanTeleUsages(pPlayer->GetHumanTeleUsages() + 1);
		timeToTele = g_cvarZMTeleDelayHuman.Get();
	}
	else if (pPlayer->IsInfected())
	{
		pPlayer->SetZombieTeleUsages(pPlayer->GetZombieTeleUsages() + 1);
		timeToTele = g_cvarZMTeleDelayZombie.Get();
	}
	else
	{
		// Should not happen but what if...
		timeToTele = 5.0f;
	}

	ClientPrint(player, HUD_PRINTTALK, ZM_PREFIX "Teleporting to spawn in %.2f seconds.", timeToTele);

	CHandle<CCSPlayerPawn> pawnHandle = pPawn->GetHandle();

	CTimer::Create(timeToTele, TIMERFLAG_MAP | TIMERFLAG_ROUND, [spawnHandle, pawnHandle, initialpos]() {
		CCSPlayerPawn* pPawn = pawnHandle.Get();
		SpawnPoint* pSpawn = spawnHandle.Get();

		if (!pPawn || !pSpawn)
			return -1.0f;

		Vector endpos = pPawn->GetAbsOrigin();

		if (initialpos.DistTo(endpos) < g_cvarZMMaxZteleDistance.Get())
		{
			Vector origin = pSpawn->GetAbsOrigin();
			QAngle rotation = pSpawn->GetAbsRotation();

			pPawn->Teleport(&origin, &rotation, nullptr);
			ClientPrint(pPawn->GetOriginalController(), HUD_PRINTTALK, ZM_PREFIX "You have been teleported to spawn.");
		}
		else
		{
			ClientPrint(pPawn->GetOriginalController(), HUD_PRINTTALK, ZM_PREFIX "Teleport failed! You moved too far.");
		}

		return -1.0f;
	});
}

CON_COMMAND_CHAT(zmclass, "<teamname/class name/number> - Find and select your ZM classes")
{
	// Silently return so the command is completely hidden
	if (!g_cvarZMEnable.Get())
		return;

	if (!player)
	{
		ClientPrint(player, HUD_PRINTCONSOLE, ZM_PREFIX "You cannot use this command from the server console.");
		return;
	}

	std::vector<std::shared_ptr<ZRClass>> vecClasses;
	int iSlot = player->GetPlayerSlot();
	bool bListingZombie = true;
	bool bListingHuman = true;

	if (args.ArgC() > 1)
	{
		bListingZombie = !V_strcasecmp(args[1], "zombie") || !V_strcasecmp(args[1], "zm") || !V_strcasecmp(args[1], "z");
		bListingHuman = !V_strcasecmp(args[1], "human") || !V_strcasecmp(args[1], "hm") || !V_strcasecmp(args[1], "h");
	}

	g_pZRPlayerClassManager->GetZRClassList(CS_TEAM_NONE, vecClasses, player);

	if (bListingZombie || bListingHuman)
	{
		for (int team = CS_TEAM_T; team <= CS_TEAM_CT; team++)
		{
			if ((team == CS_TEAM_T && !bListingZombie) || (team == CS_TEAM_CT && !bListingHuman))
				continue;

			const char* sTeamName = team == CS_TEAM_CT ? "Human" : "Zombie";
			const char* sCurrentClass = g_pUserPreferencesSystem->GetPreference(iSlot, team == CS_TEAM_CT ? HUMAN_CLASS_KEY_NAME : ZOMBIE_CLASS_KEY_NAME);

			if (sCurrentClass[0] != '\0')
				ClientPrint(player, HUD_PRINTTALK, ZM_PREFIX "Your current %s class is: \x10%s\x1. Available classes:", sTeamName, sCurrentClass);
			else
				ClientPrint(player, HUD_PRINTTALK, ZM_PREFIX "Available %s classes:", sTeamName);

			for (int i = 0; i < vecClasses.size(); i++)
				if (vecClasses[i]->iTeam == team)
					ClientPrint(player, HUD_PRINTTALK, "%i. %s", i + 1, vecClasses[i]->szClassName.c_str());
		}

		ClientPrint(player, HUD_PRINTTALK, ZM_PREFIX "Select a class using \x2!zclass <class name/number>");
		return;
	}

	for (int i = 0; i < vecClasses.size(); i++)
	{
		const char* sClassName = vecClasses[i]->szClassName.c_str();
		bool bClassMatches = !V_stricmp(sClassName, args[1]) || (V_StringToInt32(args[1], -1, NULL, NULL, PARSING_FLAG_SKIP_WARNING) - 1) == i;
		std::shared_ptr<ZRClass> pClass = vecClasses[i];

		if (bClassMatches)
		{
			ClientPrint(player, HUD_PRINTTALK, ZM_PREFIX "Your %s class is now set to \x10%s\x1.", pClass->iTeam == CS_TEAM_CT ? "Human" : "Zombie", sClassName);
			g_pUserPreferencesSystem->SetPreference(iSlot, pClass->iTeam == CS_TEAM_CT ? HUMAN_CLASS_KEY_NAME : ZOMBIE_CLASS_KEY_NAME, sClassName);
			return;
		}
	}

	ClientPrint(player, HUD_PRINTTALK, ZM_PREFIX "No available classes matched \x10%s\x1.", args[1]);
}

CON_COMMAND_CHAT_FLAGS(zminfect, "- Infect a player", ADMFLAG_GENERIC)
{
	// Silently return so the command is completely hidden
	if (!g_cvarZMEnable.Get())
		return;

	if (args.ArgC() < 2)
	{
		ClientPrint(player, HUD_PRINTTALK, ZM_PREFIX "Usage: !infect <name>");
		return;
	}

	if (g_ZMRoundState == EZMRoundState::ROUND_END)
	{
		ClientPrint(player, HUD_PRINTTALK, ZM_PREFIX "The round is already over!");
		return;
	}

	int iNumClients = 0;
	int pSlots[MAXPLAYERS];
	ETargetType nType;

	if (!g_playerManager->CanTargetPlayers(player, args[1], iNumClients, pSlots, NO_TERRORIST | NO_DEAD, nType))
		return;

	std::string strCommandPlayerName = player ? player->GetPlayerName() : CONSOLE_NAME;
	std::vector<SpawnPoint*> spawns = ZM_GetSpawns();

	if (g_cvarZMInfectSpawnType.Get() == (int)EZMSpawnType::ZM_RESPAWN && !spawns.size())
	{
		ClientPrint(player, HUD_PRINTTALK, ZM_PREFIX "There are no spawns!");
		return;
	}

	for (int i = 0; i < iNumClients; i++)
	{
		CCSPlayerController* pTarget = CCSPlayerController::FromSlot(pSlots[i]);
		CCSPlayerPawn* pPawn = (CCSPlayerPawn*)pTarget->GetPawn();

		if (g_ZMRoundState == EZMRoundState::ROUND_START)
			ZM_InfectMotherZombie(pTarget, spawns);
		else
			ZM_Infect(pTarget, pTarget, true);

		if (iNumClients == 1)
			PrintSingleAdminAction(strCommandPlayerName, pTarget->GetPlayerName(), "infected", g_ZMRoundState == EZMRoundState::ROUND_START ? " as a mother zombie" : "", ZM_PREFIX);
	}
	if (iNumClients > 1)
		PrintMultiAdminAction(nType, strCommandPlayerName, "infected", g_ZMRoundState == EZMRoundState::ROUND_START ? " as mother zombies" : "", ZM_PREFIX);

	// Note we skip MZ immunity when first infection is manually triggered
	if (g_ZMRoundState == EZMRoundState::ROUND_START)
	{
		if (g_cvarZMRespawnDelay.Get() < 0.0f)
			g_bRespawnEnabled = false;

		g_ZMRoundState = EZMRoundState::POST_INFECTION;
	}
}

CON_COMMAND_CHAT_FLAGS(zmrevive, "- Revive a player", ADMFLAG_GENERIC)
{
	// Silently return so the command is completely hidden
	if (!g_cvarZMEnable.Get())
		return;

	if (args.ArgC() < 2)
	{
		ClientPrint(player, HUD_PRINTTALK, ZM_PREFIX "Usage: !revive <name>");
		return;
	}

	if (g_ZMRoundState != EZMRoundState::POST_INFECTION)
	{
		ClientPrint(player, HUD_PRINTTALK, ZM_PREFIX "A round is not ongoing!");
		return;
	}

	int iNumClients = 0;
	int pSlots[MAXPLAYERS];
	ETargetType nType;

	if (!g_playerManager->CanTargetPlayers(player, args[1], iNumClients, pSlots, NO_DEAD | NO_COUNTER_TERRORIST, nType))
		return;

	std::string strCommandPlayerName = player ? player->GetPlayerName() : CONSOLE_NAME;

	for (int i = 0; i < iNumClients; i++)
	{
		CCSPlayerController* pTarget = CCSPlayerController::FromSlot(pSlots[i]);
		CCSPlayerPawn* pPawn = (CCSPlayerPawn*)pTarget->GetPawn();

		if (!pPawn)
			continue;

		ZM_Cure(pTarget);
		ZM_StripAndGiveKnife(pPawn);

		if (iNumClients == 1)
			PrintSingleAdminAction(strCommandPlayerName, pTarget->GetPlayerName(), "revived", "", ZM_PREFIX);
	}
	if (iNumClients > 1)
		PrintMultiAdminAction(nType, strCommandPlayerName, "revived", "", ZM_PREFIX);
}

// Lets external plugins (e.g. EconomyShopPlugin's LaserMine) kill a player via real attacker
// damage instead of CommitSuicide(), so the kill goes through the normal damage/death pipeline
// with proper attacker attribution (kill feed, scoreboard, on-kill rewards). Server console only
// (no FCVAR_CLIENT_CAN_EXECUTE) so a player can't invoke it against someone else from their own
// client console.
CON_COMMAND_F(zm_mine_kill, "<victim_userid> <owner_userid> - Kill a player, attributing the kill to another player", FCVAR_SPONLY | FCVAR_LINKED_CONCOMMAND)
{
	if (args.ArgC() < 3)
		return;

	CCSPlayerController* pVictim = CCSPlayerController::FromSlot(g_playerManager->GetSlotFromUserId(V_StringToUint16(args[1], 0)).Get());
	CCSPlayerPawn* pVictimPawn = pVictim ? pVictim->GetPlayerPawn() : nullptr;

	if (!pVictimPawn || !pVictimPawn->IsAlive())
		return;

	// World entity - used as the inflictor so the kill feed weapon icon isn't the owner's
	// currently held weapon. The owner is still credited as the attacker (kill feed name,
	// scoreboard, on-kill rewards) regardless of what the inflictor is.
	CBaseEntity* pWorld = (CBaseEntity*)g_pEntitySystem->GetEntityInstance(CEntityIndex(0));
	CBaseEntity* pAttacker = pWorld;

	CCSPlayerController* pOwner = CCSPlayerController::FromSlot(g_playerManager->GetSlotFromUserId(V_StringToUint16(args[2], 0)).Get());
	CCSPlayerPawn* pOwnerPawn = pOwner ? pOwner->GetPlayerPawn() : nullptr;

	if (pOwnerPawn && pOwnerPawn->IsAlive())
		pAttacker = pOwnerPawn;

	// Spawn a real "inferno" entity so the kill feed shows the fire/molotov icon - the icon is
	// resolved from the inflictor's classname (same check this file already does for molotov
	// knockback: V_strncmp(pszInflictorClass, "inferno", 7)). It's used for a single hit and
	// removed again immediately below, before returning control to the engine, so it never gets
	// a chance to Think() and actually spread fire or damage anyone on its own.
	CBaseEntity* pInferno = CreateEntityByName("inferno");
	CBaseEntity* pInflictor = pWorld;

	if (pInferno)
	{
		pInferno->DispatchSpawn();
		pInflictor = pInferno;
	}

	CTakeDamageInfo info(pInflictor, pAttacker, nullptr, 99999.0f, DMG_BURN);
	pVictimPawn->TakeDamage(info);

	if (pInferno)
		pInferno->Remove();
}

void ZMMotherZombiesCommand(CCSPlayerController* player)
{
	if (g_ZRRoundState == EZRRoundState::ROUND_START || g_MotherZombies.size() == 0)
	{
		ClientPrint(player, HUD_PRINTTALK, ZM_PREFIX "There are no mother zombies.");
		return;
	}

	bool first = true;
	std::string names = "";
	for (int i = g_MotherZombies.size() - 1; i >= 0; i--)
	{
		if (!g_MotherZombies[i].IsValid())
		{
			g_MotherZombies.erase(g_MotherZombies.begin() + i);
			continue;
		}

		CCSPlayerController* pMZ = CCSPlayerController::FromSlot(g_MotherZombies[i].GetPlayerSlot());
		if (!pMZ)
			continue;

		if (first)
		{
			names = pMZ->GetPlayerName();
			first = false;
		}
		else
		{
			names += ", " + pMZ->GetPlayerName();
		}
	}

	if (first)
		ClientPrint(player, HUD_PRINTTALK, ZM_PREFIX "There are no mother zombies.");
	else
		ClientPrint(player, HUD_PRINTTALK, ZM_PREFIX "Mother zombies: %s", names.c_str());
}

CON_COMMAND_CHAT(zmmotherzombies, "- Print the current mother zombies to chat")
{
	ZMMotherZombiesCommand(player);
}

CON_COMMAND_CHAT(zmmz, "- Print the current mother zombies to chat")
{
	ZMMotherZombiesCommand(player);
}