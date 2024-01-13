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
#include "entity/services.h"
#include "entity/cteam.h"
#include <sstream>

#include "tier0/memdbgon.h"

extern CGameEntitySystem* g_pEntitySystem;
extern IVEngineServer2* g_pEngineServer2;
extern CGlobalVars* gpGlobals;
extern CCSGameRules* g_pGameRules;
extern IGameEventManager2* g_gameEventManager;
extern float g_flUniversalTime;

void ZR_Infect(CCSPlayerController *pAttackerController, CCSPlayerController *pVictimController, bool bBroadcast);
bool ZR_CheckTeamWinConditions(int iTeamNum);
void ZR_Cure(CCSPlayerController *pTargetController);
void ZR_EndRoundAndAddTeamScore(int iTeamNum);
void SetupCTeams();
bool ZR_IsTeamAlive(int iTeamNum);

EZRRoundState g_ZRRoundState = EZRRoundState::ROUND_START;
static int g_iRoundNum = 0;
static int g_iInfectionCountDown = 0;
static bool g_bRespawnEnabled = true;
static CHandle<Z_CBaseEntity> g_hRespawnToggler;
static CHandle<CTeam> g_hTeamCT;
static CHandle<CTeam> g_hTeamT;

CZRPlayerClassManager* g_pZRPlayerClassManager = nullptr;
ZRWeaponConfig *g_pZRWeaponConfig = nullptr;

// CONVAR_TODO
bool g_bEnableZR = false;
static float g_flMaxZteleDistance = 150.0f;
static bool g_bZteleHuman = false;
static float g_flKnockbackScale = 5.0f;
static int g_iInfectSpawnType = EZRSpawnType::RESPAWN;
static int g_iInfectSpawnTimeMin = 15;
static int g_iInfectSpawnTimeMax = 15;
static int g_iInfectSpawnMZRatio = 7;
static int g_iInfectSpawnMinCount = 1;
static float g_flRespawnDelay = 5.0;
static int g_iDefaultWinnerTeam = CS_TEAM_SPECTATOR;
static bool g_bInfiniteAmmo = true;
static int g_iMZImmunityReduction = 20;

CON_ZR_CVAR(zr_enable, "Whether to enable ZR features", g_bEnableZR, Bool, false)
CON_ZR_CVAR(zr_ztele_max_distance, "Maximum distance players are allowed to move after starting ztele", g_flMaxZteleDistance, Float32, 150.0f)
CON_ZR_CVAR(zr_ztele_allow_humans, "Whether to allow humans to use ztele", g_bZteleHuman, Bool, false)
CON_ZR_CVAR(zr_knockback_scale, "Global knockback scale", g_flKnockbackScale, Float32, 5.0f)
CON_ZR_CVAR(zr_infect_spawn_type, "Type of Mother Zombies Spawn [0 = MZ spawn where they stand, 1 = MZ get teleported back to spawn on being picked]", g_iInfectSpawnType, Int32, EZRSpawnType::RESPAWN)
CON_ZR_CVAR(zr_infect_spawn_time_min, "Minimum time in which Mother Zombies should be picked, after round start", g_iInfectSpawnTimeMin, Int32, 15)
CON_ZR_CVAR(zr_infect_spawn_time_max, "Maximum time in which Mother Zombies should be picked, after round start", g_iInfectSpawnTimeMax, Int32, 15)
CON_ZR_CVAR(zr_infect_spawn_mz_ratio, "Ratio of all Players to Mother Zombies to be spawned at round start", g_iInfectSpawnMZRatio, Int32, 7)
CON_ZR_CVAR(zr_infect_spawn_mz_min_count, "Minimum amount of Mother Zombies to be spawned at round start", g_iInfectSpawnMinCount, Int32, 1)
CON_ZR_CVAR(zr_respawn_delay, "Time before a zombie is respawned", g_flRespawnDelay, Float32, 5.0)
CON_ZR_CVAR(zr_default_winner_team, "Which team wins when time ran out [1 = Draw, 2 = Zombies, 3 = Humans]", g_iDefaultWinnerTeam, Int32, CS_TEAM_SPECTATOR)
CON_ZR_CVAR(zr_infinite_ammo, "Whether to enable infinite reserve ammo on weapons", g_bInfiniteAmmo, Bool, true)
CON_ZR_CVAR(zr_mz_immunity_reduction, "How much mz immunity to reduce for each player per round (0-100)", g_iMZImmunityReduction, Int32, 20)

void ZR_Precache(IEntityResourceManifest* pResourceManifest)
{
	g_pZRPlayerClassManager->PrecacheModels(pResourceManifest);
}

void CZRPlayerClassManager::PrecacheModels(IEntityResourceManifest* pResourceManifest)
{
	FOR_EACH_MAP_FAST(m_ZombieClassMap, i)
	{
		pResourceManifest->AddResource(m_ZombieClassMap[i]->szModelPath.c_str());
	}
	FOR_EACH_MAP_FAST(m_HumanClassMap, i)
	{
		pResourceManifest->AddResource(m_HumanClassMap[i]->szModelPath.c_str());
	}
}

void CZRPlayerClassManager::LoadPlayerClass()
{
	Message("Loading PlayerClass...\n");
	m_ZombieClassMap.Purge();
	m_HumanClassMap.Purge();
	m_vecZombieDefaultClass.Purge();
	m_vecHumanDefaultClass.Purge();

	KeyValues* pKV = new KeyValues("PlayerClass");
	KeyValues::AutoDelete autoDelete(pKV);

	const char *pszPath = "addons/cs2fixes/configs/zr/playerclass.cfg";

	if (!pKV->LoadFromFile(g_pFullFileSystem, pszPath))
	{
		Warning("Failed to load %s\n", pszPath);
		return;
	}

	for (KeyValues* pKey = pKV->GetFirstSubKey(); pKey; pKey = pKey->GetNextKey())
	{
		bool bHuman = !V_strcmp(pKey->GetName(), "Human");
		if (bHuman)
			Message("Human Classes:\n");
		else
			Message("Zombie Classes:\n");
		
		for (KeyValues* pSubKey = pKey->GetFirstSubKey(); pSubKey; pSubKey = pSubKey->GetNextKey())
		{
			bool bEnabled = pSubKey->GetBool("enabled", false);
			bool bTeamDefault = pSubKey->GetBool("team_default", false);

			const char *pszBase = pSubKey->GetString("base", nullptr);
			const char *pszClassName = pSubKey->GetName();

			bool bMissingKey = false;
			// if (!pSubKey->FindKey("enabled"))
			// {
			// 	Warning("%s has unspecified keyvalue: enabled\n", pszClassName);
			// 	bMissingKey = true;
			// }

			// if (!bEnabled)
			// 	continue;
				
			if (!pSubKey->FindKey("team_default"))
			{
				Warning("%s has unspecified keyvalue: team_default\n", pszClassName);
				bMissingKey = true;
			}

			// check everything if no base class
			if (!pszBase)
			{
				if (!pSubKey->FindKey("health"))
				{
					Warning("%s has unspecified keyvalue: health\n", pszClassName);
					bMissingKey = true;
				}
				if (!pSubKey->FindKey("model"))
				{
					Warning("%s has unspecified keyvalue: model\n", pszClassName);
					bMissingKey = true;
				}
				if (!pSubKey->FindKey("skin"))
				{
					Warning("%s has unspecified keyvalue: skin\n", pszClassName);
					bMissingKey = true;
				}
				if (!pSubKey->FindKey("color"))
				{
					Warning("%s has unspecified keyvalue: color\n", pszClassName);
					bMissingKey = true;
				}
				if (!pSubKey->FindKey("scale"))
				{
					Warning("%s has unspecified keyvalue: scale\n", pszClassName);
					bMissingKey = true;
				}
				if (!pSubKey->FindKey("speed"))
				{
					Warning("%s has unspecified keyvalue: speed\n", pszClassName);
					bMissingKey = true;
				}
				if (!pSubKey->FindKey("gravity"))
				{
					Warning("%s has unspecified keyvalue: gravity\n", pszClassName);
					bMissingKey = true;
				}
			}
			if (bMissingKey)
				continue;

			if (bHuman)
			{
				ZRHumanClass *pHumanClass;
				if (pszBase)
				{
					ZRHumanClass *pBaseHumanClass = GetHumanClass(pszBase);
					if (pBaseHumanClass)
					{
						pHumanClass = new ZRHumanClass(pBaseHumanClass);
						pHumanClass->Override(pSubKey);
					}
					else
					{
						Warning("Could not find specified base \"%s\" for %s!!!\n", pszBase, pszClassName);
						continue;
					}
				}
				else
					pHumanClass = new ZRHumanClass(pSubKey);

				m_HumanClassMap.Insert(hash_32_fnv1a_const(pSubKey->GetName()), pHumanClass);

				if (bTeamDefault)
					m_vecHumanDefaultClass.AddToTail(pHumanClass);
				
				pHumanClass->PrintInfo();
			}
			else 
			{
				ZRZombieClass *pZombieClass;
				if (pszBase)
				{
					ZRZombieClass *pBaseZombieClass = GetZombieClass(pszBase);
					if (pBaseZombieClass)
					{
						pZombieClass = new ZRZombieClass(pBaseZombieClass);
						pZombieClass->Override(pSubKey);
					}
					else
					{
						Warning("Could not find specified base \"%s\" for %s!!!\n", pszBase, pszClassName);
						continue;
					}
				}
				else
					pZombieClass = new ZRZombieClass(pSubKey);

				m_ZombieClassMap.Insert(hash_32_fnv1a_const(pSubKey->GetName()), pZombieClass);
				if (pSubKey->GetBool("team_default", false))
					m_vecZombieDefaultClass.AddToTail(pZombieClass);
				
				pZombieClass->PrintInfo();
			}
		}
	}
}

template <typename Out>
void split(const std::string& s, char delim, Out result)
{
	std::istringstream iss(s);
	std::string item;
	while (std::getline(iss, item, delim))
		*result++ = item;
}

void CZRPlayerClassManager::ApplyBaseClass(ZRClass* pClass, CCSPlayerPawn *pPawn)
{
	variant_t strSkin(pClass->iSkin);
	variant_t flScale(pClass->flScale);

	Color clrRender;
	V_StringToColor(pClass->szColor.c_str(), clrRender);

	pPawn->m_iMaxHealth = pClass->iHealth;
	pPawn->m_iHealth = pClass->iHealth;
	pPawn->SetModel(pClass->szModelPath.c_str());
	pPawn->m_clrRender = clrRender;
	pPawn->AcceptInput("Skin", nullptr, nullptr, &strSkin);
	pPawn->m_flVelocityModifier = pClass->flSpeed;
	pPawn->m_flGravityScale = pClass->flGravity;

	// This has to be done a bit later
	UTIL_AddEntityIOEvent(pPawn, "SetScale", nullptr, nullptr, &flScale);
}

ZRHumanClass* CZRPlayerClassManager::GetHumanClass(const char *pszClassName)
{
	uint16 index = m_HumanClassMap.Find(hash_32_fnv1a_const(pszClassName));
	if (!m_HumanClassMap.IsValidIndex(index))
		return nullptr;
	return m_HumanClassMap[index];
}

void CZRPlayerClassManager::ApplyHumanClass(ZRHumanClass *pClass, CCSPlayerPawn *pPawn)
{
	ApplyBaseClass(pClass, pPawn);
	CCSPlayerController *pController = CCSPlayerController::FromPawn(pPawn);
	if (pController)
		CZRRegenTimer::StopRegen(pController);
}

void CZRPlayerClassManager::ApplyDefaultHumanClass(CCSPlayerPawn *pPawn)
{
	if (m_vecHumanDefaultClass.Count() == 0)
	{
		Warning("Missing default human class!!!\n");
		return;
	}
	ApplyHumanClass(m_vecHumanDefaultClass[rand() % m_vecHumanDefaultClass.Count()], pPawn);
}

ZRZombieClass* CZRPlayerClassManager::GetZombieClass(const char *pszClassName)
{
	uint16 index = m_ZombieClassMap.Find(hash_32_fnv1a_const(pszClassName));
	if (!m_ZombieClassMap.IsValidIndex(index))
		return nullptr;
	return m_ZombieClassMap[index];
}

void CZRPlayerClassManager::ApplyZombieClass(ZRZombieClass *pClass, CCSPlayerPawn *pPawn)
{
	ApplyBaseClass(pClass, pPawn);
	CCSPlayerController *pController = CCSPlayerController::FromPawn(pPawn);
	if (pController)
		CZRRegenTimer::StartRegen(pClass->flHealthRegenInterval, pClass->iHealthRegenCount, pController);
}

void CZRPlayerClassManager::ApplyDefaultZombieClass(CCSPlayerPawn *pPawn)
{
	if (m_vecZombieDefaultClass.Count() == 0)
	{
		Warning("Missing default zombie class!!!\n");
		return;
	}
	ApplyZombieClass(m_vecZombieDefaultClass[rand() % m_vecZombieDefaultClass.Count()], pPawn);
}

float CZRRegenTimer::s_flNextExecution;
CZRRegenTimer *CZRRegenTimer::s_vecRegenTimers[MAXPLAYERS];

bool CZRRegenTimer::Execute()
{
	CCSPlayerPawn *pPawn = m_hPawnHandle.Get();
	if (!pPawn || !pPawn->IsAlive())
		return false;

	int iHealth = pPawn->m_iHealth() + m_iRegenAmount;
	pPawn->m_iHealth = pPawn->m_iMaxHealth() < iHealth ? pPawn->m_iMaxHealth() : iHealth;
	return true;
}

void CZRRegenTimer::StartRegen(float flRegenInterval, int iRegenAmount, CCSPlayerController *pController)
{
	int slot = pController->GetPlayerSlot();
	CZRRegenTimer *pTimer = s_vecRegenTimers[slot];
	if (pTimer != nullptr)
	{
		pTimer->m_flInterval = flRegenInterval;
		pTimer->m_iRegenAmount = iRegenAmount;
		return;
	}
	s_vecRegenTimers[slot] = new CZRRegenTimer(flRegenInterval, iRegenAmount, pController->m_hPlayerPawn());
}

void CZRRegenTimer::StopRegen(CCSPlayerController *pController)
{
	int slot = pController->GetPlayerSlot();
	if (!s_vecRegenTimers[slot])
		return;

	delete s_vecRegenTimers[slot];
	s_vecRegenTimers[slot] = nullptr;
}

void CZRRegenTimer::Tick()
{
	// check every timer every 0.1
	if (s_flNextExecution > g_flUniversalTime)
		return;
	s_flNextExecution = g_flUniversalTime + 0.1f;
	for (int i = MAXPLAYERS - 1; i >= 0; i--)
	{
		CZRRegenTimer *pTimer = s_vecRegenTimers[i];
		if (!pTimer)
		{
			continue;
		}
		
		if (pTimer->m_flLastExecute == -1)
			pTimer->m_flLastExecute = g_flUniversalTime;

		// Timer execute 
		if (pTimer->m_flLastExecute + pTimer->m_flInterval <= g_flUniversalTime)
		{
			pTimer->Execute();
			pTimer->m_flLastExecute = g_flUniversalTime;
		}
	}
}

void CZRRegenTimer::RemoveAllTimers()
{
	for (int i = MAXPLAYERS - 1; i >= 0; i--)
	{
		if (!s_vecRegenTimers[i])
		{
			continue;
		}
		delete s_vecRegenTimers[i];
		s_vecRegenTimers[i] = nullptr;
	}
}

void SetupAmmoReplenish()
{
	new CTimer(5.0f, false, []()
	{
		if (!g_bInfiniteAmmo)
			return 5.0f;

		// 999 will be automatically clamped to the weapons m_nPrimaryReserveAmmoMax
		variant_t value("999");
		Z_CBaseEntity* pTarget = nullptr;

		while (pTarget = UTIL_FindEntityByClassname(pTarget, "weapon_*"))
			pTarget->AcceptInput("SetReserveAmmoAmount", nullptr, nullptr, &value);

		return 5.0f;
	});
}

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

	g_pZRPlayerClassManager->LoadPlayerClass();
	g_pZRWeaponConfig->LoadWeaponConfig();
	SetupCTeams();
	SetupAmmoReplenish();
}

void ZRWeaponConfig::LoadWeaponConfig()
{
	m_WeaponMap.Purge();
	KeyValues* pKV = new KeyValues("Weapons");
	KeyValues::AutoDelete autoDelete(pKV);

	const char *pszPath = "addons/cs2fixes/configs/zr/weapons.cfg";

	if (!pKV->LoadFromFile(g_pFullFileSystem, pszPath))
	{
		Warning("Failed to load %s\n", pszPath);
		return;
	}
	for (KeyValues* pKey = pKV->GetFirstSubKey(); pKey; pKey = pKey->GetNextKey())
	{
		const char *pszWeaponName = pKey->GetName();
		bool bEnabled = pKey->GetBool("enabled", false);
		float flKnockback= pKey->GetFloat("knockback", 0.0f);
		Message("%s knockback: %f\n", pszWeaponName, flKnockback);
		ZRWeapon *weapon = new ZRWeapon;
		if (!bEnabled)
			continue;

		weapon->flKnockback = flKnockback;

		m_WeaponMap.Insert(hash_32_fnv1a_const(pszWeaponName), weapon);
	}

	return;
}

ZRWeapon* ZRWeaponConfig::FindWeapon(const char *pszWeaponName)
{
	uint16 index = m_WeaponMap.Find(hash_32_fnv1a_const(pszWeaponName));
	if (m_WeaponMap.IsValidIndex(index))
		return m_WeaponMap[index];

	return nullptr;
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
		ZR_CheckTeamWinConditions(CS_TEAM_CT);
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
	CZRRegenTimer::RemoveAllTimers();
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

void ZR_ApplyKnockback(CCSPlayerPawn *pHuman, CCSPlayerPawn *pVictim, int iDamage, const char *szWeapon)
{
	ZRWeapon *pWeapon = g_pZRWeaponConfig->FindWeapon(szWeapon);
	// player shouldn't be able to pick up that weapon in the first place, but just in case
	if (!pWeapon)
		return;
	float flWeaponKnockbackScale = pWeapon->flKnockback;
	
	Vector vecKnockback;
	AngleVectors(pHuman->m_angEyeAngles(), &vecKnockback);
	vecKnockback *= (iDamage * g_flKnockbackScale * flWeaponKnockbackScale);
	pVictim->m_vecAbsVelocity = pVictim->m_vecAbsVelocity() + vecKnockback;
}

void ZR_ApplyKnockbackExplosion(Z_CBaseEntity *pProjectile, CCSPlayerPawn *pVictim, int iDamage)
{
	ZRWeapon *pWeapon = g_pZRWeaponConfig->FindWeapon(pProjectile->GetClassname());
	if (!pWeapon)
		return;
	float flWeaponKnockbackScale = pWeapon->flKnockback;

	Vector vecDisplacement = pVictim->GetAbsOrigin() - pProjectile->GetAbsOrigin();
	vecDisplacement.z += 36;
	VectorNormalize(vecDisplacement);
	Vector vecKnockback = vecDisplacement;
	vecKnockback *= (iDamage * g_flKnockbackScale * flWeaponKnockbackScale);
	pVictim->m_vecAbsVelocity = pVictim->m_vecAbsVelocity() + vecKnockback;
}

void ZR_FakePlayerDeath(CCSPlayerController *pAttackerController, CCSPlayerController *pVictimController, const char *szWeapon)
{
	IGameEvent *pEvent = g_gameEventManager->CreateEvent("player_death");

	if (!pEvent)
		return;

	pEvent->SetPlayer("userid", pVictimController->GetPlayerSlot());
	pEvent->SetPlayer("attacker", pAttackerController->GetPlayerSlot());
	pEvent->SetInt("assister", 65535);
	pEvent->SetInt("assister_pawn", -1);
	pEvent->SetString("weapon", szWeapon);
	pEvent->SetBool("infected", true);

	g_gameEventManager->FireEvent(pEvent, false);
}

void ZR_StripAndGiveKnife(CCSPlayerPawn *pPawn)
{
	CCSPlayer_ItemServices *pItemServices = pPawn->m_pItemServices();
	CPlayer_WeaponServices* pWeaponServices = pPawn->m_pWeaponServices();

	// it can sometimes be null when player joined on the very first round? 
	if (!pItemServices || !pWeaponServices)
		return;

	CUtlVector<CHandle<CBasePlayerWeapon>>* weapons = pWeaponServices->m_hMyWeapons();

	FOR_EACH_VEC(*weapons, i)
	{
		CHandle<CBasePlayerWeapon>& weaponHandle = (*weapons)[i];
		CBasePlayerWeapon* pWeapon = weaponHandle.Get();

		if (!pWeapon)
			continue;

		// If this is a map-spawned weapon (items), we should drop it instead
		if (V_strcmp(pWeapon->m_sUniqueHammerID().Get(), ""))
		{
			// Despite having to pass a weapon into DropPlayerWeapon, it only drops the weapon if it's also the players active weapon
			pWeaponServices->m_hActiveWeapon = weaponHandle;
			pItemServices->DropPlayerWeapon(pWeapon);
		}
	}

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

	g_pZRPlayerClassManager->ApplyDefaultHumanClass(pTargetPawn);
}

void ZR_Infect(CCSPlayerController *pAttackerController, CCSPlayerController *pVictimController, bool bDontBroadcast)
{
	if (pVictimController->m_iTeamNum() == CS_TEAM_CT)
		pVictimController->SwitchTeam(CS_TEAM_T);

	ZR_CheckTeamWinConditions(CS_TEAM_T);

	if (!bDontBroadcast)
		ZR_FakePlayerDeath(pAttackerController, pVictimController, "knife"); // or any other killicon

	CCSPlayerPawn *pVictimPawn = (CCSPlayerPawn*)pVictimController->GetPawn();
	if (!pVictimPawn)
		return;

	ZR_StripAndGiveKnife(pVictimPawn);
	
	g_pZRPlayerClassManager->ApplyDefaultZombieClass(pVictimPawn);
}

void ZR_InfectMotherZombie(CCSPlayerController *pVictimController)
{
	pVictimController->SwitchTeam(CS_TEAM_T);

	CCSPlayerPawn *pVictimPawn = (CCSPlayerPawn*)pVictimController->GetPawn();
	if (!pVictimPawn)
		return;

	ZR_StripAndGiveKnife(pVictimPawn);
	ZRZombieClass *pClass = g_pZRPlayerClassManager->GetZombieClass("MotherZombie");
	if (pClass)
		g_pZRPlayerClassManager->ApplyZombieClass(pClass, pVictimPawn);
	else
	{
		//Warning("Missing mother zombie class!!!\n");
		g_pZRPlayerClassManager->ApplyDefaultZombieClass(pVictimPawn);
	}
}

// make players who've been picked as MZ recently less likely to be picked again
// store a variable in ZEPlayer, which gets initialized with value 100 if they are picked to be a mother zombie
// the value represents a % chance of the player being skipped next time they are picked to be a mother zombie
// If the player is skipped, next random player is picked to be mother zombie (and same skip chance logic applies to him)
// the variable gets decreased by 20 every round
void ZR_InitialInfection()
{
	// mz infection candidates
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

	if (g_iInfectSpawnMZRatio <= 0)
	{
		Warning("Invalid Mother Zombie Ratio!!!");
		return;
	}

	// the num of mz to infect
	int iMZToInfect = pCandidateControllers.Count() / g_iInfectSpawnMZRatio;
	iMZToInfect = g_iInfectSpawnMinCount > iMZToInfect ? g_iInfectSpawnMinCount : iMZToInfect;
	bool vecIsMZ[MAXPLAYERS] = { false };

	// get spawn points
	CUtlVector<SpawnPoint*> spawns;
	if (g_iInfectSpawnType == EZRSpawnType::RESPAWN)
	{
		spawns.AddVectorToTail(*g_pGameRules->m_CTSpawnPoints());
		spawns.AddVectorToTail(*g_pGameRules->m_TerroristSpawnPoints());

		if (!spawns.Count())
		{
			ClientPrintAll(HUD_PRINTTALK, ZR_PREFIX"There are no spawns!");
			Panic("There are no spawns!\n");
			return;
		}
	}

	// infect
	int iFailSafeCounter = 0;
	while (iMZToInfect > 0)
	{
		//If we somehow don't have enough mother zombies after going through the players 5 times,
		if (iFailSafeCounter >= 5)
		{
			FOR_EACH_VEC(pCandidateControllers, i)
			{
				// at 5, reset everyone's immunity but mother zombies from this and last round
				// at 6, reset everyone's immunity but mother zombies from this round
				ZEPlayer* pPlayer = pCandidateControllers[i]->GetZEPlayer();
				if (pPlayer->GetImmunity() < 100 || (iFailSafeCounter >= 6 && !vecIsMZ[i]))
					pPlayer->SetImmunity(0);
			}
		}

		// a list of player who survived the previous mz roll of this round
		CUtlVector<CCSPlayerController*> pSurvivorControllers;
		FOR_EACH_VEC(pCandidateControllers, i)
		{
			// don't even bother with picked mz or player with 100 immunity
			ZEPlayer* pPlayer = pCandidateControllers[i]->GetZEPlayer();
			if (pPlayer && pPlayer->GetImmunity() < 100)
				pSurvivorControllers.AddToTail(pCandidateControllers[i]);
		}

		// no enough human even after triggering fail safe
		if (iFailSafeCounter >= 6 && pSurvivorControllers.Count() == 0)
			break;

		while (pSurvivorControllers.Count() > 0 && iMZToInfect > 0)
		{
			int randomindex = rand() % pSurvivorControllers.Count();

			CCSPlayerController* pController = (CCSPlayerController*)pSurvivorControllers[randomindex];
			CCSPlayerPawn* pPawn = (CCSPlayerPawn*)pController->GetPawn();
			ZEPlayer* pPlayer = pSurvivorControllers[randomindex]->GetZEPlayer();
			//roll for immunity
			if (rand() % 100 < pPlayer->GetImmunity())
			{
				pSurvivorControllers.FastRemove(randomindex);
				continue;
			}

			// pick random spawn point
			if (g_iInfectSpawnType == EZRSpawnType::RESPAWN)
			{
				randomindex = rand() % spawns.Count();
				pPawn->SetAbsOrigin(spawns[randomindex]->GetAbsOrigin());
				pPawn->SetAbsRotation(spawns[randomindex]->GetAbsRotation());
			}

			ZR_InfectMotherZombie(pController);
			pPlayer->SetImmunity(100);
			vecIsMZ[pPlayer->GetPlayerSlot().Get()] = true;

			iMZToInfect--;
		}
		iFailSafeCounter++;
	}

	// reduce everyone's immunity except mz
	for (int i = 0; i < gpGlobals->maxClients; i++)
	{
		ZEPlayer* pPlayer = g_playerManager->GetPlayer(i);
		if (!pPlayer || vecIsMZ[i])
			continue;
		
		pPlayer->SetImmunity(pPlayer->GetImmunity() - g_iMZImmunityReduction);
	}
	ClientPrintAll(HUD_PRINTCENTER, "First infection has started!");
	ClientPrintAll(HUD_PRINTTALK, ZR_PREFIX "First infection has started! Good luck, survivors!");
	g_ZRRoundState = EZRRoundState::POST_INFECTION;
}

void ZR_StartInitialCountdown()
{
	int iRoundNum = g_iRoundNum;
	g_iInfectionCountDown = g_iInfectSpawnTimeMin + (rand() % (g_iInfectSpawnTimeMax - g_iInfectSpawnTimeMin + 1));
	new CTimer(0.0f, false, [iRoundNum]()
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
			char message[256];
			V_snprintf(message, sizeof(message), "First infection in \7%i %s\1!", g_iInfectionCountDown, g_iInfectionCountDown == 1 ? "second" : "seconds");

			ClientPrintAll(HUD_PRINTCENTER, message);
			if (g_iInfectionCountDown % 5 == 0)
				ClientPrintAll(HUD_PRINTTALK, "%s%s", ZR_PREFIX, message);
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
		// inflictor class from grenade damage is actually hegrenade_projectile
		if (!V_strncmp(pszInflictorClass, "hegrenade", 9) || !V_strncmp(pszInflictorClass, "inferno", 7))
			ZR_ApplyKnockbackExplosion((Z_CBaseEntity*)pInflictor, (CCSPlayerPawn*)pVictimPawn, (int)pInfo->m_flDamage);
	}
	return false;
}

// return false to prevent player from picking it up
bool ZR_Detour_CCSPlayer_WeaponServices_CanUse(CCSPlayer_WeaponServices *pWeaponServices, CBasePlayerWeapon* pPlayerWeapon)
{
	CCSPlayerPawn *pPawn = pWeaponServices->__m_pChainEntity();
	if (!pPawn)
		return false;
	const char *pszWeaponClassname = pPlayerWeapon->GetClassname();
	if (pPawn->m_iTeamNum() == CS_TEAM_T && V_strncmp(pszWeaponClassname, "weapon_knife", 12))
		return false;
	if (pPawn->m_iTeamNum() == CS_TEAM_CT && V_strlen(pszWeaponClassname) > 7 && !g_pZRWeaponConfig->FindWeapon(pszWeaponClassname + 7))
		return false;
	// doesn't guarantee the player will pick the weapon up, it just allows the original function to run
	return true;
}

void ZR_Detour_CEntityIdentity_AcceptInput(CEntityIdentity* pThis, CUtlSymbolLarge* pInputName, CEntityInstance* pActivator, CEntityInstance* pCaller, variant_t* value, int nOutputID)
{
	if (!g_hRespawnToggler.IsValid())
		return;

	Z_CBaseEntity* relay = g_hRespawnToggler.Get();
	const char* inputName = pInputName->String();

	// Must be an input into our zr_toggle_respawn relay
	if (!relay || pThis != relay->m_pEntity)
		return;

	if (!V_strcasecmp(inputName, "Trigger"))
		ToggleRespawn();
	else if (!V_strcasecmp(inputName, "Enable") && !g_bRespawnEnabled)
		ToggleRespawn(true, true);
	else if (!V_strcasecmp(inputName, "Disable") && g_bRespawnEnabled)
		ToggleRespawn(true, false);
	else
		return;

	ClientPrintAll(HUD_PRINTTALK, ZR_PREFIX "Respawning is %s!", g_bRespawnEnabled ? "enabled" : "disabled");
}

void SpawnPlayer(CCSPlayerController* pController)
{
	pController->ChangeTeam(g_ZRRoundState == EZRRoundState::POST_INFECTION ? CS_TEAM_T : CS_TEAM_CT);

	// Make sure the round ends if spawning into an empty server
	if (!ZR_IsTeamAlive(CS_TEAM_CT) && !ZR_IsTeamAlive(CS_TEAM_T) && g_ZRRoundState != EZRRoundState::ROUND_END)
	{
		g_pGameRules->TerminateRound(1.0f, CSRoundEndReason::GameStart);
		g_ZRRoundState = EZRRoundState::ROUND_END;
		return;
	}

	CHandle<CCSPlayerController> handle = pController->GetHandle();
	int iRoundNum = g_iRoundNum;
	new CTimer(2.0f, false, [iRoundNum, handle]()
	{
		CCSPlayerController* pController = (CCSPlayerController*)handle.Get();
		if (iRoundNum != g_iRoundNum || !pController || !g_bRespawnEnabled || pController->m_iTeamNum < CS_TEAM_T)
			return -1.0f;
		pController->Respawn();
		return -1.0f;
	});
}

void ZR_Hook_ClientPutInServer(CPlayerSlot slot, char const *pszName, int type, uint64 xuid)
{
	CCSPlayerController* pController = CCSPlayerController::FromSlot(slot);
	if (!pController)
		return;

	SpawnPlayer(pController);
}

void ZR_Hook_ClientCommand_JoinTeam(CPlayerSlot slot, const CCommand &args)
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
		SpawnPlayer(pController);
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
	// fake player_death, don't need to respawn or check win condition
	if (pEvent->GetBool("infected"))
		return;

	CCSPlayerController *pVictimController = (CCSPlayerController*)pEvent->GetPlayerController("userid");
	if (!pVictimController)
		return;
	CCSPlayerPawn *pVictimPawn = (CCSPlayerPawn*)pVictimController->GetPawn();
	if (!pVictimPawn)
		return;

	ZR_CheckTeamWinConditions(pVictimPawn->m_iTeamNum() == CS_TEAM_T ? CS_TEAM_CT : CS_TEAM_T);

	// respawn player
	CHandle<CCSPlayerController> handle = pVictimController->GetHandle();
	int iRoundNum = g_iRoundNum;
	new CTimer(g_flRespawnDelay, false, [iRoundNum, handle]()
	{
		CCSPlayerController* pController = (CCSPlayerController*)handle.Get();
		if (iRoundNum != g_iRoundNum || !pController || !g_bRespawnEnabled || pController->m_iTeamNum < CS_TEAM_T)
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
bool ZR_IsTeamAlive(int iTeamNum)
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
bool ZR_CheckTeamWinConditions(int iTeamNum)
{
	if (g_ZRRoundState == EZRRoundState::ROUND_END || (iTeamNum == CS_TEAM_CT && g_bRespawnEnabled) || (iTeamNum != CS_TEAM_T && iTeamNum != CS_TEAM_CT))
		return false;

	// check the opposite team
	if (ZR_IsTeamAlive(iTeamNum == CS_TEAM_CT ? CS_TEAM_T : CS_TEAM_CT))
		return false;

	// allow the team to win
	ZR_EndRoundAndAddTeamScore(iTeamNum);

	return true;
}

// spectator: draw
// t: t win, add t score
// ct: ct win, add ct score
void ZR_EndRoundAndAddTeamScore(int iTeamNum)
{
	bool bServerIdle = true;

	for (int i = 0; i < gpGlobals->maxClients; i++)
	{
		ZEPlayer* pPlayer = g_playerManager->GetPlayer(i);

		if (!pPlayer || !pPlayer->IsConnected() || !pPlayer->IsInGame() || pPlayer->IsFakeClient())
			continue;

		bServerIdle = false;
		break;
	}

	// Don't end rounds while the server is idling
	if (bServerIdle)
		return;

	CSRoundEndReason iReason;
	switch (iTeamNum)
	{
		default:
		case CS_TEAM_SPECTATOR:
			iReason = CSRoundEndReason::Draw;
			break;
		case CS_TEAM_T:
			iReason = CSRoundEndReason::TerroristWin;
			break;
		case CS_TEAM_CT:
			iReason = CSRoundEndReason::CTWin;
			break;
	}

	// CONVAR_TODO
	ConVar* cvar = g_pCVar->GetConVar(g_pCVar->FindConVar("mp_round_restart_delay"));
	// HACK: values is actually the cvar value itself, hence this ugly cast.
	float flRestartDelay = *(float*)&cvar->values;

	g_pGameRules->TerminateRound(flRestartDelay, iReason);
	g_ZRRoundState = EZRRoundState::ROUND_END;
	ToggleRespawn(true, false);

	if (iTeamNum == CS_TEAM_CT)
	{
		if (!g_hTeamCT.Get())
		{
			Panic("Cannot find CTeam for CT!\n");
			return;
		}
		g_hTeamCT->m_iScore = g_hTeamCT->m_iScore() + 1;
	}
	else if (iTeamNum == CS_TEAM_T)
	{	
		if (!g_hTeamT.Get())
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

	CUtlVector<SpawnPoint*> spawns;
	spawns.AddVectorToTail(*g_pGameRules->m_CTSpawnPoints());
	spawns.AddVectorToTail(*g_pGameRules->m_TerroristSpawnPoints());

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