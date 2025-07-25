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
#include "zombiereborn.h"
#include <fstream>
#include <sstream>

#include "tier0/memdbgon.h"

using ordered_json = nlohmann::ordered_json;

extern CGameEntitySystem* g_pEntitySystem;
extern IVEngineServer2* g_pEngineServer2;
extern CGlobalVars* GetGlobals();
extern CCSGameRules* g_pGameRules;
extern IGameEventManager2* g_gameEventManager;
extern IGameEventSystem* g_gameEventSystem;
extern double g_flUniversalTime;
extern CConVar<int> g_cvarFreeArmor;

void ZR_Infect(CCSPlayerController* pAttackerController, CCSPlayerController* pVictimController, bool bBroadcast);
bool ZR_CheckTeamWinConditions(int iTeamNum);
void ZR_Cure(CCSPlayerController* pTargetController);
void ZR_EndRoundAndAddTeamScore(int iTeamNum);
void SetupCTeams();
bool ZR_IsTeamAlive(int iTeamNum);

EZRRoundState g_ZRRoundState = EZRRoundState::ROUND_START;
static bool g_bRespawnEnabled = true;
static CHandle<CBaseEntity> g_hRespawnToggler;
static CHandle<CTeam> g_hTeamCT;
static CHandle<CTeam> g_hTeamT;

CZRPlayerClassManager* g_pZRPlayerClassManager = nullptr;
ZRWeaponConfig* g_pZRWeaponConfig = nullptr;
ZRHitgroupConfig* g_pZRHitgroupConfig = nullptr;

CConVar<bool> g_cvarEnableZR("zr_enable", FCVAR_NONE, "Whether to enable ZR features", false);
CConVar<float> g_cvarMaxZteleDistance("zr_ztele_max_distance", FCVAR_NONE, "Maximum distance players are allowed to move after starting ztele", 150.0f, true, 0.0f, false, 0.0f);
CConVar<bool> g_cvarZteleHuman("zr_ztele_allow_humans", FCVAR_NONE, "Whether to allow humans to use ztele", false);
CConVar<float> g_cvarKnockbackScale("zr_knockback_scale", FCVAR_NONE, "Global knockback scale", 5.0f);
CConVar<int> g_cvarInfectSpawnType("zr_infect_spawn_type", FCVAR_NONE, "Type of Mother Zombies Spawn [0 = MZ spawn where they stand, 1 = MZ get teleported back to spawn on being picked]", (int)EZRSpawnType::RESPAWN, true, 0, true, 1);
CConVar<bool> g_cvarInfectSpawnWarning("zr_infect_spawn_warning", FCVAR_NONE, "Whether to warn players of zombies spawning between humans", true);
CConVar<int> g_cvarInfectSpawnTimeMin("zr_infect_spawn_time_min", FCVAR_NONE, "Minimum time in which Mother Zombies should be picked, after round start", 15, true, 0, false, 0);
CConVar<int> g_cvarInfectSpawnTimeMax("zr_infect_spawn_time_max", FCVAR_NONE, "Maximum time in which Mother Zombies should be picked, after round start", 15, true, 1, false, 0);
CConVar<int> g_cvarInfectSpawnMZRatio("zr_infect_spawn_mz_ratio", FCVAR_NONE, "Ratio of all Players to Mother Zombies to be spawned at round start", 7, true, 1, true, 64);
CConVar<int> g_cvarInfectSpawnMinCount("zr_infect_spawn_mz_min_count", FCVAR_NONE, "Minimum amount of Mother Zombies to be spawned at round start", 1, true, 0, false, 0);
CConVar<float> g_cvarRespawnDelay("zr_respawn_delay", FCVAR_NONE, "Time before a zombie is automatically respawned, -1 disables this. Note that maps can still manually respawn at any time", 5.0f, true, -1.0f, false, 0.0f);
CConVar<int> g_cvarDefaultWinnerTeam("zr_default_winner_team", FCVAR_NONE, "Which team wins when time ran out [1 = Draw, 2 = Zombies, 3 = Humans]", CS_TEAM_SPECTATOR, true, 1, true, 3);
CConVar<int> g_cvarMZImmunityReduction("zr_mz_immunity_reduction", FCVAR_NONE, "How much mz immunity to reduce for each player per round (0-100)", 20, true, 0, true, 100);
CConVar<int> g_cvarGroanChance("zr_sounds_groan_chance", FCVAR_NONE, "How likely should a zombie groan whenever they take damage (1 / N)", 5, true, 1, false, 0);
CConVar<float> g_cvarMoanInterval("zr_sounds_moan_interval", FCVAR_NONE, "How often in seconds should zombies moan", 30.0f, true, 0.0f, false, 0.0f);
CConVar<bool> g_cvarNapalmGrenades("zr_napalm_enable", FCVAR_NONE, "Whether to use napalm grenades", true);
CConVar<float> g_cvarNapalmDuration("zr_napalm_burn_duration", FCVAR_NONE, "How long in seconds should zombies burn from napalm grenades", 5.0f, true, 0.0f, false, 0.0f);
CConVar<float> g_cvarNapalmFullDamage("zr_napalm_full_damage", FCVAR_NONE, "The amount of damage needed to apply full burn duration for napalm grenades (max grenade damage is 99)", 50.0f, true, 0.0f, true, 99.0f);
CConVar<CUtlString> g_cvarHumanWinOverlayParticle("zr_human_win_overlay_particle", FCVAR_NONE, "Screenspace particle to display when human win", "");
CConVar<CUtlString> g_cvarHumanWinOverlayMaterial("zr_human_win_overlay_material", FCVAR_NONE, "Material override for human's win overlay particle", "");
CConVar<float> g_cvarHumanWinOverlaySize("zr_human_win_overlay_size", FCVAR_NONE, "Size of human's win overlay particle", 100.0f, true, 0.0f, true, 100.0f);
CConVar<CUtlString> g_cvarZombieWinOverlayParticle("zr_zombie_win_overlay_particle", FCVAR_NONE, "Screenspace particle to display when zombie win", "");
CConVar<CUtlString> g_cvarZombieWinOverlayMaterial("zr_zombie_win_overlay_material", FCVAR_NONE, "Material override for zombie's win overlay particle", "");
CConVar<float> g_cvarZombieWinOverlaySize("zr_zombie_win_overlay_size", FCVAR_NONE, "Size of zombie's win overlay particle", 100.0f, true, 0.0f, true, 100.0f);
CConVar<bool> g_cvarInfectShake("zr_infect_shake", FCVAR_NONE, "Whether to shake a player's view on infect", true);
CConVar<float> g_cvarInfectShakeAmplitude("zr_infect_shake_amp", FCVAR_NONE, "Amplitude of shaking effect", 15.0f, true, 0.0f, true, 16.0f);
CConVar<float> g_cvarInfectShakeFrequency("zr_infect_shake_frequency", FCVAR_NONE, "Frequency of shaking effect", 2.0f, true, 0.0f, false, 0.0f);
CConVar<float> g_cvarInfectShakeDuration("zr_infect_shake_duration", FCVAR_NONE, "Duration of shaking effect", 5.0f, true, 0.0f, false, 0.0f);
CConVar<float> g_cvarDamageCashScale("zr_damage_cash_scale", FCVAR_NONE, "Multiplier on cash given when damaging zombies (0.0 = disabled)", 0.0f, true, 0.0f, false, 100.0f);

// meant only for offline config validation and can easily cause issues when used on live server
#ifdef _DEBUG
CON_COMMAND_F(zr_reload_classes, "- Reload ZR player classes", FCVAR_SPONLY | FCVAR_LINKED_CONCOMMAND)
{
	g_pZRPlayerClassManager->LoadPlayerClass();

	Message("Reloaded ZR player classes.\n");
}
#endif

void ZR_Precache(IEntityResourceManifest* pResourceManifest)
{
	g_pZRPlayerClassManager->LoadPlayerClass();
	g_pZRPlayerClassManager->PrecacheModels(pResourceManifest);

	pResourceManifest->AddResource(g_cvarHumanWinOverlayParticle.Get().String());
	pResourceManifest->AddResource(g_cvarZombieWinOverlayParticle.Get().String());

	pResourceManifest->AddResource(g_cvarHumanWinOverlayMaterial.Get().String());
	pResourceManifest->AddResource(g_cvarZombieWinOverlayMaterial.Get().String());

	pResourceManifest->AddResource("soundevents/soundevents_zr.vsndevts");
}

void ZR_CreateOverlay(const char* pszOverlayParticlePath, float flAlpha, float flRadius, float flLifeTime, Color clrTint, const char* pszMaterialOverride)
{
	CEnvParticleGlow* particle = CreateEntityByName<CEnvParticleGlow>("env_particle_glow");

	CEntityKeyValues* pKeyValues = new CEntityKeyValues();

	pKeyValues->SetString("effect_name", pszOverlayParticlePath);
	// these properties are mapped to control point position by the entity
	pKeyValues->SetFloat("alphascale", flAlpha);		// 17.x
	pKeyValues->SetFloat("scale", flRadius);			// 17.y
	pKeyValues->SetFloat("selfillumscale", flLifeTime); // 17.z
	pKeyValues->SetColor("colortint", clrTint);			// 16.xyz

	pKeyValues->SetString("effect_textureOverride", pszMaterialOverride);

	particle->DispatchSpawn(pKeyValues);
	particle->AcceptInput("Start");

	UTIL_AddEntityIOEvent(particle, "Kill", nullptr, nullptr, "", flLifeTime + 1.0);
}

ZRModelEntry::ZRModelEntry(std::shared_ptr<ZRModelEntry> modelEntry) :
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
			for (auto& [key, skinIndex] : jsonModelEntry["skins"].items())
				vecSkins.AddToTail(skinIndex);
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
ZRClass::ZRClass(ordered_json jsonKeys, std::string szClassname, int iTeam) :
	iTeam(iTeam),
	bEnabled(jsonKeys["enabled"].get<bool>()),
	szClassName(szClassname),
	iHealth(jsonKeys["health"].get<int>()),
	flScale(jsonKeys["scale"].get<float>()),
	flSpeed(jsonKeys["speed"].get<float>()),
	flGravity(jsonKeys["gravity"].get<float>()),
	iAdminFlag(ParseClassFlags(
		jsonKeys["admin_flag"].get<std::string>().c_str()))
{
	vecModels.Purge();

	for (auto& [key, jsonModelEntry] : jsonKeys["models"].items())
	{
		std::shared_ptr<ZRModelEntry> modelEntry = std::make_shared<ZRModelEntry>(jsonModelEntry);
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
			jsonKeys["admin_flag"].get<std::string>().c_str());

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
	for (int i = jsonKeys["models"].size() - 1; i >= 0; i--)
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
		std::shared_ptr<ZRModelEntry> modelEntry = std::make_shared<ZRModelEntry>(jsonModelEntry);
		vecModels.AddToTail(modelEntry);
	}
}

ZRHumanClass::ZRHumanClass(ordered_json jsonKeys, std::string szClassname) :
	ZRClass(jsonKeys, szClassname, CS_TEAM_CT){};

ZRZombieClass::ZRZombieClass(ordered_json jsonKeys, std::string szClassname) :
	ZRClass(jsonKeys, szClassname, CS_TEAM_T),
	iHealthRegenCount(jsonKeys.value("health_regen_count", 0)),
	flHealthRegenInterval(jsonKeys.value("health_regen_interval", 0)),
	flKnockback(jsonKeys.value("knockback", 1.0)){};

void ZRZombieClass::Override(ordered_json jsonKeys, std::string szClassname)
{
	ZRClass::Override(jsonKeys, szClassname);
	if (jsonKeys.contains("health_regen_count"))
		iHealthRegenCount = jsonKeys["health_regen_count"].get<int>();
	if (jsonKeys.contains("health_regen_interval"))
		flHealthRegenInterval = jsonKeys["health_regen_interval"].get<float>();
	if (jsonKeys.contains("knockback"))
		flKnockback = jsonKeys["knockback"].get<float>();
}

bool ZRClass::IsApplicableTo(CCSPlayerController* pController)
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

void CZRPlayerClassManager::LoadPlayerClass()
{
	Message("Loading PlayerClass...\n");
	m_ZombieClassMap.Purge();
	m_HumanClassMap.Purge();
	m_vecZombieDefaultClass.Purge();
	m_vecHumanDefaultClass.Purge();

	const char* pszJsonPath = "addons/cs2fixes/configs/zr/playerclass.jsonc";
	char szPath[MAX_PATH];
	V_snprintf(szPath, sizeof(szPath), "%s%s%s", Plat_GetGameDirectory(), "/csgo/", pszJsonPath);
	std::ifstream jsoncFile(szPath);

	if (!jsoncFile.is_open())
	{
		Panic("Failed to open %s. Playerclasses not loaded\n", pszJsonPath);
		return;
	}

	// Less code than constantly traversing the full class vectors, temporary lifetime anyways
	std::set<std::string> setClassNames;
	ordered_json jsonPlayerClasses = ordered_json::parse(jsoncFile, nullptr, false, true);

	if (jsonPlayerClasses.is_discarded())
	{
		Panic("Failed parsing JSON from %s. Playerclasses not loaded\n", pszJsonPath);
		return;
	}

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
				/*if (!jsonClass.contains("knockback"))
				{
					Warning("%s has unspecified key: knockback\n", szClassName.c_str());
					bMissingKey = true;
				}*/
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
				std::shared_ptr<ZRHumanClass> pHumanClass;
				if (!szBase.empty())
				{
					std::shared_ptr<ZRHumanClass> pBaseHumanClass = GetHumanClass(szBase.c_str());
					if (pBaseHumanClass)
					{
						pHumanClass = std::make_shared<ZRHumanClass>(pBaseHumanClass);
						pHumanClass->Override(jsonClass, szClassName);
					}
					else
					{
						Panic("Could not find specified base \"%s\" for %s!!!\n", szBase.c_str(), szClassName.c_str());
						continue;
					}
				}
				else
					pHumanClass = std::make_shared<ZRHumanClass>(jsonClass, szClassName);

				m_HumanClassMap.Insert(hash_32_fnv1a_const(szClassName.c_str()), pHumanClass);

				if (bTeamDefault)
					m_vecHumanDefaultClass.AddToTail(pHumanClass);

				pHumanClass->PrintInfo();
			}
			else
			{
				std::shared_ptr<ZRZombieClass> pZombieClass;
				if (!szBase.empty())
				{
					std::shared_ptr<ZRZombieClass> pBaseZombieClass = GetZombieClass(szBase.c_str());
					if (pBaseZombieClass)
					{
						pZombieClass = std::make_shared<ZRZombieClass>(pBaseZombieClass);
						pZombieClass->Override(jsonClass, szClassName);
					}
					else
					{
						Panic("Could not find specified base \"%s\" for %s!!!\n", szBase.c_str(), szClassName.c_str());
						continue;
					}
				}
				else
					pZombieClass = std::make_shared<ZRZombieClass>(jsonClass, szClassName);

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

void CZRPlayerClassManager::ApplyBaseClass(std::shared_ptr<ZRClass> pClass, CCSPlayerPawn* pPawn)
{
	pPawn->m_iMaxHealth = pClass->iHealth;
	pPawn->m_iHealth = pClass->iHealth;
	pPawn->m_flGravityScale = pClass->flGravity;

	// I don't know why, I don't want to know why,
	// I shouldn't have to wonder why, but for whatever reason
	// this shit caused crashes on ROUND END or MAP CHANGE after the 26/04/2024 update
	// pPawn->m_flVelocityModifier = pClass->flSpeed;
	const auto pController = reinterpret_cast<CCSPlayerController*>(pPawn->GetController());
	if (const auto pPlayer = pController != nullptr ? pController->GetZEPlayer() : nullptr)
		pPlayer->SetMaxSpeed(pClass->flSpeed);

	ApplyBaseClassVisuals(pClass, pPawn);
}

// only changes that should not (directly) affect gameplay
void CZRPlayerClassManager::ApplyBaseClassVisuals(std::shared_ptr<ZRClass> pClass, CCSPlayerPawn* pPawn)
{
	std::shared_ptr<ZRModelEntry> pModelEntry = pClass->GetRandomModelEntry();
	Color clrRender;
	V_StringToColor(pModelEntry->szColor.c_str(), clrRender);

	pPawn->SetModel(pModelEntry->szModelPath.c_str());
	pPawn->m_clrRender = clrRender;
	pPawn->AcceptInput("Skin", pModelEntry->GetRandomSkin());

	const auto pController = reinterpret_cast<CCSPlayerController*>(pPawn->GetController());
	if (const auto pPlayer = pController != nullptr ? pController->GetZEPlayer() : nullptr)
	{
		pPlayer->SetActiveZRClass(pClass);
		pPlayer->SetActiveZRModel(pModelEntry);
	}

	// This has to be done a bit later
	UTIL_AddEntityIOEvent(pPawn, "SetScale", nullptr, nullptr, pClass->flScale);
}

std::shared_ptr<ZRHumanClass> CZRPlayerClassManager::GetHumanClass(const char* pszClassName)
{
	uint16 index = m_HumanClassMap.Find(hash_32_fnv1a_const(pszClassName));
	if (!m_HumanClassMap.IsValidIndex(index))
		return nullptr;
	return m_HumanClassMap[index];
}

void CZRPlayerClassManager::ApplyHumanClass(std::shared_ptr<ZRHumanClass> pClass, CCSPlayerPawn* pPawn)
{
	ApplyBaseClass(pClass, pPawn);
	CCSPlayerController* pController = CCSPlayerController::FromPawn(pPawn);
	if (pController)
		CZRRegenTimer::StopRegen(pController);

	if (!g_cvarEnableLeader.Get() || !pController)
		return;

	ZEPlayer* pPlayer = g_playerManager->GetPlayer(pController->GetPlayerSlot());

	if (pPlayer && pPlayer->IsLeader())
	{
		CHandle<CCSPlayerPawn> hPawn = pPawn->GetHandle();

		new CTimer(0.02f, false, false, [hPawn]() {
			CCSPlayerPawn* pPawn = hPawn.Get();
			if (pPawn)
				Leader_ApplyLeaderVisuals(pPawn);
			return -1.0f;
		});
	}
}

void CZRPlayerClassManager::ApplyPreferredOrDefaultHumanClass(CCSPlayerPawn* pPawn)
{
	CCSPlayerController* pController = CCSPlayerController::FromPawn(pPawn);
	if (!pController) return;

	// Get the human class user preference, or default if no class is set
	int iSlot = pController->GetPlayerSlot();
	std::shared_ptr<ZRHumanClass> humanClass = nullptr;
	const char* sPreferredHumanClass = g_pUserPreferencesSystem->GetPreference(iSlot, HUMAN_CLASS_KEY_NAME);

	// If the preferred human class exists and can be applied, override the default
	uint16 index = m_HumanClassMap.Find(hash_32_fnv1a_const(sPreferredHumanClass));
	if (m_HumanClassMap.IsValidIndex(index) && m_HumanClassMap[index]->IsApplicableTo(pController))
	{
		humanClass = m_HumanClassMap[index];
	}
	else if (m_vecHumanDefaultClass.Count())
	{
		humanClass = m_vecHumanDefaultClass[rand() % m_vecHumanDefaultClass.Count()];
	}
	else if (!humanClass)
	{
		Warning("Missing default human class or valid preferences!\n");
		return;
	}

	ApplyHumanClass(humanClass, pPawn);
}

void CZRPlayerClassManager::ApplyPreferredOrDefaultHumanClassVisuals(CCSPlayerPawn* pPawn)
{
	CCSPlayerController* pController = CCSPlayerController::FromPawn(pPawn);
	if (!pController) return;

	// Get the human class user preference, or default if no class is set
	int iSlot = pController->GetPlayerSlot();
	std::shared_ptr<ZRHumanClass> humanClass = nullptr;
	const char* sPreferredHumanClass = g_pUserPreferencesSystem->GetPreference(iSlot, HUMAN_CLASS_KEY_NAME);

	// If the preferred human class exists and can be applied, override the default
	uint16 index = m_HumanClassMap.Find(hash_32_fnv1a_const(sPreferredHumanClass));
	if (m_HumanClassMap.IsValidIndex(index) && m_HumanClassMap[index]->IsApplicableTo(pController))
	{
		humanClass = m_HumanClassMap[index];
	}
	else if (m_vecHumanDefaultClass.Count())
	{
		humanClass = m_vecHumanDefaultClass[rand() % m_vecHumanDefaultClass.Count()];
	}
	else if (!humanClass)
	{
		Warning("Missing default human class or valid preferences!\n");
		return;
	}

	ApplyBaseClassVisuals(humanClass, pPawn);
}

std::shared_ptr<ZRZombieClass> CZRPlayerClassManager::GetZombieClass(const char* pszClassName)
{
	uint16 index = m_ZombieClassMap.Find(hash_32_fnv1a_const(pszClassName));
	if (!m_ZombieClassMap.IsValidIndex(index))
		return nullptr;
	return m_ZombieClassMap[index];
}

void CZRPlayerClassManager::ApplyZombieClass(std::shared_ptr<ZRZombieClass> pClass, CCSPlayerPawn* pPawn)
{
	ApplyBaseClass(pClass, pPawn);
	CCSPlayerController* pController = CCSPlayerController::FromPawn(pPawn);
	if (pController)
		CZRRegenTimer::StartRegen(pClass->flHealthRegenInterval, pClass->iHealthRegenCount, pController);
}

void CZRPlayerClassManager::ApplyPreferredOrDefaultZombieClass(CCSPlayerPawn* pPawn)
{
	CCSPlayerController* pController = CCSPlayerController::FromPawn(pPawn);
	if (!pController) return;

	// Get the zombie class user preference, or default if no class is set
	int iSlot = pController->GetPlayerSlot();
	std::shared_ptr<ZRZombieClass> zombieClass = nullptr;
	const char* sPreferredZombieClass = g_pUserPreferencesSystem->GetPreference(iSlot, ZOMBIE_CLASS_KEY_NAME);

	// If the preferred zombie class exists and can be applied, override the default
	uint16 index = m_ZombieClassMap.Find(hash_32_fnv1a_const(sPreferredZombieClass));
	if (m_ZombieClassMap.IsValidIndex(index) && m_ZombieClassMap[index]->IsApplicableTo(pController))
	{
		zombieClass = m_ZombieClassMap[index];
	}
	else if (m_vecZombieDefaultClass.Count())
	{
		zombieClass = m_vecZombieDefaultClass[rand() % m_vecZombieDefaultClass.Count()];
	}
	else if (!zombieClass)
	{
		Warning("Missing default zombie class or valid preferences!\n");
		return;
	}

	ApplyZombieClass(zombieClass, pPawn);
}

void CZRPlayerClassManager::GetZRClassList(int iTeam, CUtlVector<std::shared_ptr<ZRClass>>& vecClasses, CCSPlayerController* pController)
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
CZRRegenTimer* CZRRegenTimer::s_vecRegenTimers[MAXPLAYERS];

bool CZRRegenTimer::Execute()
{
	CCSPlayerPawn* pPawn = m_hPawnHandle.Get();
	if (!pPawn || !pPawn->IsAlive())
		return false;

	// Do we even need to regen?
	if (pPawn->m_iHealth() >= pPawn->m_iMaxHealth())
		return true;

	int iHealth = pPawn->m_iHealth() + m_iRegenAmount;
	pPawn->m_iHealth = pPawn->m_iMaxHealth() < iHealth ? pPawn->m_iMaxHealth() : iHealth;
	return true;
}

void CZRRegenTimer::StartRegen(float flRegenInterval, int iRegenAmount, CCSPlayerController* pController)
{
	int slot = pController->GetPlayerSlot();
	CZRRegenTimer* pTimer = s_vecRegenTimers[slot];
	if (pTimer != nullptr)
	{
		pTimer->m_flInterval = flRegenInterval;
		pTimer->m_iRegenAmount = iRegenAmount;
		return;
	}
	s_vecRegenTimers[slot] = new CZRRegenTimer(flRegenInterval, iRegenAmount, pController->m_hPlayerPawn());
}

void CZRRegenTimer::StopRegen(CCSPlayerController* pController)
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
		CZRRegenTimer* pTimer = s_vecRegenTimers[i];
		if (!pTimer)
			continue;

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
			continue;
		delete s_vecRegenTimers[i];
		s_vecRegenTimers[i] = nullptr;
	}
}

void ZR_OnLevelInit()
{
	g_ZRRoundState = EZRRoundState::ROUND_START;

	// Delay one tick to override any .cfg's
	new CTimer(0.02f, false, true, []() {
		// Here we force some cvars that are necessary for the gamemode
		g_pEngineServer2->ServerCommand("mp_give_player_c4 0");
		g_pEngineServer2->ServerCommand("mp_friendlyfire 0");
		g_pEngineServer2->ServerCommand("mp_ignore_round_win_conditions 1");
		// Necessary to fix bots kicked/joining infinitely when forced to CT https://github.com/Source2ZE/ZombieReborn/issues/64
		g_pEngineServer2->ServerCommand("bot_quota_mode fill");
		g_pEngineServer2->ServerCommand("mp_autoteambalance 0");

		return -1.0f;
	});

	g_pZRWeaponConfig->LoadWeaponConfig();
	g_pZRHitgroupConfig->LoadHitgroupConfig();
	SetupCTeams();
}

void ZRWeaponConfig::LoadWeaponConfig()
{
	m_WeaponMap.Purge();
	KeyValues* pKV = new KeyValues("Weapons");
	KeyValues::AutoDelete autoDelete(pKV);

	const char* pszPath = "addons/cs2fixes/configs/zr/weapons.cfg";

	if (!pKV->LoadFromFile(g_pFullFileSystem, pszPath))
	{
		Warning("Failed to load %s\n", pszPath);
		return;
	}
	for (KeyValues* pKey = pKV->GetFirstSubKey(); pKey; pKey = pKey->GetNextKey())
	{
		const char* pszWeaponName = pKey->GetName();
		bool bEnabled = pKey->GetBool("enabled", false);
		float flKnockback = pKey->GetFloat("knockback", 1.0f);
		Message("%s knockback: %f\n", pszWeaponName, flKnockback);
		std::shared_ptr<ZRWeapon> weapon = std::make_shared<ZRWeapon>();
		if (!bEnabled)
			continue;

		weapon->flKnockback = flKnockback;

		m_WeaponMap.Insert(hash_32_fnv1a_const(pszWeaponName), weapon);
	}

	return;
}

std::shared_ptr<ZRWeapon> ZRWeaponConfig::FindWeapon(const char* pszWeaponName)
{
	if (V_strlen(pszWeaponName) > 7 && !V_strncasecmp(pszWeaponName, "weapon_", 7))
		pszWeaponName = pszWeaponName + 7;
	else if (V_strlen(pszWeaponName) > 5 && !V_strncasecmp(pszWeaponName, "item_", 5))
		pszWeaponName = pszWeaponName + 5;

	uint16 index = m_WeaponMap.Find(hash_32_fnv1a_const(pszWeaponName));
	if (m_WeaponMap.IsValidIndex(index))
		return m_WeaponMap[index];

	return nullptr;
}

void ZRHitgroupConfig::LoadHitgroupConfig()
{
	m_HitgroupMap.Purge();
	KeyValues* pKV = new KeyValues("Hitgroups");
	KeyValues::AutoDelete autoDelete(pKV);

	const char* pszPath = "addons/cs2fixes/configs/zr/hitgroups.cfg";

	if (!pKV->LoadFromFile(g_pFullFileSystem, pszPath))
	{
		Warning("Failed to load %s\n", pszPath);
		return;
	}
	for (KeyValues* pKey = pKV->GetFirstSubKey(); pKey; pKey = pKey->GetNextKey())
	{
		const char* pszHitgroupName = pKey->GetName();
		float flKnockback = pKey->GetFloat("knockback", 1.0f);
		int iIndex = -1;

		if (!V_strcasecmp(pszHitgroupName, "Generic"))
			iIndex = 0;
		else if (!V_strcasecmp(pszHitgroupName, "Head"))
			iIndex = 1;
		else if (!V_strcasecmp(pszHitgroupName, "Chest"))
			iIndex = 2;
		else if (!V_strcasecmp(pszHitgroupName, "Stomach"))
			iIndex = 3;
		else if (!V_strcasecmp(pszHitgroupName, "LeftArm"))
			iIndex = 4;
		else if (!V_strcasecmp(pszHitgroupName, "RightArm"))
			iIndex = 5;
		else if (!V_strcasecmp(pszHitgroupName, "LeftLeg"))
			iIndex = 6;
		else if (!V_strcasecmp(pszHitgroupName, "RightLeg"))
			iIndex = 7;
		else if (!V_strcasecmp(pszHitgroupName, "Neck"))
			iIndex = 8;
		else if (!V_strcasecmp(pszHitgroupName, "Gear"))
			iIndex = 10;

		if (iIndex == -1)
		{
			Panic("Failed to load hitgroup %s, invalid name!", pszHitgroupName);
			continue;
		}

		std::shared_ptr<ZRHitgroup> hitGroup = std::make_shared<ZRHitgroup>();

		hitGroup->flKnockback = flKnockback;
		m_HitgroupMap.Insert(iIndex, hitGroup);
		Message("Loaded hitgroup %s at index %d with %f knockback\n", pszHitgroupName, iIndex, hitGroup->flKnockback);
	}

	return;
}

std::shared_ptr<ZRHitgroup> ZRHitgroupConfig::FindHitgroupIndex(int iIndex)
{
	uint16 index = m_HitgroupMap.Find(iIndex);
	// Message("We are finding hitgroup index with index: %d and index is: %d\n", iIndex, index);

	if (m_HitgroupMap.IsValidIndex(index))
	{
		// Message("We found valid index with (m_HitgroupMap[index]): %d\n", m_HitgroupMap[index]);
		return m_HitgroupMap[index];
	}

	return nullptr;
}

void ZR_RespawnAll()
{
	if (!GetGlobals())
		return;

	for (int i = 0; i < GetGlobals()->maxClients; i++)
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
	// Gamerules may not be available earlier, so easiest to just enforce this here
	if (g_pGameRules)
	{
		g_pGameRules->m_iMaxNumCTs = 64;
		g_pGameRules->m_iMaxNumTerrorists = 64;
	}

	g_ZRRoundState = EZRRoundState::ROUND_START;
	ToggleRespawn(true, true);

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
		if (pTeam->m_iTeamNum() == CS_TEAM_CT)
			g_hTeamCT = pTeam->GetHandle();
		else if (pTeam->m_iTeamNum() == CS_TEAM_T)
			g_hTeamT = pTeam->GetHandle();
}

void ZR_OnRoundStart(IGameEvent* pEvent)
{
	ClientPrintAll(HUD_PRINTTALK, ZR_PREFIX "The game is \x05Humans vs. Zombies\x01, the goal for zombies is to infect all humans by knifing them.");
	if (g_cvarInfectSpawnWarning.Get() && g_cvarInfectSpawnType.Get() == (int)EZRSpawnType::IN_PLACE)
		ClientPrintAll(HUD_PRINTTALK, ZR_PREFIX "Classic spawn is enabled! Zombies will be \x07spawning between humans\x01!");
	SetupRespawnToggler();
	CZRRegenTimer::RemoveAllTimers();

	if (!GetGlobals())
		return;

	for (int i = 0; i < GetGlobals()->maxClients; i++)
	{
		CCSPlayerController* pController = CCSPlayerController::FromSlot(i);

		if (!pController)
			continue;

		CCSPlayerPawn* pPawn = pController->GetPlayerPawn();

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
	new CTimer(0.05f, false, false, [handle, bInfect]() {
		CCSPlayerController* pController = (CCSPlayerController*)handle.Get();
		if (!pController)
			return -1.0f;
		if (bInfect)
			ZR_Infect(pController, pController, true);
		else
			ZR_Cure(pController);
		return -1.0f;
	});
}

void ZR_ApplyKnockback(CCSPlayerPawn* pHuman, CCSPlayerPawn* pVictim, int iDamage, const char* szWeapon, int hitgroup, float classknockback)
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
	vecKnockback *= (iDamage * g_cvarKnockbackScale.Get() * flWeaponKnockbackScale * flHitgroupKnockbackScale * classknockback);
	pVictim->m_vecAbsVelocity = pVictim->m_vecAbsVelocity() + vecKnockback;
}

void ZR_ApplyKnockbackExplosion(CBaseEntity* pProjectile, CCSPlayerPawn* pVictim, int iDamage, bool bMolotov)
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

	vecKnockback *= (iDamage * g_cvarKnockbackScale.Get() * flWeaponKnockbackScale);
	pVictim->m_vecAbsVelocity = pVictim->m_vecAbsVelocity() + vecKnockback;
}

void ZR_FakePlayerDeath(CCSPlayerController* pAttackerController, CCSPlayerController* pVictimController, const char* szWeapon, bool bDontBroadcast)
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

void ZR_StripAndGiveKnife(CCSPlayerPawn* pPawn)
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

void ZR_Cure(CCSPlayerController* pTargetController)
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

float ZR_MoanTimer(ZEPlayerHandle hPlayer)
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
		return g_cvarMoanInterval.Get();

	pPawn->EmitSound("zr.amb.zombie_voice_idle");

	return g_cvarMoanInterval.Get();
}

void ZR_InfectShake(CCSPlayerController* pController)
{
	if (!pController || !pController->IsConnected() || pController->IsBot() || !g_cvarInfectShake.Get())
		return;

	INetworkMessageInternal* pNetMsg = g_pNetworkMessages->FindNetworkMessagePartial("Shake");

	auto data = pNetMsg->AllocateMessage()->ToPB<CUserMessageShake>();

	data->set_duration(g_cvarInfectShakeDuration.Get());
	data->set_frequency(g_cvarInfectShakeFrequency.Get());
	data->set_amplitude(g_cvarInfectShakeAmplitude.Get());
	data->set_command(0);

	CSingleRecipientFilter filter(pController->GetPlayerSlot());
	g_gameEventSystem->PostEventAbstract(-1, false, &filter, pNetMsg, data, 0);

	delete data;
}

std::vector<SpawnPoint*> ZR_GetSpawns()
{
	std::vector<SpawnPoint*> spawns;

	if (!g_pGameRules)
		return spawns;

	CUtlVector<SpawnPoint*>* ctSpawns = g_pGameRules->m_CTSpawnPoints();
	CUtlVector<SpawnPoint*>* tSpawns = g_pGameRules->m_TerroristSpawnPoints();

	FOR_EACH_VEC(*ctSpawns, i)
	spawns.push_back((*ctSpawns)[i]);

	FOR_EACH_VEC(*tSpawns, i)
	spawns.push_back((*tSpawns)[i]);

	if (!spawns.size())
		Panic("There are no spawns!\n");

	return spawns;
}

void ZR_Infect(CCSPlayerController* pAttackerController, CCSPlayerController* pVictimController, bool bDontBroadcast)
{
	// This can be null if the victim disconnected right before getting hit AND someone joined in their place immediately, thus replacing the controller
	if (!pVictimController)
		return;

	if (pVictimController->m_iTeamNum() == CS_TEAM_CT)
		pVictimController->SwitchTeam(CS_TEAM_T);

	ZR_CheckTeamWinConditions(CS_TEAM_T);

	ZR_FakePlayerDeath(pAttackerController, pVictimController, "knife", bDontBroadcast); // or any other killicon

	CCSPlayerPawn* pVictimPawn = (CCSPlayerPawn*)pVictimController->GetPawn();
	if (!pVictimPawn)
		return;

	// We disabled damage due to the delayed infection, restore
	pVictimPawn->m_bTakesDamage(true);

	pVictimPawn->EmitSound("zr.amb.scream");

	ZR_StripAndGiveKnife(pVictimPawn);

	g_pZRPlayerClassManager->ApplyPreferredOrDefaultZombieClass(pVictimPawn);

	ZR_InfectShake(pVictimController);

	ZEPlayer* pZEPlayer = pVictimController->GetZEPlayer();

	if (pZEPlayer && !pZEPlayer->IsInfected())
	{
		pZEPlayer->SetInfectState(true);

		ZEPlayerHandle hPlayer = pZEPlayer->GetHandle();
		new CTimer(rand() % (int)g_cvarMoanInterval.Get(), false, false, [hPlayer]() { return ZR_MoanTimer(hPlayer); });
	}
}

void ZR_InfectMotherZombie(CCSPlayerController* pVictimController, std::vector<SpawnPoint*> spawns)
{
	CCSPlayerPawn* pVictimPawn = (CCSPlayerPawn*)pVictimController->GetPawn();
	if (!pVictimPawn)
		return;

	pVictimController->SwitchTeam(CS_TEAM_T);

	ZR_FakePlayerDeath(pVictimController, pVictimController, "knife", true); // not sent to clients

	ZR_StripAndGiveKnife(pVictimPawn);

	// pick random spawn point
	if (g_cvarInfectSpawnType.Get() == (int)EZRSpawnType::RESPAWN)
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

	ZR_InfectShake(pVictimController);

	ZEPlayer* pZEPlayer = pVictimController->GetZEPlayer();

	pZEPlayer->SetInfectState(true);

	ZEPlayerHandle hPlayer = pZEPlayer->GetHandle();
	new CTimer(rand() % (int)g_cvarMoanInterval.Get(), false, false, [hPlayer]() { return ZR_MoanTimer(hPlayer); });
}

// make players who've been picked as MZ recently less likely to be picked again
// store a variable in ZEPlayer, which gets initialized with value 100 if they are picked to be a mother zombie
// the value represents a % chance of the player being skipped next time they are picked to be a mother zombie
// If the player is skipped, next random player is picked to be mother zombie (and same skip chance logic applies to him)
// the variable gets decreased by 20 every round
void ZR_InitialInfection()
{
	if (!GetGlobals())
		return;

	// mz infection candidates
	CUtlVector<CCSPlayerController*> pCandidateControllers;
	for (int i = 0; i < GetGlobals()->maxClients; i++)
	{
		CCSPlayerController* pController = CCSPlayerController::FromSlot(i);
		if (!pController || !pController->IsConnected() || pController->m_iTeamNum() != CS_TEAM_CT)
			continue;

		CCSPlayerPawn* pPawn = (CCSPlayerPawn*)pController->GetPawn();
		if (!pPawn || !pPawn->IsAlive())
			continue;

		pCandidateControllers.AddToTail(pController);
	}

	if (g_cvarInfectSpawnMZRatio.Get() <= 0)
	{
		Warning("Invalid Mother Zombie Ratio!!!");
		return;
	}

	// the num of mz to infect
	int iMZToInfect = pCandidateControllers.Count() / g_cvarInfectSpawnMZRatio.Get();
	iMZToInfect = g_cvarInfectSpawnMinCount.Get() > iMZToInfect ? g_cvarInfectSpawnMinCount.Get() : iMZToInfect;
	bool vecIsMZ[MAXPLAYERS] = {false};

	// get spawn points
	std::vector<SpawnPoint*> spawns = ZR_GetSpawns();
	if (g_cvarInfectSpawnType.Get() == (int)EZRSpawnType::RESPAWN && !spawns.size())
	{
		ClientPrintAll(HUD_PRINTTALK, ZR_PREFIX "There are no spawns!");
		return;
	}

	// infect
	int iFailSafeCounter = 0;
	while (iMZToInfect > 0)
	{
		// If we somehow don't have enough mother zombies after going through the players 5 times,
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
			// roll for immunity
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
	for (int i = 0; i < GetGlobals()->maxClients; i++)
	{
		ZEPlayer* pPlayer = g_playerManager->GetPlayer(i);
		if (!pPlayer || vecIsMZ[i])
			continue;

		pPlayer->SetImmunity(pPlayer->GetImmunity() - g_cvarMZImmunityReduction.Get());
	}

	if (g_cvarRespawnDelay.Get() < 0.0f)
		g_bRespawnEnabled = false;

	SendHudMessageAll(4, EHudPriority::InfectionCountdown, "First infection has started!");
	ClientPrintAll(HUD_PRINTTALK, ZR_PREFIX "First infection has started! Good luck, survivors!");
	g_ZRRoundState = EZRRoundState::POST_INFECTION;
}

void ZR_StartInitialCountdown()
{
	if (g_cvarInfectSpawnTimeMin.Get() > g_cvarInfectSpawnTimeMax.Get())
		g_cvarInfectSpawnTimeMin.Set(g_cvarInfectSpawnTimeMax.Get());

	int iRand = rand();
	auto iSecondsElapsed = std::make_shared<int>(0);
	new CTimer(0.0f, false, false, [iRand, iSecondsElapsed]() {
		if (g_ZRRoundState != EZRRoundState::ROUND_START)
			return -1.0f;

		int g_iInfectionCountDown = g_cvarInfectSpawnTimeMin.Get() + (iRand % (g_cvarInfectSpawnTimeMax.Get() - g_cvarInfectSpawnTimeMin.Get() + 1));
		g_iInfectionCountDown -= *iSecondsElapsed;

		if (g_iInfectionCountDown <= 0)
		{
			ZR_InitialInfection();
			return -1.0f;
		}

		if (g_iInfectionCountDown <= 60)
		{
			char classicSpawnMsg[256];

			if (g_cvarInfectSpawnWarning.Get() && g_cvarInfectSpawnType.Get() == (int)EZRSpawnType::IN_PLACE)
				V_snprintf(classicSpawnMsg, sizeof(classicSpawnMsg), "<span color='#940000'>WARNING: </span><span color='#FF3333'>Zombies will spawn between humans!</span><br>\u00A0<br>");
			else
				V_snprintf(classicSpawnMsg, sizeof(classicSpawnMsg), "");

			SendHudMessageAll(2, EHudPriority::InfectionCountdown, "%sFirst infection in <span color='#00FF00'>%i %s</span>!", classicSpawnMsg, g_iInfectionCountDown, g_iInfectionCountDown == 1 ? "second" : "seconds");

			if (g_iInfectionCountDown % 5 == 0)
				ClientPrintAll(HUD_PRINTTALK, "%sFirst infection in \7%i %s\1!", ZR_PREFIX, g_iInfectionCountDown, g_iInfectionCountDown == 1 ? "second" : "seconds");
		}
		(*iSecondsElapsed)++;

		return 1.0f;
	});
}

bool ZR_Hook_OnTakeDamage_Alive(CTakeDamageInfo* pInfo, CCSPlayerPawn* pVictimPawn)
{
	CCSPlayerPawn* pAttackerPawn = (CCSPlayerPawn*)pInfo->m_hAttacker.Get();

	if (!(pAttackerPawn && pVictimPawn && pAttackerPawn->IsPawn() && pVictimPawn->IsPawn()))
		return false;

	CCSPlayerController* pAttackerController = CCSPlayerController::FromPawn(pAttackerPawn);
	CCSPlayerController* pVictimController = CCSPlayerController::FromPawn(pVictimPawn);
	const char* pszAbilityClass = pInfo->m_hAbility.Get() ? pInfo->m_hAbility.Get()->GetClassname() : "";
	if (pAttackerPawn->m_iTeamNum() == CS_TEAM_T && pVictimPawn->m_iTeamNum() == CS_TEAM_CT && !V_strncmp(pszAbilityClass, "weapon_knife", 12))
	{
		ZR_Infect(pAttackerController, pVictimController, false);
		return true; // nullify the damage
	}

	if (g_cvarGroanChance.Get() && pVictimPawn->m_iTeamNum() == CS_TEAM_T && (rand() % g_cvarGroanChance.Get()) == 1)
		pVictimPawn->EmitSound("zr.amb.zombie_pain");

	// grenade and molotov knockback
	if (pAttackerPawn->m_iTeamNum() == CS_TEAM_CT && pVictimPawn->m_iTeamNum() == CS_TEAM_T)
	{
		CBaseEntity* pInflictor = pInfo->m_hInflictor.Get();
		const char* pszInflictorClass = pInflictor ? pInflictor->GetClassname() : "";
		// inflictor class from grenade damage is actually hegrenade_projectile
		bool bGrenade = V_strncmp(pszInflictorClass, "hegrenade", 9) == 0;
		bool bInferno = V_strncmp(pszInflictorClass, "inferno", 7) == 0;

		if (g_cvarNapalmGrenades.Get() && bGrenade)
		{
			// Scale burn duration by damage, so nades from farther away burn zombies for less time
			float flDuration = (pInfo->m_flDamage / g_cvarNapalmFullDamage.Get()) * g_cvarNapalmDuration.Get();
			flDuration = clamp(flDuration, 0.0f, g_cvarNapalmDuration.Get());

			// Can't use the same inflictor here as it'll end up calling this again each burn damage tick
			// DMG_BURN makes loud noises so use DMG_FALL instead which is completely silent
			IgnitePawn(pVictimPawn, flDuration, pAttackerPawn, pAttackerPawn, nullptr, DMG_FALL);
		}

		if (bGrenade || bInferno)
			ZR_ApplyKnockbackExplosion((CBaseEntity*)pInflictor, (CCSPlayerPawn*)pVictimPawn, (int)pInfo->m_flDamage, bInferno);
	}
	return false;
}

// can prevent purchasing and picking it up
AcquireResult ZR_Detour_CCSPlayer_ItemServices_CanAcquire(CCSPlayer_ItemServices* pItemServices, CEconItemView* pEconItem)
{
	CCSPlayerPawn* pPawn = pItemServices->__m_pChainEntity();

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
		if (!g_pGameRules)
			return;

		g_pGameRules->TerminateRound(1.0f, CSRoundEndReason::GameStart);
		g_ZRRoundState = EZRRoundState::ROUND_END;
		return;
	}

	CHandle<CCSPlayerController> handle = pController->GetHandle();
	new CTimer(2.0f, false, false, [handle]() {
		CCSPlayerController* pController = (CCSPlayerController*)handle.Get();
		if (!pController || !g_bRespawnEnabled || pController->m_iTeamNum < CS_TEAM_T)
			return -1.0f;
		pController->Respawn();
		return -1.0f;
	});
}

void ZR_Hook_ClientPutInServer(CPlayerSlot slot, char const* pszName, int type, uint64 xuid)
{
	CCSPlayerController* pController = CCSPlayerController::FromSlot(slot);
	if (!pController)
		return;

	SpawnPlayer(pController);
}

void ZR_Hook_ClientCommand_JoinTeam(CPlayerSlot slot, const CCommand& args)
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

void ZR_OnPlayerTakeDamage(CCSPlayerPawn* pVictimPawn, const CTakeDamageInfo* pInfo, const int32 damage)
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
		float flCashScale = g_cvarDamageCashScale.Get();

		if(flCashScale > 0)
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

		ZR_ApplyKnockback(pKillerPawn, pVictimPawn, damage, pszWeapon, pInfo->m_pTrace->m_pHitbox->m_nGroupId, flClassKnockback);
	}
}

void ZR_OnPlayerDeath(IGameEvent* pEvent)
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

	ZR_CheckTeamWinConditions(pVictimPawn->m_iTeamNum() == CS_TEAM_T ? CS_TEAM_CT : CS_TEAM_T);

	if (pVictimPawn->m_iTeamNum() == CS_TEAM_T && g_ZRRoundState == EZRRoundState::POST_INFECTION)
		pVictimPawn->EmitSound("zr.amb.zombie_die");

	// respawn player
	CHandle<CCSPlayerController> handle = pVictimController->GetHandle();
	new CTimer(g_cvarRespawnDelay.Get() < 0.0f ? 2.0f : g_cvarRespawnDelay.Get(), false, false, [handle]() {
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
	new CTimer(10.0, false, false, []() {
		if (g_ZRRoundState == EZRRoundState::ROUND_END)
			return -1.0f;
		ZR_EndRoundAndAddTeamScore(g_cvarDefaultWinnerTeam.Get());
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

	if (!GetGlobals() || !g_pGameRules)
		return;

	for (int i = 0; i < GetGlobals()->maxClients; i++)
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

	static ConVarRefAbstract mp_round_restart_delay("mp_round_restart_delay");
	float flRestartDelay = mp_round_restart_delay.GetFloat();

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
		if (g_cvarHumanWinOverlayParticle.Get().Length() != 0)
			ZR_CreateOverlay(g_cvarHumanWinOverlayParticle.Get().String(), 1.0f,
							 g_cvarHumanWinOverlaySize.Get(), flRestartDelay,
							 Color(255, 255, 255), g_cvarHumanWinOverlayMaterial.Get().String());
	}
	else if (iTeamNum == CS_TEAM_T)
	{
		if (!g_hTeamT.Get())
		{
			Panic("Cannot find CTeam for T!\n");
			return;
		}
		g_hTeamT->m_iScore = g_hTeamT->m_iScore() + 1;
		if (g_cvarZombieWinOverlayParticle.Get().Length() != 0)
			ZR_CreateOverlay(g_cvarZombieWinOverlayParticle.Get().String(), 1.0f,
							 g_cvarZombieWinOverlaySize.Get(), flRestartDelay,
							 Color(255, 255, 255), g_cvarZombieWinOverlayMaterial.Get().String());
	}
}

CON_COMMAND_CHAT(ztele, "- Teleport to spawn")
{
	// Silently return so the command is completely hidden
	if (!g_cvarEnableZR.Get())
		return;

	if (!player)
	{
		ClientPrint(player, HUD_PRINTCONSOLE, ZR_PREFIX "You cannot use this command from the server console.");
		return;
	}

	// Check if command is enabled for humans
	if (!g_cvarZteleHuman.Get() && player->m_iTeamNum() == CS_TEAM_CT)
	{
		ClientPrint(player, HUD_PRINTTALK, ZR_PREFIX "You cannot use this command as a human.");
		return;
	}

	std::vector<SpawnPoint*> spawns = ZR_GetSpawns();
	if (!spawns.size())
	{
		ClientPrint(player, HUD_PRINTTALK, ZR_PREFIX "There are no spawns!");
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
		ClientPrint(player, HUD_PRINTTALK, ZR_PREFIX "You cannot teleport when dead!");
		return;
	}

	// Get initial player position so we can do distance check
	Vector initialpos = pPawn->GetAbsOrigin();

	ClientPrint(player, HUD_PRINTTALK, ZR_PREFIX "Teleporting to spawn in 5 seconds.");

	CHandle<CCSPlayerPawn> pawnHandle = pPawn->GetHandle();

	new CTimer(5.0f, false, false, [spawnHandle, pawnHandle, initialpos]() {
		CCSPlayerPawn* pPawn = pawnHandle.Get();
		SpawnPoint* pSpawn = spawnHandle.Get();

		if (!pPawn || !pSpawn)
			return -1.0f;

		Vector endpos = pPawn->GetAbsOrigin();

		if (initialpos.DistTo(endpos) < g_cvarMaxZteleDistance.Get())
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

CON_COMMAND_CHAT(zclass, "<teamname/class name/number> - Find and select your Z:R classes")
{
	// Silently return so the command is completely hidden
	if (!g_cvarEnableZR.Get())
		return;

	if (!player)
	{
		ClientPrint(player, HUD_PRINTCONSOLE, ZR_PREFIX "You cannot use this command from the server console.");
		return;
	}

	CUtlVector<std::shared_ptr<ZRClass>> vecClasses;
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
					ClientPrint(player, HUD_PRINTTALK, "%i. %s", i + 1, vecClasses[i]->szClassName.c_str());
			}
		}

		ClientPrint(player, HUD_PRINTTALK, ZR_PREFIX "Select a class using \x2!zclass <class name/number>");
		return;
	}

	FOR_EACH_VEC(vecClasses, i)
	{
		const char* sClassName = vecClasses[i]->szClassName.c_str();
		bool bClassMatches = !V_stricmp(sClassName, args[1]) || (V_StringToInt32(args[1], -1, NULL, NULL, PARSING_FLAG_SKIP_WARNING) - 1) == i;
		std::shared_ptr<ZRClass> pClass = vecClasses[i];

		if (bClassMatches)
		{
			ClientPrint(player, HUD_PRINTTALK, ZR_PREFIX "Your %s class is now set to \x10%s\x1.", pClass->iTeam == CS_TEAM_CT ? "Human" : "Zombie", sClassName);
			g_pUserPreferencesSystem->SetPreference(iSlot, pClass->iTeam == CS_TEAM_CT ? HUMAN_CLASS_KEY_NAME : ZOMBIE_CLASS_KEY_NAME, sClassName);
			return;
		}
	}

	ClientPrint(player, HUD_PRINTTALK, ZR_PREFIX "No available classes matched \x10%s\x1.", args[1]);
}

CON_COMMAND_CHAT_FLAGS(infect, "- Infect a player", ADMFLAG_GENERIC)
{
	// Silently return so the command is completely hidden
	if (!g_cvarEnableZR.Get())
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

	int iNumClients = 0;
	int pSlots[MAXPLAYERS];
	ETargetType nType;

	if (!g_playerManager->CanTargetPlayers(player, args[1], iNumClients, pSlots, NO_TERRORIST | NO_DEAD, nType))
		return;

	const char* pszCommandPlayerName = player ? player->GetPlayerName() : CONSOLE_NAME;
	std::vector<SpawnPoint*> spawns = ZR_GetSpawns();

	if (g_cvarInfectSpawnType.Get() == (int)EZRSpawnType::RESPAWN && !spawns.size())
	{
		ClientPrint(player, HUD_PRINTTALK, ZR_PREFIX "There are no spawns!");
		return;
	}

	for (int i = 0; i < iNumClients; i++)
	{
		CCSPlayerController* pTarget = CCSPlayerController::FromSlot(pSlots[i]);
		CCSPlayerPawn* pPawn = (CCSPlayerPawn*)pTarget->GetPawn();

		if (g_ZRRoundState == EZRRoundState::ROUND_START)
			ZR_InfectMotherZombie(pTarget, spawns);
		else
			ZR_Infect(pTarget, pTarget, true);

		if (iNumClients == 1)
			PrintSingleAdminAction(pszCommandPlayerName, pTarget->GetPlayerName(), "infected", g_ZRRoundState == EZRRoundState::ROUND_START ? " as a mother zombie" : "", ZR_PREFIX);
	}
	if (iNumClients > 1)
		PrintMultiAdminAction(nType, pszCommandPlayerName, "infected", g_ZRRoundState == EZRRoundState::ROUND_START ? " as mother zombies" : "", ZR_PREFIX);

	// Note we skip MZ immunity when first infection is manually triggered
	if (g_ZRRoundState == EZRRoundState::ROUND_START)
	{
		if (g_cvarRespawnDelay.Get() < 0.0f)
			g_bRespawnEnabled = false;

		g_ZRRoundState = EZRRoundState::POST_INFECTION;
	}
}

CON_COMMAND_CHAT_FLAGS(revive, "- Revive a player", ADMFLAG_GENERIC)
{
	// Silently return so the command is completely hidden
	if (!g_cvarEnableZR.Get())
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

	int iNumClients = 0;
	int pSlots[MAXPLAYERS];
	ETargetType nType;

	if (!g_playerManager->CanTargetPlayers(player, args[1], iNumClients, pSlots, NO_DEAD | NO_COUNTER_TERRORIST, nType))
		return;

	const char* pszCommandPlayerName = player ? player->GetPlayerName() : CONSOLE_NAME;

	for (int i = 0; i < iNumClients; i++)
	{
		CCSPlayerController* pTarget = CCSPlayerController::FromSlot(pSlots[i]);
		CCSPlayerPawn* pPawn = (CCSPlayerPawn*)pTarget->GetPawn();

		if (!pPawn)
			continue;

		ZR_Cure(pTarget);
		ZR_StripAndGiveKnife(pPawn);

		if (iNumClients == 1)
			PrintSingleAdminAction(pszCommandPlayerName, pTarget->GetPlayerName(), "revived", "", ZR_PREFIX);
	}
	if (iNumClients > 1)
		PrintMultiAdminAction(nType, pszCommandPlayerName, "revived", "", ZR_PREFIX);
}