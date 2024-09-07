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

#include "commands.h"
#include "utils/entity.h"
#include "playermanager.h"
#include "ctimer.h"
#include "eventlistener.h"
#include "zombiereborn.h"
#include "entity/cgamerules.h"
#include "entity/services.h"
#include "entity/cteam.h"
#include "entity/cparticlesystem.h"
#include "engine/igameeventsystem.h"
#include "networksystem/inetworkmessages.h"
#include "recipientfilters.h"
#include "serversideclient.h"
#include "user_preferences.h"
#include "customio.h"
#include <sstream>
#include "leader.h"
#include "tier0/vprof.h"
#include <fstream>
#include "vendor/nlohmann/json.hpp"

#include "tier0/memdbgon.h"

using ordered_json = nlohmann::ordered_json;

extern CGameEntitySystem* g_pEntitySystem;
extern IVEngineServer2* g_pEngineServer2;
extern CGlobalVars* gpGlobals;
extern CCSGameRules* g_pGameRules;
extern IGameEventManager2* g_gameEventManager;
extern IGameEventSystem *g_gameEventSystem;
extern double g_flUniversalTime;

void ZR_Infect(CCSPlayerController *pAttackerController, CCSPlayerController *pVictimController, bool bBroadcast);
bool ZR_CheckTeamWinConditions(int iTeamNum);
void ZR_Cure(CCSPlayerController *pTargetController);
void ZR_EndRoundAndAddTeamScore(int iTeamNum);
void SetupCTeams();
bool ZR_IsTeamAlive(int iTeamNum);

EZRRoundState g_ZRRoundState = EZRRoundState::ROUND_START;
static int g_iInfectionCountDown = 0;
static bool g_bRespawnEnabled = true;
static CHandle<CBaseEntity> g_hRespawnToggler;
static CHandle<CTeam> g_hTeamCT;
static CHandle<CTeam> g_hTeamT;

CZRPlayerClassManager* g_pZRPlayerClassManager = nullptr;
ZRWeaponConfig *g_pZRWeaponConfig = nullptr;

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
static int g_iMZImmunityReduction = 20;
static int g_iGroanChance = 5;
static float g_flMoanInterval = 30.f;
static bool g_bNapalmGrenades = true;
static float g_flNapalmDuration = 5.f;
static float g_flNapalmFullDamage = 50.f;

static std::string g_szHumanWinOverlayParticle;
static std::string g_szHumanWinOverlayMaterial;
static float g_flHumanWinOverlaySize;

static std::string g_szZombieWinOverlayParticle;
static std::string g_szZombieWinOverlayMaterial;
static float g_flZombieWinOverlaySize;

static bool g_bInfectShake = true;
static float g_flInfectShakeAmplitude = 15.f;
static float g_flInfectShakeFrequency = 2.f;
static float g_flInfectShakeDuration = 5.f;

FAKE_BOOL_CVAR(zr_enable, "Whether to enable ZR features", g_bEnableZR, false, false)
FAKE_FLOAT_CVAR(zr_ztele_max_distance, "Maximum distance players are allowed to move after starting ztele", g_flMaxZteleDistance, 150.0f, false)
FAKE_BOOL_CVAR(zr_ztele_allow_humans, "Whether to allow humans to use ztele", g_bZteleHuman, false, false)
FAKE_FLOAT_CVAR(zr_knockback_scale, "Global knockback scale", g_flKnockbackScale, 5.0f, false)
FAKE_INT_CVAR(zr_infect_spawn_type, "Type of Mother Zombies Spawn [0 = MZ spawn where they stand, 1 = MZ get teleported back to spawn on being picked]", g_iInfectSpawnType, EZRSpawnType::RESPAWN, false)
FAKE_INT_CVAR(zr_infect_spawn_time_min, "Minimum time in which Mother Zombies should be picked, after round start", g_iInfectSpawnTimeMin, 15, false)
FAKE_INT_CVAR(zr_infect_spawn_time_max, "Maximum time in which Mother Zombies should be picked, after round start", g_iInfectSpawnTimeMax, 15, false)
FAKE_INT_CVAR(zr_infect_spawn_mz_ratio, "Ratio of all Players to Mother Zombies to be spawned at round start", g_iInfectSpawnMZRatio, 7, false)
FAKE_INT_CVAR(zr_infect_spawn_mz_min_count, "Minimum amount of Mother Zombies to be spawned at round start", g_iInfectSpawnMinCount, 1, false)
FAKE_FLOAT_CVAR(zr_respawn_delay, "Time before a zombie is automatically respawned, negative values (e.g. -1.0) disable this, note maps can still manually respawn at any time", g_flRespawnDelay, 5.0f, false)
FAKE_INT_CVAR(zr_default_winner_team, "Which team wins when time ran out [1 = Draw, 2 = Zombies, 3 = Humans]", g_iDefaultWinnerTeam, CS_TEAM_SPECTATOR, false)
FAKE_INT_CVAR(zr_mz_immunity_reduction, "How much mz immunity to reduce for each player per round (0-100)", g_iMZImmunityReduction, 20, false)
FAKE_INT_CVAR(zr_sounds_groan_chance, "How likely should a zombie groan whenever they take damage (1 / N)", g_iGroanChance, 5, false)
FAKE_FLOAT_CVAR(zr_sounds_moan_interval, "How often in seconds should zombies moan", g_flMoanInterval, 5.f, false)
FAKE_BOOL_CVAR(zr_napalm_enable, "Whether to use napalm grenades", g_bNapalmGrenades, true, false)
FAKE_FLOAT_CVAR(zr_napalm_burn_duration, "How long in seconds should zombies burn from napalm grenades", g_flNapalmDuration, 5.f, false)
FAKE_FLOAT_CVAR(zr_napalm_full_damage, "The amount of damage needed to apply full burn duration for napalm grenades (max grenade damage is 99)", g_flNapalmFullDamage, 50.f, false)
FAKE_STRING_CVAR(zr_human_win_overlay_particle, "Screenspace particle to display when human win", g_szHumanWinOverlayParticle, false)
FAKE_STRING_CVAR(zr_human_win_overlay_material, "Material override for human's win overlay particle", g_szHumanWinOverlayMaterial, false)
FAKE_FLOAT_CVAR(zr_human_win_overlay_size, "Size of human's win overlay particle", g_flHumanWinOverlaySize, 5.0f, false)
FAKE_STRING_CVAR(zr_zombie_win_overlay_particle, "Screenspace particle to display when zombie win", g_szZombieWinOverlayParticle, false)
FAKE_STRING_CVAR(zr_zombie_win_overlay_material, "Material override for zombie's win overlay particle", g_szZombieWinOverlayMaterial, false)
FAKE_FLOAT_CVAR(zr_zombie_win_overlay_size, "Size of zombie's win overlay particle", g_flZombieWinOverlaySize, 5.0f, false)
FAKE_BOOL_CVAR(zr_infect_shake, "Whether to shake a player's view on infect", g_bInfectShake, true, false);
FAKE_FLOAT_CVAR(zr_infect_shake_amp, "Amplitude of shaking effect", g_flInfectShakeAmplitude, 15.f, false);
FAKE_FLOAT_CVAR(zr_infect_shake_frequency, "Frequency of shaking effect", g_flInfectShakeFrequency, 2.f, false);
FAKE_FLOAT_CVAR(zr_infect_shake_duration, "Duration of shaking effect", g_flInfectShakeDuration, 5.f, false);

// meant only for offline config validation and can easily cause issues when used on live server
#ifdef _DEBUG
CON_COMMAND_F(zr_reload_classes, "Reload ZR player classes", FCVAR_SPONLY | FCVAR_LINKED_CONCOMMAND)
{
	g_pZRPlayerClassManager->LoadPlayerClass();
	
	Message("Reloaded ZR player classes.\n");
}
#endif

void ZR_Precache(IEntityResourceManifest* pResourceManifest)
{
	g_pZRPlayerClassManager->LoadPlayerClass();
	g_pZRPlayerClassManager->PrecacheModels(pResourceManifest);

	pResourceManifest->AddResource(g_szHumanWinOverlayParticle.c_str());
	pResourceManifest->AddResource(g_szZombieWinOverlayParticle.c_str());
	
	pResourceManifest->AddResource(g_szHumanWinOverlayMaterial.c_str());
	pResourceManifest->AddResource(g_szZombieWinOverlayMaterial.c_str());

	pResourceManifest->AddResource("soundevents/soundevents_zr.vsndevts");
}

void ZR_CreateOverlay(const char* pszOverlayParticlePath, float flAlpha, float flRadius, float flLifeTime, Color clrTint, const char* pszMaterialOverride)
{
	CEnvParticleGlow* particle = CreateEntityByName<CEnvParticleGlow>("env_particle_glow");

	CEntityKeyValues* pKeyValues = new CEntityKeyValues();

	pKeyValues->SetString("effect_name", pszOverlayParticlePath);
	// these properties are mapped to control point position by the entity
	pKeyValues->SetFloat("alphascale", flAlpha);		//17.x
	pKeyValues->SetFloat("scale", flRadius);			//17.y
	pKeyValues->SetFloat("selfillumscale", flLifeTime); //17.z
	pKeyValues->SetColor("colortint", clrTint);			//16.xyz

	pKeyValues->SetString("effect_textureOverride", pszMaterialOverride);

	particle->DispatchSpawn(pKeyValues);
	particle->AcceptInput("Start");

	UTIL_AddEntityIOEvent(particle, "Kill", nullptr, nullptr, "", flLifeTime + 1.0);
}

ZRModelEntry::ZRModelEntry(ZRModelEntry *modelEntry) :
	szModelPath(modelEntry->szModelPath),
	szColor(modelEntry->szColor)
	{
		vecSkins.Purge();
		FOR_EACH_VEC(modelEntry->vecSkins, i)
		{
			vecSkins.AddToTail(modelEntry->vecSkins[i]);
		}
	};

ZRModelEntry::ZRModelEntry(ordered_json jsonModelEntry) :
	szModelPath(jsonModelEntry.value("modelname", "")),
	szColor(jsonModelEntry.value("color", "255 255 255"))
	{
		vecSkins.Purge();

		if (jsonModelEntry.contains("skins"))
		{
			if (jsonModelEntry["skins"].size() > 0) // single int or array of ints
			{
				for (auto& [key, skinIndex] : jsonModelEntry["skins"].items())
					vecSkins.AddToTail(skinIndex);
			}
			return;
		}
		vecSkins.AddToTail(0); // key missing, set default
	};

// seperate parsing to adminsystem's ParseFlags as making class 'z' flagged would make it available to players with non-zero flag
uint64 ZRClass::ParseClassFlags(const char* pszFlags)
{
	uint64 flags = 0;
	size_t length = V_strlen(pszFlags);

	for (size_t i = 0; i < length; i++)
	{
		char c = tolower(pszFlags[i]);
		if (c < 'a' || c > 'z')
			continue;

		flags |= ((uint64)1 << (c - 'a'));
	}

	return flags;
}

// this constructor is only used to create base class, which is required to have all values and min. 1 valid model entry
ZRClass::ZRClass(ordered_json jsonKeys, std::string szClassname) :
	bEnabled(jsonKeys["enabled"].get<bool>()),
	szClassName(szClassname),
	iHealth(jsonKeys["health"].get<int>()),
	flScale(jsonKeys["scale"].get<float>()),
	flSpeed(jsonKeys["speed"].get<float>()),
	flGravity(jsonKeys["gravity"].get<float>()),
	iAdminFlag(ParseClassFlags(
		jsonKeys["admin_flag"].get<std::string>().c_str()
	))
	{
		vecModels.Purge();

		for (auto& [key, jsonModelEntry] : jsonKeys["models"].items())
		{
			ZRModelEntry *modelEntry = new ZRModelEntry(jsonModelEntry);
			vecModels.AddToTail(modelEntry);
		}
	};

void ZRClass::Override(ordered_json jsonKeys, std::string szClassname)
{
	szClassName = szClassname;
	if (jsonKeys.contains("enabled"))
		bEnabled = jsonKeys["enabled"].get<bool>();
	if (jsonKeys.contains("health"))
		iHealth = jsonKeys["health"].get<int>();
	if (jsonKeys.contains("scale"))
		flScale = jsonKeys["scale"].get<float>();
	if (jsonKeys.contains("speed"))
		flSpeed = jsonKeys["speed"].get<float>();
	if (jsonKeys.contains("gravity"))
		flGravity = jsonKeys["gravity"].get<float>();
	if (jsonKeys.contains("admin_flag"))
		iAdminFlag = ParseClassFlags(
			jsonKeys["admin_flag"].get<std::string>().c_str()
		);

	// no models entry key or it's empty, use model entries of base class
	if (!jsonKeys.contains("models") || jsonKeys["models"].empty())
		return;
	
	// one model entry in base and overriding class, apply model entry keys if defined
	if (vecModels.Count() == 1 && jsonKeys["models"].size() == 1)
	{
		if (jsonKeys["models"][0].contains("modelname"))
			vecModels[0]->szModelPath = jsonKeys["models"][0]["modelname"];
		if (jsonKeys["models"][0].contains("color"))
			vecModels[0]->szColor = jsonKeys["models"][0]["color"];
		if (jsonKeys["models"][0].contains("skins") && jsonKeys["models"][0]["skins"].size() > 0)
		{
			vecModels[0]->vecSkins.Purge();

			for (auto& [key, skinIndex] : jsonKeys["models"][0]["skins"].items())
				vecModels[0]->vecSkins.AddToTail(skinIndex);
		}

		return;
	}
	
	// more than one model entry in either base or child class, either override all entries or none
	for (int i = jsonKeys["models"].size()-1; i >= 0; i--)
	{
		if (jsonKeys["models"][i].size() < 3)
		{
			Warning("Model entry in child class %s has empty key(s), skipping\n", szClassname.c_str());
			jsonKeys["models"].erase(i);
		}
	}
	
	if (jsonKeys["models"].empty())
	{
		Warning("No valid model entries remaining in child class %s, using model entries of base class %s\n",
				szClassname.c_str(), jsonKeys["base"].get<std::string>().c_str());
		return;
	}

	vecModels.Purge();

	for (auto& [key, jsonModelEntry] : jsonKeys["models"].items())
	{
		ZRModelEntry *modelEntry = new ZRModelEntry(jsonModelEntry);
		vecModels.AddToTail(modelEntry);
	}
}

ZRHumanClass::ZRHumanClass(ordered_json jsonKeys, std::string szClassname) : ZRClass(jsonKeys, szClassname){};

ZRZombieClass::ZRZombieClass(ordered_json jsonKeys, std::string szClassname) :
	ZRClass(jsonKeys, szClassname),
	iHealthRegenCount(jsonKeys.value("health_regen_count", 0)),
	flHealthRegenInterval(jsonKeys.value("health_regen_interval", 0)){};

void ZRZombieClass::Override(ordered_json jsonKeys, std::string szClassname)
{
	ZRClass::Override(jsonKeys, szClassname);
	if (jsonKeys.contains("health_regen_count"))
		iHealthRegenCount = jsonKeys["health_regen_count"].get<int>();
	if (jsonKeys.contains("health_regen_interval"))
		flHealthRegenInterval = jsonKeys["health_regen_interval"].get<float>();
}

bool ZRClass::IsApplicableTo(CCSPlayerController *pController)
{
	if (!bEnabled) return false;
	if (!V_stricmp(szClassName.c_str(), "MotherZombie")) return false;
	ZEPlayer* pPlayer = pController->GetZEPlayer();
	if (!pPlayer) return false;
	if (!pPlayer->IsAdminFlagSet(iAdminFlag)) return false;
	return true;
}

void CZRPlayerClassManager::PrecacheModels(IEntityResourceManifest* pResourceManifest)
{
	FOR_EACH_MAP_FAST(m_ZombieClassMap, i)
	{
		FOR_EACH_VEC(m_ZombieClassMap[i]->vecModels, j)
		{
			pResourceManifest->AddResource(m_ZombieClassMap[i]->vecModels[j]->szModelPath.c_str());
		}
	}
	FOR_EACH_MAP_FAST(m_HumanClassMap, i)
	{
		FOR_EACH_VEC(m_HumanClassMap[i]->vecModels, j)
		{
			pResourceManifest->AddResource(m_HumanClassMap[i]->vecModels[j]->szModelPath.c_str());
		}
	}
}

bool CZRPlayerClassManager::CreateJsonConfigFromKeyValuesFile()
{
	Message("Attempting to convert KeyValues1 config format to JSON format...\n");

	const char *pszPath = "addons/cs2fixes/configs/zr/playerclass.cfg";

	KeyValues* pKV = new KeyValues("PlayerClass");
	KeyValues::AutoDelete autoDelete(pKV);

	if (!pKV->LoadFromFile(g_pFullFileSystem, pszPath))
	{
		Warning("Failed to load %s when trying to convert playerclass.cfg to JSON format\n", pszPath);
		return false;
	}

	ordered_json jsonPlayerClasses;

	for (KeyValues* pKey = pKV->GetFirstSubKey(); pKey; pKey = pKey->GetNextKey())
	{
		for (KeyValues* pSubKey = pKey->GetFirstSubKey(); pSubKey; pSubKey = pSubKey->GetNextKey())
		{
			ordered_json jsonClass;

			if (pSubKey->FindKey("enabled"))
				jsonClass["enabled"] = pSubKey->GetBool("enabled");
			if (pSubKey->FindKey("team_default"))
				jsonClass["team_default"] = pSubKey->GetBool("team_default");
			if (pSubKey->FindKey("base"))
				jsonClass["base"] = std::string(pSubKey->GetString("base"));
			if (pSubKey->FindKey("health"))
				jsonClass["health"] = pSubKey->GetInt("health");
			if (pSubKey->FindKey("model"))
				jsonClass["models"][0]["modelname"] = std::string(pSubKey->GetString("model"));
			if (pSubKey->FindKey("color"))
				jsonClass["models"][0]["color"] = std::string(pSubKey->GetString("color"));
			if (pSubKey->FindKey("skin"))
				jsonClass["models"][0]["skins"] = pSubKey->GetInt("skin");
			if (pSubKey->FindKey("scale"))
				// combating float imprecision when writing to .jsonc
				jsonClass["scale"] = std::stod(pSubKey->GetString("scale"));
			if (pSubKey->FindKey("speed"))
				jsonClass["speed"] = std::stod(pSubKey->GetString("speed"));
			if (pSubKey->FindKey("gravity"))
				jsonClass["gravity"] = std::stod(pSubKey->GetString("gravity"));
			if (pSubKey->FindKey("admin_flag"))
				jsonClass["admin_flag"] = std::string(pSubKey->GetString("admin_flag"));
			if (pSubKey->FindKey("health_regen_count"))
				jsonClass["health_regen_count"] = pSubKey->GetInt("health_regen_count");
			if (pSubKey->FindKey("health_regen_interval"))
				jsonClass["health_regen_interval"] = std::stod(pSubKey->GetString("health_regen_interval"));

			jsonPlayerClasses[pKey->GetName()][pSubKey->GetName()] = jsonClass;
		}
	}

	const char *pszJsonPath = "addons/cs2fixes/configs/zr/playerclass.jsonc";
	const char *pszKVConfigRenamePath = "addons/cs2fixes/configs/zr/playerclass_old.cfg";
	char szPath[MAX_PATH];
	V_snprintf(szPath, sizeof(szPath), "%s%s%s", Plat_GetGameDirectory(), "/csgo/", pszJsonPath);
	std::ofstream jsoncFile(szPath);

	if (!jsoncFile.is_open())
	{
		Warning("Failed to open %s\n", pszJsonPath);
		jsoncFile.close();
		return false;
	}

	jsoncFile << std::setfill('\t') << std::setw(1) << jsonPlayerClasses << std::endl;
	jsoncFile.close();

	char szKVRenamePath[MAX_PATH];
	V_snprintf(szPath, sizeof(szPath), "%s%s%s", Plat_GetGameDirectory(), "/csgo/", pszPath);
	V_snprintf(szKVRenamePath, sizeof(szPath), "%s%s%s", Plat_GetGameDirectory(), "/csgo/", pszKVConfigRenamePath);

	std::rename(szPath, szKVRenamePath);

	// remove old cfg example if it exists
	const char *pszKVExamplePath = "addons/cs2fixes/configs/zr/playerclass.cfg.example";
	V_snprintf(szPath, sizeof(szPath), "%s%s%s", Plat_GetGameDirectory(), "/csgo/", pszKVExamplePath);
	std::remove(szPath);

	Message("Created JSON player class config at %s\n", pszJsonPath);
	Message("Renamed %s to %s\n", pszPath, pszKVConfigRenamePath);
	return true;
}

void CZRPlayerClassManager::LoadPlayerClass()
{
	Message("Loading PlayerClass...\n");
	m_ZombieClassMap.Purge();
	m_HumanClassMap.Purge();
	m_vecZombieDefaultClass.Purge();
	m_vecHumanDefaultClass.Purge();

	const char *pszJsonPath = "addons/cs2fixes/configs/zr/playerclass.jsonc";
	char szPath[MAX_PATH];
	V_snprintf(szPath, sizeof(szPath), "%s%s%s", Plat_GetGameDirectory(), "/csgo/", pszJsonPath);
	std::ifstream jsoncFile(szPath);

	if (!jsoncFile.is_open())
	{
		Message("Failed to open %s\n", pszJsonPath);
		bool bJsonCreated = CreateJsonConfigFromKeyValuesFile();
		if (!bJsonCreated)
		{
			Panic("Playerclass config conversion failed. Playerclasses not loaded\n");
			jsoncFile.close();
			return;
		}
		jsoncFile.open(szPath);
	}

	// Less code than constantly traversing the full class vectors, temporary lifetime anyways
	std::set<std::string> setClassNames;
	ordered_json jsonPlayerClasses = ordered_json::parse(jsoncFile, nullptr, true, true);

	for (auto& [szTeamName, jsonTeamClasses] : jsonPlayerClasses.items())
	{
		bool bHuman = szTeamName == "Human";
		if (bHuman)
			Message("Human Classes:\n");
		else
			Message("Zombie Classes:\n");

		for (auto& [szClassName, jsonClass] : jsonTeamClasses.items())
		{
			bool bEnabled = jsonClass.value("enabled", false);
			bool bTeamDefault = jsonClass.value("team_default", false);

			std::string szBase = jsonClass.value("base", "");

			bool bMissingKey = false;

			if (setClassNames.contains(szClassName))
			{
				Panic("A class named %s already exists!\n", szClassName.c_str());
				bMissingKey = true;
			}

			if (!jsonClass.contains("team_default"))
			{
				Panic("%s has unspecified key: team_default\n", szClassName.c_str());
				bMissingKey = true;
			}

			// check everything if no base class
			if (szBase.empty())
			{
				if (!jsonClass.contains("health"))
				{
					Panic("%s has unspecified key: health\n", szClassName.c_str());
					bMissingKey = true;
				}
				if (!jsonClass.contains("models"))
				{
					Panic("%s has unspecified key: models\n", szClassName.c_str());
					bMissingKey = true;
				}
				else if (jsonClass["models"].size() < 1)
				{
					Panic("%s has no model entries\n", szClassName.c_str());
					bMissingKey = true;
				}
				else
				{
					for (auto& [key, jsonModelEntry] : jsonClass["models"].items())
					{
						if (!jsonModelEntry.contains("modelname"))
						{
							Panic("%s has unspecified model entry key: modelname\n", szClassName.c_str());
							bMissingKey = true;
						}
					}
					// BASE CLASS BEHAVIOUR: if not present, skins defaults to [0] and color defaults to "255 255 255"
				}
				if (!jsonClass.contains("scale"))
				{
					Panic("%s has unspecified key: scale\n", szClassName.c_str());
					bMissingKey = true;
				}
				if (!jsonClass.contains("speed"))
				{
					Panic("%s has unspecified key: speed\n", szClassName.c_str());
					bMissingKey = true;
				}
				if (!jsonClass.contains("gravity"))
				{
					Panic("%s has unspecified key: gravity\n", szClassName.c_str());
					bMissingKey = true;
				}
				if (!jsonClass.contains("admin_flag"))
				{
					Panic("%s has unspecified key: admin_flag\n", szClassName.c_str());
					bMissingKey = true;
				}
			}
			if (bMissingKey)
				continue;

			if (bHuman)
			{
				ZRHumanClass *pHumanClass;
				if (!szBase.empty())
				{
					ZRHumanClass *pBaseHumanClass = GetHumanClass(szBase.c_str());
					if (pBaseHumanClass)
					{
						pHumanClass = new ZRHumanClass(pBaseHumanClass);
						pHumanClass->Override(jsonClass, szClassName);
					}
					else
					{
						Panic("Could not find specified base \"%s\" for %s!!!\n", szBase.c_str(), szClassName.c_str());
						continue;
					}
				}
				else
					pHumanClass = new ZRHumanClass(jsonClass, szClassName);

				m_HumanClassMap.Insert(hash_32_fnv1a_const(szClassName.c_str()), pHumanClass);

				if (bTeamDefault)
					m_vecHumanDefaultClass.AddToTail(pHumanClass);
				
				pHumanClass->PrintInfo();
			}
			else 
			{
				ZRZombieClass *pZombieClass;
				if (!szBase.empty())
				{
					ZRZombieClass *pBaseZombieClass = GetZombieClass(szBase.c_str());
					if (pBaseZombieClass)
					{
						pZombieClass = new ZRZombieClass(pBaseZombieClass);
						pZombieClass->Override(jsonClass, szClassName);
					}
					else
					{
						Panic("Could not find specified base \"%s\" for %s!!!\n", szBase.c_str(), szClassName.c_str());
						continue;
					}
				}
				else
					pZombieClass = new ZRZombieClass(jsonClass, szClassName);

				m_ZombieClassMap.Insert(hash_32_fnv1a_const(szClassName.c_str()), pZombieClass);
				if (bTeamDefault)
					m_vecZombieDefaultClass.AddToTail(pZombieClass);
				
				pZombieClass->PrintInfo();
			}

			setClassNames.insert(szClassName);
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
	ZRModelEntry *pModelEntry = pClass->GetRandomModelEntry();
	Color clrRender;
	V_StringToColor(pModelEntry->szColor.c_str(), clrRender);

	pPawn->m_iMaxHealth = pClass->iHealth;
	pPawn->m_iHealth = pClass->iHealth;
	pPawn->SetModel(pModelEntry->szModelPath.c_str());
	pPawn->m_clrRender = clrRender;
	pPawn->AcceptInput("Skin", pModelEntry->GetRandomSkin());
	pPawn->m_flGravityScale = pClass->flGravity;

	// I don't know why, I don't want to know why,
	// I shouldn't have to wonder why, but for whatever reason
	// this shit caused crashes on ROUND END or MAP CHANGE after the 26/04/2024 update
	//pPawn->m_flVelocityModifier = pClass->flSpeed;
	const auto pController = reinterpret_cast<CCSPlayerController*>(pPawn->GetController());
	if (const auto pPlayer = pController != nullptr ? pController->GetZEPlayer() : nullptr)
	{
		pPlayer->SetMaxSpeed(pClass->flSpeed);
	}

	// This has to be done a bit later
	UTIL_AddEntityIOEvent(pPawn, "SetScale", nullptr, nullptr, pClass->flScale);
}

// only changes that should not (directly) affect gameplay
void CZRPlayerClassManager::ApplyBaseClassVisuals(ZRClass *pClass, CCSPlayerPawn *pPawn)
{
	ZRModelEntry *pModelEntry = pClass->GetRandomModelEntry();
	Color clrRender;
	V_StringToColor(pModelEntry->szColor.c_str(), clrRender);
	
	pPawn->SetModel(pModelEntry->szModelPath.c_str());
	pPawn->m_clrRender = clrRender;
	pPawn->AcceptInput("Skin", pModelEntry->GetRandomSkin());

	// This has to be done a bit later
	UTIL_AddEntityIOEvent(pPawn, "SetScale", nullptr, nullptr, pClass->flScale);
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
	
	if (!g_bEnableLeader || !pController)
		return;
	
	ZEPlayer *pPlayer = g_playerManager->GetPlayer(pController->GetPlayerSlot());

	if (pPlayer && pPlayer->IsLeader())
	{
		CHandle<CCSPlayerPawn> hPawn = pPawn->GetHandle();

		new CTimer(0.02f, false, false, [hPawn]()
		{
			CCSPlayerPawn *pPawn = hPawn.Get();
			if (pPawn)
				Leader_ApplyLeaderVisuals(pPawn);
			return -1.0f;
		});
	}
}

void CZRPlayerClassManager::ApplyPreferredOrDefaultHumanClass(CCSPlayerPawn *pPawn)
{
	CCSPlayerController *pController = CCSPlayerController::FromPawn(pPawn);
	if (!pController) return;

	// Get the human class user preference, or default if no class is set
	int iSlot = pController->GetPlayerSlot();
	ZRHumanClass* humanClass = nullptr;
	const char* sPreferredHumanClass = g_pUserPreferencesSystem->GetPreference(iSlot, HUMAN_CLASS_KEY_NAME);

	// If the preferred human class exists and can be applied, override the default
	uint16 index = m_HumanClassMap.Find(hash_32_fnv1a_const(sPreferredHumanClass));
	if (m_HumanClassMap.IsValidIndex(index) && m_HumanClassMap[index]->IsApplicableTo(pController)) {
		humanClass = m_HumanClassMap[index];
	} else if (m_vecHumanDefaultClass.Count()) {
		humanClass = m_vecHumanDefaultClass[rand() % m_vecHumanDefaultClass.Count()];
	} else if (!humanClass) {
		Warning("Missing default human class or valid preferences!\n");
		return;
	}
	
	ApplyHumanClass(humanClass, pPawn);
}

void CZRPlayerClassManager::ApplyPreferredOrDefaultHumanClassVisuals(CCSPlayerPawn *pPawn)
{
	CCSPlayerController *pController = CCSPlayerController::FromPawn(pPawn);
	if (!pController) return;

	// Get the human class user preference, or default if no class is set
	int iSlot = pController->GetPlayerSlot();
	ZRHumanClass* humanClass = nullptr;
	const char* sPreferredHumanClass = g_pUserPreferencesSystem->GetPreference(iSlot, HUMAN_CLASS_KEY_NAME);

	// If the preferred human class exists and can be applied, override the default
	uint16 index = m_HumanClassMap.Find(hash_32_fnv1a_const(sPreferredHumanClass));
	if (m_HumanClassMap.IsValidIndex(index) && m_HumanClassMap[index]->IsApplicableTo(pController)) {
		humanClass = m_HumanClassMap[index];
	} else if (m_vecHumanDefaultClass.Count()) {
		humanClass = m_vecHumanDefaultClass[rand() % m_vecHumanDefaultClass.Count()];
	} else if (!humanClass) {
		Warning("Missing default human class or valid preferences!\n");
		return;
	}

	ApplyBaseClassVisuals((ZRClass *)humanClass, pPawn);
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

void CZRPlayerClassManager::ApplyPreferredOrDefaultZombieClass(CCSPlayerPawn *pPawn)
{
	CCSPlayerController *pController = CCSPlayerController::FromPawn(pPawn);
	if (!pController) return;

	// Get the zombie class user preference, or default if no class is set
	int iSlot = pController->GetPlayerSlot();
	ZRZombieClass* zombieClass = nullptr;
	const char* sPreferredZombieClass = g_pUserPreferencesSystem->GetPreference(iSlot, ZOMBIE_CLASS_KEY_NAME);

	// If the preferred zombie class exists and can be applied, override the default
	uint16 index = m_ZombieClassMap.Find(hash_32_fnv1a_const(sPreferredZombieClass));
	if (m_ZombieClassMap.IsValidIndex(index) && m_ZombieClassMap[index]->IsApplicableTo(pController)) {
		zombieClass = m_ZombieClassMap[index];
	} else if (m_vecZombieDefaultClass.Count()) {
		zombieClass = m_vecZombieDefaultClass[rand() % m_vecZombieDefaultClass.Count()];
	} else if (!zombieClass) {
		Warning("Missing default zombie class or valid preferences!\n");
		return;
	}
	
	ApplyZombieClass(zombieClass, pPawn);
}

void CZRPlayerClassManager::GetZRClassList(int iTeam, CUtlVector<ZRClass*> &vecClasses, CCSPlayerController* pController)
{
	if (iTeam == CS_TEAM_T || iTeam == CS_TEAM_NONE)
	{
		FOR_EACH_MAP_FAST(m_ZombieClassMap, i)
		{
			if (!pController || m_ZombieClassMap[i]->IsApplicableTo(pController))
				vecClasses.AddToTail(m_ZombieClassMap[i]);
		}
	}

	if (iTeam == CS_TEAM_CT || iTeam == CS_TEAM_NONE)
	{
		FOR_EACH_MAP_FAST(m_HumanClassMap, i)
		{
			if (!pController || m_HumanClassMap[i]->IsApplicableTo(pController))
				vecClasses.AddToTail(m_HumanClassMap[i]);
		}
	}
}

double CZRRegenTimer::s_flNextExecution;
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

	VPROF("CZRRegenTimer::Tick");

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

void ZR_OnLevelInit()
{
	g_ZRRoundState = EZRRoundState::ROUND_START;

	// Delay one tick to override any .cfg's
	new CTimer(0.02f, false, true, []()
	{
		// Here we force some cvars that are necessary for the gamemode
		g_pEngineServer2->ServerCommand("mp_give_player_c4 0");
		g_pEngineServer2->ServerCommand("mp_friendlyfire 0");
		g_pEngineServer2->ServerCommand("mp_ignore_round_win_conditions 1");
		// Necessary to fix bots kicked/joining infinitely when forced to CT https://github.com/Source2ZE/ZombieReborn/issues/64
		g_pEngineServer2->ServerCommand("bot_quota_mode fill");
		g_pEngineServer2->ServerCommand("mp_autoteambalance 0");
		// These disable most of the buy menu for zombies
		g_pEngineServer2->ServerCommand("mp_weapons_allow_pistols 3");
		g_pEngineServer2->ServerCommand("mp_weapons_allow_smgs 3");
		g_pEngineServer2->ServerCommand("mp_weapons_allow_heavy 3");
		g_pEngineServer2->ServerCommand("mp_weapons_allow_rifles 3");

		return -1.0f;
	});

	g_pZRWeaponConfig->LoadWeaponConfig();
	SetupCTeams();
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

		if (!pController || pController->m_bIsHLTV || (pController->m_iTeamNum() != CS_TEAM_CT && pController->m_iTeamNum() != CS_TEAM_T))
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
	ToggleRespawn(true, true);

	for (int i = 0; i < gpGlobals->maxClients; i++)
	{
		CCSPlayerController* pController = CCSPlayerController::FromSlot(i);

		if (!pController || pController->m_bIsHLTV)
			continue;

		// Only do this for Ts, ignore CTs and specs
		if (pController->m_iTeamNum() == CS_TEAM_T)
			pController->SwitchTeam(CS_TEAM_CT);

		CCSPlayerPawn *pPawn = pController->GetPlayerPawn();

		// Prevent damage that occurs between now and when the round restart is finished
		// Somehow CT filtered nukes can apply damage during the round restart (all within CCSGameRules::RestartRound)
		// And if everyone was a zombie at this moment, they will all die and trigger ANOTHER round restart which breaks everything
		if (pPawn)
			pPawn->m_bTakesDamage = false;
	}
}

void SetupRespawnToggler()
{
	CBaseEntity* relay = CreateEntityByName("logic_relay");
	CEntityKeyValues* pKeyValues = new CEntityKeyValues();

	pKeyValues->SetString("targetname", "zr_toggle_respawn");
	relay->DispatchSpawn(pKeyValues);
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

	for (int i = 0; i < gpGlobals->maxClients; i++)
	{
		CCSPlayerController *pController = CCSPlayerController::FromSlot(i);

		if (!pController)
			continue;

		CCSPlayerPawn *pPawn = pController->GetPlayerPawn();

		// Now we can enable damage back
		if (pPawn)
			pPawn->m_bTakesDamage = true;
	}
}

void ZR_OnPlayerSpawn(CCSPlayerController* pController)
{
	// delay infection a bit
	bool bInfect = g_ZRRoundState == EZRRoundState::POST_INFECTION;

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
	new CTimer(0.05f, false, false, [handle, bInfect]()
	{
		CCSPlayerController* pController = (CCSPlayerController*)handle.Get();
		if (!pController)
			return -1.0f;
		if(bInfect)
			ZR_Infect(pController, pController, true);
		else
			ZR_Cure(pController);
		return -1.0f;
	});
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

void ZR_ApplyKnockbackExplosion(CBaseEntity *pProjectile, CCSPlayerPawn *pVictim, int iDamage, bool bMolotov)
{
	ZRWeapon *pWeapon = g_pZRWeaponConfig->FindWeapon(pProjectile->GetClassname());
	if (!pWeapon)
		return;
	float flWeaponKnockbackScale = pWeapon->flKnockback;

	Vector vecDisplacement = pVictim->GetAbsOrigin() - pProjectile->GetAbsOrigin();
	vecDisplacement.z += 36;
	VectorNormalize(vecDisplacement);
	Vector vecKnockback = vecDisplacement;

	if (bMolotov)
		vecKnockback.z = 0;

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

	// it can sometimes be null when player joined on the very first round? 
	if (!pItemServices)
		return;

	pPawn->DropMapWeapons();
	pItemServices->StripPlayerWeapons(true);
	pItemServices->GiveNamedItem("weapon_knife");
}

void ZR_Cure(CCSPlayerController *pTargetController)
{
	if (pTargetController->m_iTeamNum() == CS_TEAM_T)
		pTargetController->SwitchTeam(CS_TEAM_CT);

	ZEPlayer *pZEPlayer = pTargetController->GetZEPlayer();

	if (pZEPlayer)
		pZEPlayer->SetInfectState(false);

	CCSPlayerPawn *pTargetPawn = (CCSPlayerPawn*)pTargetController->GetPawn();
	if (!pTargetPawn)
		return;

	g_pZRPlayerClassManager->ApplyPreferredOrDefaultHumanClass(pTargetPawn);
}

float ZR_MoanTimer(ZEPlayerHandle hPlayer)
{
	if (!hPlayer.IsValid())
		return -1.f;

	if (!hPlayer.Get()->IsInfected())
		return -1.f;

	CCSPlayerPawn *pPawn = CCSPlayerController::FromSlot(hPlayer.GetPlayerSlot())->GetPlayerPawn();

	if (!pPawn || pPawn->m_iTeamNum == CS_TEAM_CT)
		return -1.f;

	// This guy is dead but still infected, and corpses are quiet
	if (!pPawn->IsAlive())
		return g_flMoanInterval + (rand() % 5);

	pPawn->EmitSound("zr.amb.zombie_voice_idle");

	return g_flMoanInterval + (rand() % 5);
}

void ZR_InfectShake(CCSPlayerController *pController)
{
	if (!pController || !pController->IsConnected() || pController->IsBot() || !g_bInfectShake)
		return;

	INetworkMessageInternal *pNetMsg = g_pNetworkMessages->FindNetworkMessagePartial("Shake");

	auto data = pNetMsg->AllocateMessage()->ToPB<CUserMessageShake>();

	data->set_duration(g_flInfectShakeDuration);
	data->set_frequency(g_flInfectShakeFrequency);
	data->set_amplitude(g_flInfectShakeAmplitude);
	data->set_command(0);

	pController->GetServerSideClient()->GetNetChannel()->SendNetMessage(data, BUF_RELIABLE);

	delete data;
}

std::vector<SpawnPoint*> ZR_GetSpawns()
{
	CUtlVector<SpawnPoint*>* ctSpawns = g_pGameRules->m_CTSpawnPoints();
	CUtlVector<SpawnPoint*>* tSpawns = g_pGameRules->m_TerroristSpawnPoints();
	std::vector<SpawnPoint*> spawns;

	FOR_EACH_VEC(*ctSpawns, i)
		spawns.push_back((*ctSpawns)[i]);

	FOR_EACH_VEC(*tSpawns, i)
		spawns.push_back((*tSpawns)[i]);

	if (!spawns.size())
		Panic("There are no spawns!\n");

	return spawns;
}

void ZR_Infect(CCSPlayerController *pAttackerController, CCSPlayerController *pVictimController, bool bDontBroadcast)
{
	// This can be null if the victim disconnected right before getting hit AND someone joined in their place immediately, thus replacing the controller
	if (!pVictimController)
		return;

	if (pVictimController->m_iTeamNum() == CS_TEAM_CT)
		pVictimController->SwitchTeam(CS_TEAM_T);

	ZR_CheckTeamWinConditions(CS_TEAM_T);

	if (!bDontBroadcast)
		ZR_FakePlayerDeath(pAttackerController, pVictimController, "knife"); // or any other killicon

	CCSPlayerPawn *pVictimPawn = (CCSPlayerPawn*)pVictimController->GetPawn();
	if (!pVictimPawn)
		return;

	// We disabled damage due to the delayed infection, restore
	pVictimPawn->m_bTakesDamage(true);

	pVictimPawn->EmitSound("zr.amb.scream");

	ZR_StripAndGiveKnife(pVictimPawn);
	
	g_pZRPlayerClassManager->ApplyPreferredOrDefaultZombieClass(pVictimPawn);

	ZR_InfectShake(pVictimController);

	ZEPlayer *pZEPlayer = pVictimController->GetZEPlayer();

	if (pZEPlayer && !pZEPlayer->IsInfected())
	{
		pZEPlayer->SetInfectState(true);

		ZEPlayerHandle hPlayer = pZEPlayer->GetHandle();
		new CTimer(g_flMoanInterval + (rand() % 5), false, false, [hPlayer]() { return ZR_MoanTimer(hPlayer); });
	}
}

void ZR_InfectMotherZombie(CCSPlayerController *pVictimController, std::vector<SpawnPoint*> spawns)
{
	CCSPlayerPawn *pVictimPawn = (CCSPlayerPawn*)pVictimController->GetPawn();
	if (!pVictimPawn)
		return;

	ZR_StripAndGiveKnife(pVictimPawn);

	// pick random spawn point
	if (g_iInfectSpawnType == EZRSpawnType::RESPAWN)
	{
		int randomindex = rand() % spawns.size();
		Vector origin = spawns[randomindex]->GetAbsOrigin();
		QAngle rotation = spawns[randomindex]->GetAbsRotation();

		pVictimPawn->Teleport(&origin, &rotation, &vec3_origin);
	}

	pVictimController->SwitchTeam(CS_TEAM_T);
	pVictimPawn->EmitSound("zr.amb.scream");

	ZRZombieClass *pClass = g_pZRPlayerClassManager->GetZombieClass("MotherZombie");
	if (pClass)
		g_pZRPlayerClassManager->ApplyZombieClass(pClass, pVictimPawn);
	else
		g_pZRPlayerClassManager->ApplyPreferredOrDefaultZombieClass(pVictimPawn);

	ZR_InfectShake(pVictimController);

	ZEPlayer *pZEPlayer = pVictimController->GetZEPlayer();

	pZEPlayer->SetInfectState(true);

	ZEPlayerHandle hPlayer = pZEPlayer->GetHandle();
	new CTimer(g_flMoanInterval + (rand() % 5), false, false, [hPlayer]() { return ZR_MoanTimer(hPlayer); });
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
		if (!pController || !pController->IsConnected() || pController->m_iTeamNum() != CS_TEAM_CT)
			continue;

		CCSPlayerPawn* pPawn = (CCSPlayerPawn*)pController->GetPawn();
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
	std::vector<SpawnPoint*> spawns = ZR_GetSpawns();
	if (g_iInfectSpawnType == EZRSpawnType::RESPAWN && !spawns.size())
	{
		ClientPrintAll(HUD_PRINTTALK, ZR_PREFIX"There are no spawns!");
		return;
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

			ZR_InfectMotherZombie(pController, spawns);
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

	if (g_flRespawnDelay < 0.0f)
		g_bRespawnEnabled = false;

	ClientPrintAll(HUD_PRINTCENTER, "First infection has started!");
	ClientPrintAll(HUD_PRINTTALK, ZR_PREFIX "First infection has started! Good luck, survivors!");
	g_ZRRoundState = EZRRoundState::POST_INFECTION;
}

void ZR_StartInitialCountdown()
{
	if (g_iInfectSpawnTimeMin > g_iInfectSpawnTimeMax)
		V_swap(g_iInfectSpawnTimeMin, g_iInfectSpawnTimeMax);

	g_iInfectionCountDown = g_iInfectSpawnTimeMin + (rand() % (g_iInfectSpawnTimeMax - g_iInfectSpawnTimeMin + 1));
	new CTimer(0.0f, false, false, []()
	{
		if (g_ZRRoundState != EZRRoundState::ROUND_START)
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

bool ZR_Hook_OnTakeDamage_Alive(CTakeDamageInfo *pInfo, CCSPlayerPawn *pVictimPawn)
{
	CCSPlayerPawn* pAttackerPawn = (CCSPlayerPawn*)pInfo->m_hAttacker.Get();

	if (!(pAttackerPawn && pVictimPawn && pAttackerPawn->IsPawn() && pVictimPawn->IsPawn()))
		return false;

	CCSPlayerController *pAttackerController = CCSPlayerController::FromPawn(pAttackerPawn);
	CCSPlayerController *pVictimController = CCSPlayerController::FromPawn(pVictimPawn);
	const char *pszAbilityClass = pInfo->m_hAbility.Get() ? pInfo->m_hAbility.Get()->GetClassname() : "";
	if (pAttackerPawn->m_iTeamNum() == CS_TEAM_T && pVictimPawn->m_iTeamNum() == CS_TEAM_CT && !V_strncmp(pszAbilityClass, "weapon_knife", 12))
	{
		ZR_Infect(pAttackerController, pVictimController, false);
		return true; // nullify the damage
	}

	if (g_iGroanChance && pVictimPawn->m_iTeamNum() == CS_TEAM_T && (rand() % g_iGroanChance) == 1)
		pVictimPawn->EmitSound("zr.amb.zombie_pain");

	// grenade and molotov knockback
	if (pAttackerPawn->m_iTeamNum() == CS_TEAM_CT && pVictimPawn->m_iTeamNum() == CS_TEAM_T)
	{
		CBaseEntity *pInflictor = pInfo->m_hInflictor.Get();
		const char *pszInflictorClass = pInflictor ? pInflictor->GetClassname() : "";
		// inflictor class from grenade damage is actually hegrenade_projectile
		bool bGrenade = V_strncmp(pszInflictorClass, "hegrenade", 9) == 0;
		bool bInferno = V_strncmp(pszInflictorClass, "inferno", 7) == 0;

		if (g_bNapalmGrenades && bGrenade)
		{
			// Scale burn duration by damage, so nades from farther away burn zombies for less time
			float flDuration = (pInfo->m_flDamage / g_flNapalmFullDamage) * g_flNapalmDuration;
			flDuration = clamp(flDuration, 0.f, g_flNapalmDuration);

			// Can't use the same inflictor here as it'll end up calling this again each burn damage tick
			// DMG_BURN makes loud noises so use DMG_FALL instead which is completely silent
			IgnitePawn(pVictimPawn, flDuration, pAttackerPawn, pAttackerPawn, nullptr, DMG_FALL);
		}

		if (bGrenade || bInferno)
			ZR_ApplyKnockbackExplosion((CBaseEntity*)pInflictor, (CCSPlayerPawn*)pVictimPawn, (int)pInfo->m_flDamage, bInferno);
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

	CBaseEntity* relay = g_hRespawnToggler.Get();
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
	new CTimer(2.0f, false, false, [handle]()
	{
		CCSPlayerController* pController = (CCSPlayerController*)handle.Get();
		if (!pController || !g_bRespawnEnabled || pController->m_iTeamNum < CS_TEAM_T)
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

	if (pVictimPawn->m_iTeamNum() == CS_TEAM_T && g_ZRRoundState == EZRRoundState::POST_INFECTION)
		pVictimPawn->EmitSound("zr.amb.zombie_die");

	// respawn player
	CHandle<CCSPlayerController> handle = pVictimController->GetHandle();
	new CTimer(g_flRespawnDelay < 0.0f ? 2.0f : g_flRespawnDelay, false, false, [handle]()
	{
		CCSPlayerController* pController = (CCSPlayerController*)handle.Get();
		if (!pController || !g_bRespawnEnabled || pController->m_iTeamNum < CS_TEAM_T)
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
	new CTimer(10.0, false, false, []()
	{
		if (g_ZRRoundState == EZRRoundState::ROUND_END)
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
		if (!g_szHumanWinOverlayParticle.empty())
			ZR_CreateOverlay(g_szHumanWinOverlayParticle.c_str(), 1.0f, g_flHumanWinOverlaySize, flRestartDelay, Color(255, 255, 255), g_szHumanWinOverlayMaterial.c_str());
	}
	else if (iTeamNum == CS_TEAM_T)
	{	
		if (!g_hTeamT.Get())
		{
			Panic("Cannot find CTeam for T!\n");
			return;
		}
		g_hTeamT->m_iScore = g_hTeamT->m_iScore() + 1;
		if (!g_szZombieWinOverlayParticle.empty())
			ZR_CreateOverlay(g_szZombieWinOverlayParticle.c_str(), 1.0f, g_flZombieWinOverlaySize, flRestartDelay, Color(255, 255, 255), g_szZombieWinOverlayMaterial.c_str());
	}
}

CON_COMMAND_CHAT(ztele, "- teleport to spawn")
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

	std::vector<SpawnPoint*> spawns = ZR_GetSpawns();
	if (!spawns.size())
	{
		ClientPrint(player, HUD_PRINTTALK, ZR_PREFIX"There are no spawns!");
		return;
	}

	//Pick and get random spawnpoint
	int randomindex = rand() % spawns.size();
	CHandle<SpawnPoint> spawnHandle = spawns[randomindex]->GetHandle();

	//Here's where the mess starts
	CBasePlayerPawn *pPawn = player->GetPawn();

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

	CHandle<CCSPlayerPawn> pawnHandle = pPawn->GetHandle();

	new CTimer(5.0f, false, false, [spawnHandle, pawnHandle, initialpos]()
	{
		CCSPlayerPawn* pPawn = pawnHandle.Get();
		SpawnPoint* pSpawn = spawnHandle.Get();

		if (!pPawn || !pSpawn)
			return -1.0f;

		Vector endpos = pPawn->GetAbsOrigin();

		if (initialpos.DistTo(endpos) < g_flMaxZteleDistance)
		{
			Vector origin = pSpawn->GetAbsOrigin();
			QAngle rotation = pSpawn->GetAbsRotation();

			pPawn->Teleport(&origin, &rotation, nullptr);
			ClientPrint(pPawn->GetOriginalController(), HUD_PRINTTALK, ZR_PREFIX "You have been teleported to spawn.");
		}
		else
		{
			ClientPrint(pPawn->GetOriginalController(), HUD_PRINTTALK, ZR_PREFIX "Teleport failed! You moved too far.");
		}

		return -1.0f;
	});
}

CON_COMMAND_CHAT(zclass, "<teamname/class name/number> - find and select your Z:R classes")
{
	// Silently return so the command is completely hidden
	if (!g_bEnableZR)
		return;

	if (!player)
	{
		ClientPrint(player, HUD_PRINTCONSOLE, ZR_PREFIX "You cannot use this command from the server console.");
		return;
	}

	CUtlVector<ZRClass*> vecClasses;
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
				ClientPrint(player, HUD_PRINTTALK, ZR_PREFIX "Your current %s class is: \x10%s\x1. Available classes:", sTeamName, sCurrentClass);
			else
				ClientPrint(player, HUD_PRINTTALK, ZR_PREFIX "Available %s classes:", sTeamName);

			FOR_EACH_VEC(vecClasses, i)
			{
				if (vecClasses[i]->iTeam == team)
					ClientPrint(player, HUD_PRINTTALK, "%i. %s", i+1, vecClasses[i]->szClassName.c_str());
			}
		}

		ClientPrint(player, HUD_PRINTTALK, ZR_PREFIX "Select a class using \x2!zclass <class name/number>");
		return;
	}

	FOR_EACH_VEC(vecClasses, i)
	{
		const char* sClassName = vecClasses[i]->szClassName.c_str();
		bool bClassMatches = !V_stricmp(sClassName, args[1]) || (V_StringToInt32(args[1], -1) - 1) == i;
		ZRClass* pClass = vecClasses[i];

		if (bClassMatches)
		{
			ClientPrint(player, HUD_PRINTTALK, ZR_PREFIX "Your %s class is now set to \x10%s\x1.", pClass->iTeam == CS_TEAM_CT ? "Human" : "Zombie", sClassName);
			g_pUserPreferencesSystem->SetPreference(iSlot, pClass->iTeam == CS_TEAM_CT ? HUMAN_CLASS_KEY_NAME : ZOMBIE_CLASS_KEY_NAME, sClassName);
			return;
		}
	}

	ClientPrint(player, HUD_PRINTTALK, ZR_PREFIX "No available classes matched \x10%s\x1.", args[1]);
}

CON_COMMAND_CHAT_FLAGS(infect, "infect a player", ADMFLAG_GENERIC)
{
	// Silently return so the command is completely hidden
	if (!g_bEnableZR)
		return;

	if (args.ArgC() < 2)
	{
		ClientPrint(player, HUD_PRINTTALK, ZR_PREFIX "Usage: !infect <name>");
		return;
	}

	if (g_ZRRoundState == EZRRoundState::ROUND_END)
	{
		ClientPrint(player, HUD_PRINTTALK, ZR_PREFIX "The round is already over!");
		return;
	}

	int iCommandPlayer = player ? player->GetPlayerSlot() : -1;
	int iNumClients = 0;
	int pSlots[MAXPLAYERS];

	ETargetType nType = g_playerManager->TargetPlayerString(iCommandPlayer, args[1], iNumClients, pSlots);

	if (!iNumClients)
	{
		ClientPrint(player, HUD_PRINTTALK, ZR_PREFIX "Target not found.");
		return;
	}

	if (nType == ETargetType::PLAYER && iNumClients > 1)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "More than one client matched.");
		return;
	}

	const char* pszCommandPlayerName = player ? player->GetPlayerName() : "Console";
	std::vector<SpawnPoint*> spawns = ZR_GetSpawns();

	if (g_iInfectSpawnType == EZRSpawnType::RESPAWN && !spawns.size())
	{
		ClientPrint(player, HUD_PRINTTALK, ZR_PREFIX"There are no spawns!");
		return;
	}

	for (int i = 0; i < iNumClients; i++)
	{
		CCSPlayerController* pTarget = CCSPlayerController::FromSlot(pSlots[i]);

		if (!pTarget)
			continue;

		CCSPlayerPawn* pPawn = (CCSPlayerPawn*)pTarget->GetPawn();

		if (pTarget->m_iTeamNum() != CS_TEAM_CT || !pPawn || !pPawn->IsAlive())
		{
			ClientPrint(player, HUD_PRINTTALK, ZR_PREFIX "%s is not an alive human!", pTarget->GetPlayerName());
			continue;
		}

		if (g_ZRRoundState == EZRRoundState::ROUND_START)
			ZR_InfectMotherZombie(pTarget, spawns);
		else
			ZR_Infect(pTarget, pTarget, true);

		if (nType < ETargetType::ALL)
			PrintSingleAdminAction(pszCommandPlayerName, pTarget->GetPlayerName(), "infected", g_ZRRoundState == EZRRoundState::ROUND_START ? " as a mother zombie" : "", ZR_PREFIX);
	}

	PrintMultiAdminAction(nType, pszCommandPlayerName, "infected", g_ZRRoundState == EZRRoundState::ROUND_START ? " as mother zombies" : "", ZR_PREFIX);

	// Note we skip MZ immunity when first infection is manually triggered
	if (g_ZRRoundState == EZRRoundState::ROUND_START)
	{
		if (g_flRespawnDelay < 0.0f)
			g_bRespawnEnabled = false;

		g_ZRRoundState = EZRRoundState::POST_INFECTION;
	}
}

CON_COMMAND_CHAT_FLAGS(revive, "revive a player", ADMFLAG_GENERIC)
{
	// Silently return so the command is completely hidden
	if (!g_bEnableZR)
		return;

	if (args.ArgC() < 2)
	{
		ClientPrint(player, HUD_PRINTTALK, ZR_PREFIX "Usage: !revive <name>");
		return;
	}

	if (g_ZRRoundState != EZRRoundState::POST_INFECTION)
	{
		ClientPrint(player, HUD_PRINTTALK, ZR_PREFIX "A round is not ongoing!");
		return;
	}

	int iCommandPlayer = player ? player->GetPlayerSlot() : -1;
	int iNumClients = 0;
	int pSlots[MAXPLAYERS];

	ETargetType nType = g_playerManager->TargetPlayerString(iCommandPlayer, args[1], iNumClients, pSlots);

	if (!iNumClients)
	{
		ClientPrint(player, HUD_PRINTTALK, ZR_PREFIX "Target not found.");
		return;
	}

	if (nType == ETargetType::PLAYER && iNumClients > 1)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "More than one client matched.");
		return;
	}

	const char* pszCommandPlayerName = player ? player->GetPlayerName() : "Console";

	for (int i = 0; i < iNumClients; i++)
	{
		CCSPlayerController* pTarget = CCSPlayerController::FromSlot(pSlots[i]);

		if (!pTarget)
			continue;

		CCSPlayerPawn* pPawn = (CCSPlayerPawn*)pTarget->GetPawn();

		if (pTarget->m_iTeamNum() != CS_TEAM_T || !pPawn || !pPawn->IsAlive())
		{
			ClientPrint(player, HUD_PRINTTALK, ZR_PREFIX "%s is not an alive zombie!", pTarget->GetPlayerName());
			continue;
		}

		ZR_Cure(pTarget);

		if (nType < ETargetType::ALL)
			PrintSingleAdminAction(pszCommandPlayerName, pTarget->GetPlayerName(), "revived", "", ZR_PREFIX);
	}

	PrintMultiAdminAction(nType, pszCommandPlayerName, "revived", "", ZR_PREFIX);
}