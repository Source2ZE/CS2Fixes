/**
 * =============================================================================
 * CS2Fixes
 * Copyright (C) 2023-2026 Source2ZE
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

#include "bosshud.h"
#include "commands.h"
#include "detours.h"
#include "entity/cbaseentity.h"
#include "entity/cmathcounter.h"
#include "entwatch.h"
#include "hud_manager.h"
#include "playermanager.h"
#include "recipientfilters.h"
#include <fstream>
#undef snprintf
#include "vendor/nlohmann/json.hpp"

extern CGlobalVars* GetGlobals();

CBossHudHandler* g_pBossHudHandler = nullptr;
CSimpleHudHandler* g_pSimpleHudHandler = nullptr;

std::weak_ptr<CTimer> m_pBossHudTimer;
std::weak_ptr<CTimer> m_pAssistUpdateTimer;
#define BHUD_ASSIST_TIMER_RATE 1.0f

std::set<std::string> g_setBossEnts = {
	"math_counter",
	"func_breakable",
	"func_physbox"};

std::set<std::string> g_setBossOutputs = {
	"OutValue",
	"OnHealthChanged"};

CConVar<bool> g_cvarBossHudEnable("bosshud_enable", FCVAR_NONE, "INCOMPATIBLE WITH CS#. Whether to enable BossHud features", false);
CConVar<bool> g_cvarBossHudSimpleHud("bosshud_simplehud", FCVAR_NONE, "Whether a simple BossHud is shown on maps without a valid config", true);
CConVar<bool> g_cvarBossHudScoreboard("bosshud_scoreboard", FCVAR_NONE, "Whether boss damage dealt is displayed as assists on scoreboard", false);
CConVar<bool> g_cvarBossHudTopHits("bosshud_tophits", FCVAR_NONE, "Whether top boss deamage is displayed after boss death", true);
CConVar<int> g_cvarBossHudReward("bosshud_reward", FCVAR_NONE, "Money rewarded to players per boss hit", 0, true, 0, false, 0);
CConVar<bool> g_cvarBossHudHitmarker("bosshud_hitmarker", FCVAR_NONE, "Whether to enable hitmarkers for bosses", true);
CConVar<CUtlString> g_cvarBossHudHitmarkerParticle("bosshud_hitmarker_particle", FCVAR_NONE, "The particle to use for boss hitmarker", "particles/hitmarker/hitmarker_boss.vpcf");
CConVar<float> g_cvarBossHudRate("bosshud_rate", FCVAR_NONE, "How often does BossHUD update", 0.1, true, 0.0, false, 0.0);
CConVar<float> g_cvarBossHudMaxHp("bosshud_maxhp", FCVAR_NONE, "Bosses with more than this HP will not start showing on the HUD (0.0 = no limit)", 500000.0, true, 0.0, false, 0.0);

void CBossHudHandler::UnloadConfig()
{
	if (!bLoadedConfig)
		return;

	if (BossHud_IsFireOutputHooked())
		mapIOFunctions.erase("bosshud");

	vecBossConfig.clear();
	vecBossInstance.clear();
	bLoadedConfig = false;
}

void CBossHudHandler::LoadConfig(const char* sMapName)
{
	UnloadConfig();

	char szPath[MAX_PATH];

	V_snprintf(szPath, sizeof(szPath), "%s%s%s.jsonc", Plat_GetGameDirectory(), "/csgo/addons/cs2fixes/configs/bosshud/", sMapName);
	std::ifstream jsoncFile(szPath);

	if (!jsoncFile.is_open())
		return;

	ordered_json jsonItems = ordered_json::parse(jsoncFile, nullptr, false, true);
	if (jsonItems.is_discarded())
	{
		Panic("[BossHUD] Error parsing json! %s\n", szPath);
		return;
	}

	for (auto& [key, jsonItemData] : jsonItems.items())
	{
		std::shared_ptr<BossConfig> boss = std::make_shared<BossConfig>(jsonItemData, vecBossConfig.size());

		if (boss->type == BossHudType::Invalid)
		{
			Panic("[BossHUD] Boss without a valid type (name: %s)\n", boss->szBossName.c_str());
			continue;
		}

		vecBossConfig.push_back(boss);
	}

	if (vecBossConfig.size() > 0)
	{
		// Hook FireOutput
		if (!SetupFireOutputInternalDetour())
			mapIOFunctions.erase("bosshud");
		else if (!BossHud_IsFireOutputHooked())
			mapIOFunctions["bosshud"] = BossHud_FireOutput;
	}

	// Make sure simple hud and hud timer are stopped
	if (g_pSimpleHudHandler)
		g_pSimpleHudHandler->ClearSimpleHud();

	if (!m_pBossHudTimer.expired())
		m_pBossHudTimer.lock()->Cancel();

	if (!m_pAssistUpdateTimer.expired())
		m_pAssistUpdateTimer.lock()->Cancel();

	bLoadedConfig = true;
}

BossConfig::BossConfig(int bossId)
{
	if (!g_pBossHudHandler || g_pBossHudHandler->vecBossConfig.size() <= bossId || bossId < 0)
		return;

	std::shared_ptr<BossConfig> pBoss = g_pBossHudHandler->vecBossConfig[bossId];

	id = pBoss->id;

	szBossName = pBoss->szBossName;
	type = pBoss->type;

	bTriggerUseHammerid = pBoss->bTriggerUseHammerid;
	szTrigger = pBoss->szTrigger;
	szOutput = pBoss->szOutput;
	flTriggerDelay = pBoss->flTriggerDelay;

	bShowTriggerUseHammerid = pBoss->bShowTriggerUseHammerid;
	szShowTrigger = pBoss->szShowTrigger;
	szShowOutput = pBoss->szShowOutput;
	flShowTriggerDelay = pBoss->flShowTriggerDelay;
	bShowOnDecrease = pBoss->bShowOnDecrease;

	bHurtTriggerUseHammerid = pBoss->bHurtTriggerUseHammerid;
	szHurtTrigger = pBoss->szHurtTrigger;
	szHurtOutput = pBoss->szHurtOutput;

	bKillTriggerUseHammerid = pBoss->bKillTriggerUseHammerid;
	szKillTrigger = pBoss->szKillTrigger;
	szKillOutput = pBoss->szKillOutput;
	flKillTriggerDelay = pBoss->flKillTriggerDelay;

	bHitmarkerOnly = pBoss->bHitmarkerOnly;
	bUseMinorHud = pBoss->bUseMinorHud;
	bMultiTrigger = pBoss->bMultiTrigger;
	bTemplated = pBoss->bTemplated;
	bShowBeaten = pBoss->bShowBeaten;
	flTimeout = pBoss->flTimeout;
	flOffset = pBoss->flOffset;
	flOffsetIterator = pBoss->flOffsetIterator;
	flMaxAllowedHp = pBoss->flMaxAllowedHp;

	szBreakable = pBoss->szBreakable;
	szCounter = pBoss->szCounter;
	bCounterReverse = pBoss->bCounterReverse;
	hpBarType = pBoss->hpBarType;
	szIterator = pBoss->szIterator;
	bIteratorReverse = pBoss->bIteratorReverse;
	szBackup = pBoss->szBackup;
}

void BossConfig::SetDefaultValues()
{
	szBossName = "Boss";
	type = BossHudType::Invalid;

	szTrigger = "";
	szOutput = "";
	flTriggerDelay = 0.0;

	szShowTrigger = "";
	szShowOutput = "";
	flShowTriggerDelay = 0.0;
	bShowOnDecrease = false;

	szKillTrigger = "";
	szKillOutput = "";
	flKillTriggerDelay = 0.0;

	bHitmarkerOnly = false;
	bUseMinorHud = false;
	bMultiTrigger = false;
	bTemplated = false;
	bShowBeaten = true;
	flTimeout = 0.0;
	flOffset = 0.0;
	flOffsetIterator = 0.0;
	flMaxAllowedHp = g_cvarBossHudMaxHp.Get();

	bCounterReverse = false;
	bIteratorReverse = false;

	bTriggered = false;
}

// Sets any unset trigger/killtrigger values to defaults based on type
void BossConfig::SetDefaultTriggers()
{
	// If a trigger or showtrigger is defined, show on that rather than doing weird stuff
	if (szTrigger == "" && szShowTrigger == "")
		bShowOnDecrease = !bHitmarkerOnly;

	// Showtrigger doesnt need a default as if its unset,
	// the boss auto shows when triggered

	if (type == BossHudType::Breakable || type == BossHudType::BreakableHPBar)
	{
		if (szTrigger == "")
		{
			szTrigger = szBreakable;
			szOutput = "OnHealthChanged";
		}

		if (szHurtTrigger == "")
		{
			szHurtTrigger = szBreakable;
			szHurtOutput = "OnHealthChanged";
		}

		if (type == BossHudType::Breakable)
		{
			if (szKillTrigger == "")
			{
				szKillTrigger = szBreakable;
				szKillOutput = "OnBreak";
			}
		}
		else if (szKillTrigger == "") // BreakableHPBar
		{
			szKillTrigger = szIterator;
			if (bIteratorReverse)
				szKillOutput = "OnHitMax";
			else
				szKillOutput = "OnHitMin";
		}
	}
	else if (type != BossHudType::Invalid)
	{
		if (szTrigger == "")
		{
			szTrigger = szCounter;
			szOutput = "OutValue";
		}

		if (szHurtTrigger == "")
		{
			szHurtTrigger = szCounter;
			szHurtOutput = "OutValue";
		}

		if (szKillTrigger == "")
		{
			if (type == BossHudType::Counter)
			{
				szKillTrigger = szCounter;
				if (bCounterReverse)
					szKillOutput = "OnHitMax";
				else
					szKillOutput = "OnHitMin";
			}
			else
			{
				szKillTrigger = szIterator;
				if (bIteratorReverse)
					szKillOutput = "OnHitMax";
				else
					szKillOutput = "OnHitMin";
			}
		}
	}

	if (szShowTrigger == "")
		flShowTriggerDelay = 0.0;
}

BossConfig::BossConfig(ordered_json jsonKeys, int _id)
{
	id = _id;

	SetDefaultValues();

	if (jsonKeys.contains("name"))
		szBossName = jsonKeys["name"].get<std::string>();

	if (jsonKeys.contains("breakable"))
	{
		szBreakable = jsonKeys["breakable"].get<std::string>();
		if (jsonKeys.contains("iterator"))
		{
			type = BossHudType::BreakableHPBar;
			szIterator = jsonKeys["iterator"].get<std::string>();

			hpBarType = BossHudHPBarType::Double;
			// if a map needs triple for this i'll add it
			// until then the only BreakableHPBar i know is ze_mist
			// which only uses double
		}
		else
			type = BossHudType::Breakable;
	}
	else if (jsonKeys.contains("counter"))
	{
		szCounter = jsonKeys["counter"].get<std::string>();
		if (jsonKeys.contains("iterator"))
		{
			type = BossHudType::HPBar;
			szIterator = jsonKeys["iterator"].get<std::string>();

			if (jsonKeys.contains("backup"))
			{
				hpBarType = BossHudHPBarType::Triple;
				szBackup = jsonKeys["backup"].get<std::string>();
			}
			else
				hpBarType = BossHudHPBarType::Double;
		}
		else
			type = BossHudType::Counter;

		if (jsonKeys.contains("reversecounter"))
			bCounterReverse = jsonKeys["reversecounter"].get<bool>();
	}
	else
	{
		type = BossHudType::Invalid;
		return;
	}

	if (jsonKeys.contains("trigger"))
	{
		if (jsonKeys["trigger"].contains("ent"))
			szTrigger = jsonKeys["trigger"]["ent"].get<std::string>();

		if (jsonKeys["trigger"].contains("output"))
			szOutput = jsonKeys["trigger"]["output"].get<std::string>();

		if (jsonKeys["trigger"].contains("delay"))
			flTriggerDelay = jsonKeys["trigger"]["delay"].get<float>();
	}

	if (jsonKeys.contains("showtrigger"))
	{
		if (jsonKeys["showtrigger"].contains("ent"))
			szShowTrigger = jsonKeys["showtrigger"]["ent"].get<std::string>();

		if (jsonKeys["showtrigger"].contains("output"))
			szShowOutput = jsonKeys["showtrigger"]["output"].get<std::string>();

		if (jsonKeys["showtrigger"].contains("delay"))
			flShowTriggerDelay = jsonKeys["showtrigger"]["delay"].get<float>();
	}

	if (jsonKeys.contains("hurttrigger"))
	{
		if (jsonKeys["hurttrigger"].contains("ent"))
			szHurtTrigger = jsonKeys["hurttrigger"]["ent"].get<std::string>();

		if (jsonKeys["hurttrigger"].contains("output"))
			szHurtOutput = jsonKeys["hurttrigger"]["output"].get<std::string>();
	}

	if (jsonKeys.contains("killtrigger"))
	{
		if (jsonKeys["killtrigger"].contains("ent"))
			szKillTrigger = jsonKeys["killtrigger"]["ent"].get<std::string>();

		if (jsonKeys["killtrigger"].contains("output"))
			szKillOutput = jsonKeys["killtrigger"]["output"].get<std::string>();

		if (jsonKeys["killtrigger"].contains("delay"))
			flKillTriggerDelay = jsonKeys["killtrigger"]["delay"].get<float>();
	}

	if (jsonKeys.contains("reverseiterator"))
		bIteratorReverse = jsonKeys["reverseiterator"].get<bool>();

	if (jsonKeys.contains("hitmarkeronly"))
		bHitmarkerOnly = jsonKeys["hitmarkeronly"].get<bool>();

	if (jsonKeys.contains("minorhud"))
		bUseMinorHud = jsonKeys["minorhud"].get<bool>();

	if (jsonKeys.contains("multitrigger"))
		bMultiTrigger = jsonKeys["multitrigger"].get<bool>();

	if (jsonKeys.contains("templated"))
		bTemplated = jsonKeys["templated"].get<bool>();

	if (jsonKeys.contains("showbeaten"))
		bShowBeaten = jsonKeys["showbeaten"].get<bool>();

	if (jsonKeys.contains("timeout"))
		flTimeout = jsonKeys["timeout"].get<float>();

	if (jsonKeys.contains("offset"))
		flOffset = jsonKeys["offset"].get<float>();

	if (jsonKeys.contains("offsetiterator"))
		flOffsetIterator = jsonKeys["offsetiterator"].get<float>();

	if (jsonKeys.contains("maxhp"))
		flMaxAllowedHp = jsonKeys["maxhp"].get<float>();

	SetDefaultTriggers();

	bTriggerUseHammerid = (szTrigger[0] == '#');
	if (bTriggerUseHammerid)
		szTrigger = szTrigger.substr(1);

	bShowTriggerUseHammerid = (szShowTrigger[0] == '#');
	if (bShowTriggerUseHammerid)
		szShowTrigger = szShowTrigger.substr(1);

	bKillTriggerUseHammerid = (szKillTrigger[0] == '#');
	if (bKillTriggerUseHammerid)
		szKillTrigger = szKillTrigger.substr(1);

	bHurtTriggerUseHammerid = (szHurtTrigger[0] == '#');
	if (bHurtTriggerUseHammerid)
		szHurtTrigger = szHurtTrigger.substr(1);
}

void BossConfig::Trigger(CHandle<CBaseEntity> hTriggerEntity, int templateNum = -1)
{
	bTriggered = true;
	std::shared_ptr<BossInstance> pBoss = std::make_shared<BossInstance>(id);
	pBoss->hTriggerEnt = hTriggerEntity;
	pBoss->iTemplateNum = templateNum;

	// Place items in order of the config
	int place = -1;
	for (int i = 0; i < (g_pBossHudHandler->vecBossInstance).size(); i++)
	{
		if (g_pBossHudHandler->vecBossInstance[i]->id >= pBoss->id)
		{
			place = i;
			break;
		}
	}

	if (place == -1) // reached the end, our id still higher
		g_pBossHudHandler->vecBossInstance.push_back(pBoss);
	else
		g_pBossHudHandler->vecBossInstance.insert(g_pBossHudHandler->vecBossInstance.begin() + place, pBoss);

	pBoss->Trigger();
}

bool BossInstance::FindBreakable()
{
	if (szBreakable[0] == '#')
	{
		if (bTriggerUseHammerid && szTrigger == szBreakable.substr(1))
			hBreakableEnt = hTriggerEnt;
		else
		{
			CBaseEntity* pBreakableEnt = nullptr;
			pBreakableEnt = FindEntityByHammerid(pBreakableEnt, szBreakable.substr(1).c_str());
			if (!pBreakableEnt)
				return false;

			hBreakableEnt = pBreakableEnt->GetHandle();
		}
	}
	else if (!bTemplated)
	{
		if (!bTriggerUseHammerid && szTrigger == szBreakable)
			hBreakableEnt = hTriggerEnt;
		else
		{
			CBaseEntity* pBreakableEnt = nullptr;
			pBreakableEnt = UTIL_FindEntityByName(pBreakableEnt, szBreakable.c_str());
			if (!pBreakableEnt)
				return false;

			hBreakableEnt = pBreakableEnt->GetHandle();
		}
	}
	else
	{
		if (!bTriggerUseHammerid && szTrigger == szBreakable)
			hBreakableEnt = hTriggerEnt;
		else
		{
			std::string entName = szBreakable;
			if (iTemplateNum != -1)
				entName += "_" + std::to_string(iTemplateNum);
			else
				entName += "_*";

			CBaseEntity* pBreakableEnt = nullptr;
			while ((pBreakableEnt = UTIL_FindEntityByName(pBreakableEnt, entName.c_str())) != nullptr)
			{
				bool bSkip = false;
				for (int i = 0; i < g_pBossHudHandler->vecBossInstance.size(); i++)
				{
					std::shared_ptr<BossInstance> pBossSearch = g_pBossHudHandler->vecBossInstance[i];
					if (pBossSearch->hBreakableEnt == pBreakableEnt)
					{
						bSkip = true;
						break;
					}
				}

				if (!bSkip)
					break;
			}

			if (!pBreakableEnt)
				return false;

			hBreakableEnt = pBreakableEnt->GetHandle();
		}
	}

	flHealth = hBreakableEnt.Get()->m_iHealth();
	flMaxHealth = flHealth;
	return true;
}

bool BossInstance::FindCounter()
{
	if (szCounter[0] == '#')
	{
		if (bTriggerUseHammerid && szTrigger == szCounter.substr(1))
			hCounterEnt = hTriggerEnt;
		else
		{
			CBaseEntity* pCounterEnt = nullptr;
			pCounterEnt = FindEntityByHammerid(pCounterEnt, szCounter.substr(1).c_str());
			if (!pCounterEnt)
				return false;

			hCounterEnt = pCounterEnt->GetHandle();
		}
	}
	else if (!bTemplated)
	{
		if (!bTriggerUseHammerid && szTrigger == szCounter)
			hCounterEnt = hTriggerEnt;
		else
		{
			CBaseEntity* pCounterEnt = nullptr;
			pCounterEnt = UTIL_FindEntityByName(pCounterEnt, szCounter.c_str());
			if (!pCounterEnt)
				return false;

			hCounterEnt = pCounterEnt->GetHandle();
		}
	}
	else
	{
		if (!bTriggerUseHammerid && szTrigger == szCounter)
			hCounterEnt = hTriggerEnt;
		else
		{
			std::string entName = szCounter;
			if (iTemplateNum != -1)
				entName += "_" + std::to_string(iTemplateNum);
			else
				entName += "_*";

			CBaseEntity* pCounterEnt = nullptr;
			while ((pCounterEnt = UTIL_FindEntityByName(pCounterEnt, entName.c_str())) != nullptr)
			{
				bool bSkip = false;
				for (int i = 0; i < g_pBossHudHandler->vecBossInstance.size(); i++)
				{
					std::shared_ptr<BossInstance> pBossSearch = g_pBossHudHandler->vecBossInstance[i];
					if (pBossSearch->hCounterEnt == pCounterEnt)
					{
						bSkip = true;
						break;
					}
				}

				if (!bSkip)
					break;
			}

			if (!pCounterEnt)
				return false;

			hCounterEnt = pCounterEnt->GetHandle();
		}
	}

	return true;
}

bool BossInstance::FindIterator()
{
	if (szIterator[0] == '#')
	{
		if (bTriggerUseHammerid && szTrigger == szIterator.substr(1))
			hIteratorEnt = hTriggerEnt;
		else
		{
			CBaseEntity* pIteratorEnt = nullptr;
			pIteratorEnt = FindEntityByHammerid(pIteratorEnt, szIterator.substr(1).c_str());
			if (!pIteratorEnt)
				return false;

			hIteratorEnt = pIteratorEnt->GetHandle();
		}
	}
	else if (!bTemplated)
	{
		if (!bTriggerUseHammerid && szTrigger == szIterator)
			hIteratorEnt = hTriggerEnt;
		else
		{
			CBaseEntity* pIteratorEnt = nullptr;
			pIteratorEnt = UTIL_FindEntityByName(pIteratorEnt, szIterator.c_str());
			if (!pIteratorEnt)
				return false;

			hIteratorEnt = pIteratorEnt->GetHandle();
		}
	}
	else
	{
		if (!bTriggerUseHammerid && szTrigger == szIterator)
			hIteratorEnt = hTriggerEnt;
		else
		{
			std::string entName = szIterator;
			if (iTemplateNum != -1)
				entName += "_" + std::to_string(iTemplateNum);
			else
				entName += "_*";

			CBaseEntity* pIteratorEnt = nullptr;
			while ((pIteratorEnt = UTIL_FindEntityByName(pIteratorEnt, entName.c_str())) != nullptr)
			{
				bool bSkip = false;
				for (int i = 0; i < g_pBossHudHandler->vecBossInstance.size(); i++)
				{
					std::shared_ptr<BossInstance> pBossSearch = g_pBossHudHandler->vecBossInstance[i];
					if (pBossSearch->hIteratorEnt == pIteratorEnt)
					{
						bSkip = true;
						break;
					}
				}

				if (!bSkip)
					break;
			}

			if (!pIteratorEnt)
				return false;

			hIteratorEnt = pIteratorEnt->GetHandle();
		}
	}

	return true;
}

bool BossInstance::FindBackup()
{
	if (szBackup[0] == '#')
	{
		if (bTriggerUseHammerid && szTrigger == szBackup.substr(1))
			hBackupEnt = hTriggerEnt;
		else
		{
			CBaseEntity* pBackupEnt = nullptr;
			pBackupEnt = FindEntityByHammerid(pBackupEnt, szBackup.substr(1).c_str());
			if (!pBackupEnt)
				return false;

			hBackupEnt = pBackupEnt->GetHandle();
		}
	}
	else if (!bTemplated)
	{
		if (!bTriggerUseHammerid && szTrigger == szBackup)
			hBackupEnt = hTriggerEnt;
		else
		{
			CBaseEntity* pBackupEnt = nullptr;
			pBackupEnt = UTIL_FindEntityByName(pBackupEnt, szBackup.c_str());
			if (!pBackupEnt)
				return false;

			hBackupEnt = pBackupEnt->GetHandle();
		}
	}
	else
	{
		if (!bTriggerUseHammerid && szTrigger == szBackup)
			hBackupEnt = hTriggerEnt;
		else
		{
			std::string entName = szBackup;
			if (iTemplateNum != -1)
				entName += "_" + std::to_string(iTemplateNum);
			else
				entName += "_*";

			CBaseEntity* pBackupEnt = nullptr;
			while ((pBackupEnt = UTIL_FindEntityByName(pBackupEnt, entName.c_str())) != nullptr)
			{
				bool bSkip = false;
				for (int i = 0; i < g_pBossHudHandler->vecBossInstance.size(); i++)
				{
					std::shared_ptr<BossInstance> pBossSearch = g_pBossHudHandler->vecBossInstance[i];
					if (pBossSearch->hBackupEnt == pBackupEnt)
					{
						bSkip = true;
						break;
					}
				}

				if (!bSkip)
					break;
			}

			if (!pBackupEnt)
				return false;

			hBackupEnt = pBackupEnt->GetHandle();
		}
	}

	return true;
}

bool BossInstance::FindShowTrigger()
{
	if (szShowTrigger == "")
		ShowTrigger();
	else if (bShowTriggerUseHammerid)
	{
		if (bTriggerUseHammerid && szShowTrigger == szTrigger)
			hShowTriggerEnt = hTriggerEnt;
		else
		{
			CBaseEntity* pShowTriggerEnt = nullptr;
			pShowTriggerEnt = FindEntityByHammerid(pShowTriggerEnt, szShowTrigger.c_str());
			if (pShowTriggerEnt)
				hShowTriggerEnt = pShowTriggerEnt->GetHandle();
			else
				return false;
		}
	}
	else if (!bTriggerUseHammerid && szShowTrigger == szTrigger)
		hShowTriggerEnt = hTriggerEnt;
	else
	{
		std::string entName = szShowTrigger;

		if (iTemplateNum != -1)
			entName += "_" + std::to_string(iTemplateNum);

		CBaseEntity* pShowTriggerEnt = nullptr;
		pShowTriggerEnt = UTIL_FindEntityByName(pShowTriggerEnt, entName.c_str());
		if (pShowTriggerEnt)
			hShowTriggerEnt = pShowTriggerEnt->GetHandle();
		else
			return false;
	}

	return true;
}

bool BossInstance::FindHurtTrigger()
{
	if (bHurtTriggerUseHammerid)
	{
		if (bTriggerUseHammerid && szHurtTrigger == szTrigger)
			hHurtTriggerEnt = hTriggerEnt;
		else
		{
			CBaseEntity* pHurtTriggerEnt = nullptr;
			pHurtTriggerEnt = FindEntityByHammerid(pHurtTriggerEnt, szKillTrigger.c_str());
			if (pHurtTriggerEnt)
				hHurtTriggerEnt = pHurtTriggerEnt->GetHandle();
			else
				return false;
		}
	}
	else if (!bTriggerUseHammerid && szHurtTrigger == szTrigger)
		hHurtTriggerEnt = hTriggerEnt;
	else
	{
		std::string entName = szHurtTrigger;
		if (iTemplateNum != -1)
			entName += "_" + std::to_string(iTemplateNum);

		CBaseEntity* pHurtTriggerEnt = nullptr;
		pHurtTriggerEnt = UTIL_FindEntityByName(pHurtTriggerEnt, entName.c_str());
		if (pHurtTriggerEnt)
			hHurtTriggerEnt = pHurtTriggerEnt->GetHandle();
		else
			return false;
	}

	return true;
}

bool BossInstance::FindKillTrigger()
{
	if (bKillTriggerUseHammerid)
	{
		if (bTriggerUseHammerid && szKillTrigger == szTrigger)
			hKillTriggerEnt = hTriggerEnt;
		else
		{
			CBaseEntity* pKillTriggerEnt = nullptr;
			pKillTriggerEnt = FindEntityByHammerid(pKillTriggerEnt, szKillTrigger.c_str());
			if (pKillTriggerEnt)
				hKillTriggerEnt = pKillTriggerEnt->GetHandle();
			else
				return false;
		}
	}
	else if (!bTriggerUseHammerid && szKillTrigger == szTrigger)
		hKillTriggerEnt = hTriggerEnt;
	else
	{
		std::string entName = szKillTrigger;
		if (iTemplateNum != -1)
			entName += "_" + std::to_string(iTemplateNum);

		CBaseEntity* pKillTriggerEnt = nullptr;
		pKillTriggerEnt = UTIL_FindEntityByName(pKillTriggerEnt, entName.c_str());
		if (pKillTriggerEnt)
			hKillTriggerEnt = pKillTriggerEnt->GetHandle();
		else
			return false;
	}

	return true;
}

bool BossInstance::InitHealth()
{
	if (IsBreakable())
	{
		flDirectHealth = hBreakableEnt.Get()->m_iHealth();
		flHealth = hBreakableEnt.Get()->m_iHealth() + flOffset;
	}
	else if (IsBreakableHPBar())
	{
		CBaseEntity* pBreakable = hBreakableEnt.Get();
		CMathCounter* pIterator = (CMathCounter*)(hIteratorEnt.Get());

		flDirectHealth = pBreakable->m_iHealth();
		if (hpBarType == BossHudHPBarType::Double)
		{
			if (!bIteratorReverse)
				iSegmentsLeft = pIterator->GetCounterValue() - pIterator->m_flMin() - 1;
			else
				iSegmentsLeft = pIterator->m_flMax() - pIterator->GetCounterValue() - 1;

			iSegmentsLeft += flOffsetIterator;

			flHealth = (pBreakable->m_iHealth() + flOffset) * iSegmentsLeft;
		}
		// triple not done until a map actually has it
	}
	else if (IsCounter())
	{
		CMathCounter* pCounter = (CMathCounter*)(hCounterEnt.Get());
		if (!bCounterReverse) // start high, die onhitmin
			flHealth = pCounter->GetCounterValue() - pCounter->m_flMin();
		else // start low, die onhitmax
			flHealth = pCounter->m_flMax() - pCounter->GetCounterValue();

		flDirectHealth = flHealth;
		flHealth += flOffset;
	}
	else if (IsHPBar())
	{
		CMathCounter* pCounter = (CMathCounter*)(hCounterEnt.Get());
		if (!bCounterReverse)
			flHealth = pCounter->GetCounterValue() - pCounter->m_flMin();
		else
			flHealth = pCounter->m_flMax() - pCounter->GetCounterValue();

		flDirectHealth = flHealth;
		flHealth += flOffset;

		CMathCounter* pIterator = (CMathCounter*)(hIteratorEnt.Get());
		if (hpBarType == BossHudHPBarType::Double)
		{
			if (!bIteratorReverse)
				iSegmentsLeft = pIterator->GetCounterValue() - pIterator->m_flMin() - 1;
			else
				iSegmentsLeft = pIterator->m_flMax() - pIterator->GetCounterValue() - 1;

			iSegmentsLeft += flOffsetIterator;
			flHealth += (flHealth * iSegmentsLeft);
		}
		else
		{
			CMathCounter* pBackup = (CMathCounter*)(hBackupEnt.Get());
			if (!bIteratorReverse)
				iSegmentsLeft = pIterator->GetCounterValue() - pIterator->m_flMin() - 1;
			else
				iSegmentsLeft = pIterator->m_flMax() - pIterator->GetCounterValue() - 1;

			iSegmentsLeft += flOffsetIterator;
			flHealth += (iSegmentsLeft * pBackup->GetCounterValue());
		}
	}

	flMaxHealth = flHealth;

	if (flMaxAllowedHp == 0.0)
		return true;

	return (flHealth < flMaxAllowedHp);
}

void BossInstance::Trigger()
{
	if (flTriggerDelay > 0.0)
	{
		auto weak_this = weak_from_this();
		CTimer::Create(flTriggerDelay, TIMERFLAG_MAP | TIMERFLAG_ROUND, [weak_this] {
			if (auto boss = weak_this.lock())
			{
				if (!boss->TriggerPost())
				{
					boss->bCleanup = true;
					g_pBossHudHandler->CleanupBosses();
				}
			}
			return -1.0f;
		});
	}
	else if (!TriggerPost())
	{
		bCleanup = true;
		g_pBossHudHandler->CleanupBosses();
	}

	bTriggered = true;
}

bool BossInstance::TriggerPost()
{
	Message("[BHUD] Triggered: %s\n", szBossName.c_str());

	switch (type)
	{
		case BossHudType::Breakable:
			if (!FindBreakable())
				return false;
			break;
		case BossHudType::BreakableHPBar:
			if (!FindBreakable())
				return false;

			if (!FindIterator())
				return false;

			// Again, triple is not implemented until theres
			// an actual need for it
			break;
		default:
			if (!FindCounter())
				return false;

			if (!IsHPBar())
				break;

			if (!FindIterator())
				return false;

			if (hpBarType == BossHudHPBarType::Double)
				break;

			if (!FindBackup())
				return false;

			break;
	}

	if (!bHitmarkerOnly && !FindShowTrigger())
		Message("[BossHud] Failed to find showtrigger entity for: %s\n", szBossName.c_str());

	if (!FindHurtTrigger())
		Message("[BossHud] Failed to find hurttrigger entity for: %s\n", szBossName.c_str());

	if (!FindKillTrigger())
		Message("[BossHud] Failed to find killtrigger entity for: %s\n", szBossName.c_str());

	g_pBossHudHandler->StartAssistTimer();

	bActive = InitHealth();

	return true;
}

void BossInstance::ShowTrigger()
{
	bShowTriggered = true;

	if (bShowOnDecrease)
		return;

	if (flShowTriggerDelay > 0.0)
	{
		auto weak_this = weak_from_this();
		CTimer::Create(flShowTriggerDelay, TIMERFLAG_MAP | TIMERFLAG_ROUND, [weak_this] {
			if (auto boss = weak_this.lock())
			{
				Message("[bhud] Showtrigger timer tick\n");
				boss->ShowTriggerPost();
			}

			return -1.0f;
		});
	}
	else
		ShowTriggerPost();
}

void BossInstance::ShowTriggerPost()
{
	bShow = true;
	if (GetGlobals())
		flLastChange = GetGlobals()->curtime;

	Message("[BHUD] Boss \"%s\" is now being shown!\n", szBossName.c_str());

	if (m_pBossHudTimer.expired())
		m_pBossHudTimer = CTimer::Create(g_cvarBossHudRate.Get(), TIMERFLAG_MAP | TIMERFLAG_ROUND, [] {
			return BossHud_UpdateHud();
		});
}

// Activator can be null
void BossInstance::Hurt(CBaseEntity* pActivator)
{
	if (!bActive)
	{
		bActive = InitHealth();
		// Can't do comparisons for damage if we just initialised so just return
		return;
	}

	if (GetGlobals())
		flLastChange = GetGlobals()->curtime;

	float actualHealth = 0.0; // for accurate player damage
	int segments = 0;
	float newHealth = 0.0; // calculated total health
	CBaseEntity* pBreakable = nullptr;
	CMathCounter* pCounter = nullptr;
	CMathCounter* pIterator = nullptr;
	CMathCounter* pBackup = nullptr;
	switch (type)
	{
		case BossHudType::Breakable:
			pBreakable = hBreakableEnt.Get();
			if (!pBreakable)
				return;

			actualHealth = pBreakable->m_iHealth();
			newHealth = pBreakable->m_iHealth() + flOffset;

			break;
		case BossHudType::BreakableHPBar:
			pBreakable = hBreakableEnt.Get();
			if (!pBreakable)
				return;

			pIterator = (CMathCounter*)(hIteratorEnt.Get());
			if (!pIterator)
				return;

			actualHealth = pBreakable->m_iHealth();

			if (hpBarType == BossHudHPBarType::Double)
			{
				if (!bIteratorReverse)
					segments = pIterator->GetCounterValue() - pIterator->m_flMin() - 1;
				else
					segments = pIterator->m_flMax() - pIterator->GetCounterValue() - 1;

				segments += flOffsetIterator;

				newHealth = (pBreakable->m_iHealth() + flOffset);

				if (flSegmentHealth == -1 || newHealth > flSegmentHealth)
					flSegmentHealth = newHealth;

				newHealth += (flSegmentHealth * iSegmentsLeft);

				if (segments != iSegmentsLeft)
				{
					iSegmentsLeft = segments;
					flSegmentHealth = -1;
				}
			}
			// triple not implemented until a map uses it

			break;
		case BossHudType::Counter:
			pCounter = (CMathCounter*)(hCounterEnt.Get());
			if (!pCounter)
				return;

			if (!bCounterReverse) // start high, die onhitmin
				newHealth = pCounter->GetCounterValue() - pCounter->m_flMin();
			else // start low, die onhitmax
				newHealth = pCounter->m_flMax() - pCounter->GetCounterValue();

			actualHealth = newHealth;

			newHealth += flOffset;
			break;
		case BossHudType::HPBar:
			pCounter = (CMathCounter*)(hCounterEnt.Get());
			if (!pCounter)
				return;

			pIterator = (CMathCounter*)(hIteratorEnt.Get());
			if (!pIterator)
				return;

			if (hpBarType == BossHudHPBarType::Double)
			{
				if (!bIteratorReverse)
					segments = pIterator->GetCounterValue() - pIterator->m_flMin() - 1;
				else
					segments = pIterator->m_flMax() - pIterator->GetCounterValue() - 1;

				segments += flOffsetIterator;

				if (!bCounterReverse)
					newHealth = pCounter->GetCounterValue() - pCounter->m_flMin();
				else
					newHealth = pCounter->m_flMax() - pCounter->GetCounterValue();

				actualHealth = newHealth;

				newHealth += flOffset;

				if (flSegmentHealth == -1 || newHealth > flSegmentHealth)
					flSegmentHealth = newHealth;

				newHealth += flSegmentHealth * iSegmentsLeft;

				if (segments != iSegmentsLeft)
				{
					iSegmentsLeft = segments;
					flSegmentHealth = -1;
				}
			}
			else
			{
				pBackup = (CMathCounter*)(hBackupEnt.Get());
				if (!pBackup)
					return;

				if (!bCounterReverse)
					newHealth = pCounter->GetCounterValue() - pCounter->m_flMin();
				else
					newHealth = pCounter->m_flMax() - pCounter->GetCounterValue();

				actualHealth = newHealth;
				newHealth += flOffset;

				if (!bIteratorReverse)
					segments = pIterator->GetCounterValue() - pIterator->m_flMin() - 1;
				else
					segments = pIterator->m_flMax() - pIterator->GetCounterValue() - 1;

				segments += flOffsetIterator;
				newHealth += (segments * pBackup->GetCounterValue());
			}
			break;
	}

	if (bShowOnDecrease && !bShow)
	{
		if (newHealth < flHealth)
			ShowTriggerPost();
	}

	// Update maxhealth so percent is always accurate
	if (newHealth > flMaxHealth)
		flMaxHealth = newHealth;

	// Only credit player if health decreased
	if (actualHealth < flDirectHealth)
	{
		if (pActivator)
		{
			CCSPlayerPawn* pPawn = (CCSPlayerPawn*)pActivator;
			if (pPawn)
			{
				CCSPlayerController* pController = pPawn->GetOriginalController();
				if (pController)
				{
					if (bShowBeaten)
					{
						float damage = flDirectHealth - actualHealth;
						flTotalDamage += damage;

						int slot = pController->GetPlayerSlot();

						// damage array isnt sorted until its dead so we can do this
						PlayerDamage[slot].damage += damage;

						if (g_cvarBossHudScoreboard.Get())
							g_pBossHudHandler->iAssists[slot] += 1;

						if (g_cvarBossHudReward.Get() > 0)
							pController->m_pInGameMoneyServices->m_iAccount += g_cvarBossHudReward.Get();
					}

					// Disable hitmarkers for now until we make a new particle for it
					/* 
					if (g_cvarBossHudHitmarker.Get())
					{
						ZEPlayer* zpPlayer = pController->GetZEPlayer();
						if (zpPlayer)
						{
							BossHudMode mode = (BossHudMode)(zpPlayer->GetBossHudMode());
							if (mode == BossHudMode::Display_All || mode == BossHudMode::Display_Hit)
							{
								CRecipientFilter filter;
								filter.AddRecipient(zpPlayer->GetPlayerSlot());
								addresses::DispatchParticleEffect(g_cvarBossHudHitmarkerParticle.Get().String(), PATTACH_WORLDORIGIN, pPawn, 0, "", false, -1, &filter, 0);
							}
						}
					}
					*/
				}
			}
		}
	}

	flDirectHealth = actualHealth;
	flHealth = newHealth;
}

void BossInstance::KillTrigger()
{
	bKillTriggered = true;
	if (flKillTriggerDelay > 0.0)
	{
		auto weak_this = weak_from_this();
		CTimer::Create(flKillTriggerDelay, TIMERFLAG_MAP | TIMERFLAG_ROUND, [weak_this] {
			if (auto boss = weak_this.lock())
				boss->KillTriggerPost();

			return -1.0f;
		});
	}
	else
		KillTriggerPost();
}

void BossInstance::KillTriggerPost()
{
	Message("[BHUD] Boss \"%s\" is DEAD!\n", szBossName.c_str());

	// Show top damage i guess
	if (bShowBeaten)
	{
		// Sort by descending damage
		std::sort(PlayerDamage, PlayerDamage + MAXPLAYERS + 1,
				  [](const BossDamage& a, const BossDamage& b) {
					  return (a.damage > b.damage);
				  });

		ClientPrintAll(HUD_PRINTTALK, " \x09%s TOP DAMAGE", szBossName.c_str());
		if (PlayerDamage[0].damage <= 0.0)
			ClientPrintAll(HUD_PRINTTALK, " \x0BNobody? Yikes.");
		else
		{
			const char colorMap[] = {'\x10', '\x08', '\x09'};
			char typeText[8];
			snprintf(typeText, sizeof(typeText), "%s", (IsBreakable() || IsBreakableHPBar()) ? "damage" : "hits");

			for (int i = 0; i < MAXPLAYERS + 1; i++)
			{
				if (PlayerDamage[i].damage <= 0.0)
					break;

				CCSPlayerController* pController = CCSPlayerController::FromSlot(CPlayerSlot(PlayerDamage[i].slot));
				if (!pController)
					continue;

				if (i < 3)
					ClientPrintAll(HUD_PRINTTALK, " %c%d. %s \x01- \x0F%.0f %s \x05(%.1f%%)",
								   colorMap[i],
								   i + 1,
								   pController->GetPlayerName().c_str(),
								   PlayerDamage[i].damage,
								   typeText,
								   (PlayerDamage[i].damage / flTotalDamage) * 100.0);
				else
					ClientPrint(pController, HUD_PRINTTALK, " \x0B%d. %s \x01- \x0F%.0f %s \x05(%.1f%%)",
								i + 1,
								pController->GetPlayerName().c_str(),
								PlayerDamage[i].damage,
								typeText,
								(PlayerDamage[i].damage / flTotalDamage) * 100.0);
			}
		}
	}

	bDead = true;

	auto weak_this = weak_from_this();
	CTimer::Create(3.0f, TIMERFLAG_MAP | TIMERFLAG_ROUND, [weak_this] {
		if (auto boss = weak_this.lock())
		{
			boss->bCleanup = true;
			g_pBossHudHandler->CleanupBosses();
		}

		return -1.0f;
	});
}

void CBossHudHandler::PrintLoadedConfig(CCSPlayerController* player)
{
	if (!bLoadedConfig)
	{
		ClientPrint(player, HUD_PRINTTALK, BOSSHUD_PREFIX "No config loaded.");
		return;
	}

	for (int i = 0; i < vecBossConfig.size(); i++)
	{
		std::shared_ptr<BossConfig> config = vecBossConfig[i];
		ClientPrint(player, HUD_PRINTCONSOLE, BOSSHUD_PREFIX "------------   Boss %02d   ------------", i + 1);
		ClientPrint(player, HUD_PRINTCONSOLE, BOSSHUD_PREFIX "     Name: %s", config->szBossName.c_str());
		switch (config->type)
		{
			case BossHudType::Breakable:
				ClientPrint(player, HUD_PRINTCONSOLE, BOSSHUD_PREFIX "     Type: Breakable");
				ClientPrint(player, HUD_PRINTCONSOLE, BOSSHUD_PREFIX "Breakable: %s", config->szBreakable.c_str());
				break;
			case BossHudType::BreakableHPBar:
				ClientPrint(player, HUD_PRINTCONSOLE, BOSSHUD_PREFIX "     Type: Breakable HPBar");
				ClientPrint(player, HUD_PRINTCONSOLE, BOSSHUD_PREFIX "Breakable: %s", config->szBreakable.c_str());
				ClientPrint(player, HUD_PRINTCONSOLE, BOSSHUD_PREFIX " Iterator: %s", config->szIterator.c_str());
				ClientPrint(player, HUD_PRINTCONSOLE, BOSSHUD_PREFIX "   Backup: %s", config->szBackup.c_str());
				break;
			case BossHudType::Counter:
				ClientPrint(player, HUD_PRINTCONSOLE, BOSSHUD_PREFIX "     Type: Counter");
				ClientPrint(player, HUD_PRINTCONSOLE, BOSSHUD_PREFIX "  Counter: %s", config->szCounter.c_str());
				break;
			case BossHudType::HPBar:
				ClientPrint(player, HUD_PRINTCONSOLE, BOSSHUD_PREFIX "     Type: HPBar");
				ClientPrint(player, HUD_PRINTCONSOLE, BOSSHUD_PREFIX "  Counter: %s", config->szCounter.c_str());
				ClientPrint(player, HUD_PRINTCONSOLE, BOSSHUD_PREFIX " Iterator: %s", config->szIterator.c_str());
				ClientPrint(player, HUD_PRINTCONSOLE, BOSSHUD_PREFIX "   Backup: %s", config->szBackup.c_str());
				break;
		}

		ClientPrint(player, HUD_PRINTCONSOLE, BOSSHUD_PREFIX "------------   Trigger   ------------");

		ClientPrint(player, HUD_PRINTCONSOLE, BOSSHUD_PREFIX "   Ent: %s", config->szTrigger.c_str());
		ClientPrint(player, HUD_PRINTCONSOLE, BOSSHUD_PREFIX "Output: %s", config->szOutput.c_str());
		ClientPrint(player, HUD_PRINTCONSOLE, BOSSHUD_PREFIX " Delay: %.1f", config->flTriggerDelay);

		ClientPrint(player, HUD_PRINTCONSOLE, BOSSHUD_PREFIX "------------ ShowTrigger ------------");

		if (config->szShowTrigger != "")
		{
			ClientPrint(player, HUD_PRINTCONSOLE, BOSSHUD_PREFIX "   Ent: %s", config->szShowTrigger.c_str());
			ClientPrint(player, HUD_PRINTCONSOLE, BOSSHUD_PREFIX "Output: %s", config->szShowOutput.c_str());
			ClientPrint(player, HUD_PRINTCONSOLE, BOSSHUD_PREFIX "Delay: %.1f", config->flShowTriggerDelay);
		}
		else
			ClientPrint(player, HUD_PRINTCONSOLE, BOSSHUD_PREFIX "No ShowTrigger set");

		ClientPrint(player, HUD_PRINTCONSOLE, BOSSHUD_PREFIX "------------ KillTrigger ------------");

		if (config->szKillTrigger != "")
		{
			ClientPrint(player, HUD_PRINTCONSOLE, BOSSHUD_PREFIX "   Ent: %s", config->szKillTrigger.c_str());
			ClientPrint(player, HUD_PRINTCONSOLE, BOSSHUD_PREFIX "Output: %s", config->szKillOutput.c_str());
			ClientPrint(player, HUD_PRINTCONSOLE, BOSSHUD_PREFIX " Delay: %.1f", config->flKillTriggerDelay);
		}
		else
			ClientPrint(player, HUD_PRINTCONSOLE, BOSSHUD_PREFIX "No KillTrigger set");

		ClientPrint(player, HUD_PRINTCONSOLE, BOSSHUD_PREFIX "------------ ----------- ------------");
		ClientPrint(player, HUD_PRINTCONSOLE, BOSSHUD_PREFIX " reversecounter: %s", config->bCounterReverse ? "True" : "False");
		ClientPrint(player, HUD_PRINTCONSOLE, BOSSHUD_PREFIX "reverseiterator: %s", config->bIteratorReverse ? "True" : "False");
		ClientPrint(player, HUD_PRINTCONSOLE, BOSSHUD_PREFIX "  hitmarkeronly: %s", config->bHitmarkerOnly ? "True" : "False");
		ClientPrint(player, HUD_PRINTCONSOLE, BOSSHUD_PREFIX "   multitrigger: %s", config->bMultiTrigger ? "True" : "False");
		ClientPrint(player, HUD_PRINTCONSOLE, BOSSHUD_PREFIX "      templated: %s", config->bTemplated ? "True" : "False");
		ClientPrint(player, HUD_PRINTCONSOLE, BOSSHUD_PREFIX "     showbeaten: %s", config->bShowBeaten ? "True" : "False");
		ClientPrint(player, HUD_PRINTCONSOLE, BOSSHUD_PREFIX "        timeout: %.1f", config->flTimeout);
		ClientPrint(player, HUD_PRINTCONSOLE, BOSSHUD_PREFIX "         offset: %.1f", config->flOffset);
		ClientPrint(player, HUD_PRINTCONSOLE, BOSSHUD_PREFIX "------------ ----------- ------------");
	}
	ClientPrint(player, HUD_PRINTTALK, BOSSHUD_PREFIX "See console for output.");
}

void CBossHudHandler::ResetTriggeredBosses()
{
	for (int i = 0; i < vecBossConfig.size(); i++)
		vecBossConfig[i]->bTriggered = false;
}

void CBossHudHandler::ResetScoreboard()
{
	if (!GetGlobals() || !g_cvarBossHudScoreboard.Get())
		return;

	for (int i = 0; i < GetGlobals()->maxClients; i++)
	{
		iAssists[i] = 0;

		CCSPlayerController* pController = CCSPlayerController::FromSlot(i);
		if (!pController)
			continue;

		pController->m_pActionTrackingServices->m_matchStats().m_iAssists = 0;
	}
}

void CBossHudHandler::ClearBossInstances()
{
	vecBossInstance.clear();
}

void CBossHudHandler::CleanupBosses()
{
	for (auto it = vecBossInstance.begin(); it != vecBossInstance.end();)
	{
		std::shared_ptr<BossInstance> pBoss = *it;
		if (!pBoss || pBoss->type == BossHudType::Invalid || pBoss->bCleanup)
			it = vecBossInstance.erase(it);
		else
			it++;
	}
}

void CBossHudHandler::StartAssistTimer()
{
	if (g_cvarBossHudScoreboard.Get() && m_pAssistUpdateTimer.expired())
		m_pAssistUpdateTimer = CTimer::Create(BHUD_ASSIST_TIMER_RATE, TIMERFLAG_MAP | TIMERFLAG_ROUND, [] {
			return BossHud_UpdateAssists();
		});
}

void BossHud_OnLevelInit(const char* sMapName)
{
	g_pBossHudHandler->LoadConfig(sMapName);

	if (!SetupFireOutputInternalDetour())
		mapIOFunctions.erase("bosshud");
	else if (!BossHud_IsFireOutputHooked())
		mapIOFunctions["bosshud"] = BossHud_FireOutput;
}

void BossHud_RoundPreStart()
{
	if (!g_pBossHudHandler || !g_pSimpleHudHandler)
		return;

	// If map has config, we clean everything up
	if (g_pBossHudHandler->IsConfigLoaded())
	{
		g_pBossHudHandler->ResetTriggeredBosses();
		g_pBossHudHandler->ResetScoreboard();
		g_pBossHudHandler->ClearBossInstances();
	}

	g_pSimpleHudHandler->ClearSimpleHud();
}

void BossHud_OnEntityDeleted(CEntityInstance* pEntity)
{
	if (!pEntity || !g_pBossHudHandler)
		return;

	CBaseEntity* pEnt = (CBaseEntity*)pEntity;
	if (!pEnt)
		return;

	// For now, we only check the main entity if it's killed. I doubt checking triggers makes sense anyways
	// since if the trigger was killed, then trigger/showtrigger wouldnt even fire in the first place
	// Calling KillTriggerPost() is so the HUD updates and it is instantly counted as dead,
	// it also then removes the boss from the bosses vector
	for (auto it = g_pBossHudHandler->vecBossInstance.begin(); it != g_pBossHudHandler->vecBossInstance.end(); it++)
	{
		std::shared_ptr<BossInstance> pInstance = *it;
		if (!pInstance || pInstance->bKillTriggered || pInstance->bDead)
			continue;

		switch (pInstance->type)
		{
			case BossHudType::Breakable:
				if (pInstance->hBreakableEnt == pEnt)
					pInstance->KillTriggerPost();
				break;
			case BossHudType::BreakableHPBar:
				if (pInstance->hBreakableEnt == pEnt || pInstance->hIteratorEnt == pEnt)
					pInstance->KillTriggerPost();
				break;
			case BossHudType::Counter:
				if (pInstance->hCounterEnt == pEnt)
					pInstance->KillTriggerPost();
				break;
			case BossHudType::HPBar:
				if (pInstance->hCounterEnt == pEnt || pInstance->hIteratorEnt == pEnt || pInstance->hBackupEnt == pEnt)
					pInstance->KillTriggerPost();
				break;
		}
	}
}

void BossHud_PlayerDisconnect(int slot)
{
	if (g_pBossHudHandler && g_pBossHudHandler->IsConfigLoaded())
	{
		g_pBossHudHandler->iAssists[slot] = 0;

		for (auto it = g_pBossHudHandler->vecBossInstance.begin(); it != g_pBossHudHandler->vecBossInstance.end(); it++)
		{
			std::shared_ptr<BossInstance> pBoss = *it;
			if (!pBoss->bDead)
				pBoss->PlayerDamage[slot].damage = 0.0;
		}
	}
}

bool BossHud_IsFireOutputHooked()
{
	return std::any_of(mapIOFunctions.begin(), mapIOFunctions.end(), [](const auto& p) { return p.first == "bosshud"; });
}

float BossHud_UpdateSimpleHud()
{
	if (!GetGlobals())
		return g_cvarBossHudRate.Get();

	std::string szHudText = "";

	for (int i = 0; i < (g_pSimpleHudHandler->vecSimpleInstance).size(); i++)
	{
		std::shared_ptr<SimpleInstance> pInstance = g_pSimpleHudHandler->vecSimpleInstance[i];
		if (!pInstance)
			continue;

		if (!pInstance->bActive)
			continue;

		if (pInstance->bCounter)
		{
			CMathCounter* pCounter = (CMathCounter*)g_pEntitySystem->GetEntityInstance((CEntityIndex)pInstance->iEnt);
			if (!pCounter)
				continue;

			if (V_strcmp(pCounter->GetName(), ""))
				szHudText.append(pCounter->GetName());
			else
			{
				szHudText.append("[");
				szHudText.append(pCounter->m_sUniqueHammerID.Get().String());
				szHudText.append("]");
			}

			szHudText.append(": ");
			szHudText.append(std::to_string(static_cast<int>(std::round(pCounter->GetCounterValue()))));
		}
		else
		{
			CBaseEntity* pBreakable = (CBaseEntity*)g_pEntitySystem->GetEntityInstance((CEntityIndex)pInstance->iEnt);
			if (!pBreakable)
				continue;

			if (V_strcmp(pBreakable->GetName(), ""))
				szHudText.append(pBreakable->GetName());
			else
			{
				szHudText.append("[");
				szHudText.append(pBreakable->m_sUniqueHammerID.Get().String());
				szHudText.append("]");
			}

			szHudText.append(": ");
			szHudText.append(std::to_string(pBreakable->m_iHealth()));
		}

		if (GetGlobals()->curtime - pInstance->flLastHitTime > 3.0)
			pInstance->bActive = false;

		if (i < (g_pSimpleHudHandler->vecSimpleInstance.size() - 1))
			szHudText.append("<br>");
	}

	if (szHudText != "")
		SendHudMessageAll(1, EHudPriority::BossHud, szHudText.c_str());

	return g_cvarBossHudRate.Get();
}

float BossHud_UpdateHud()
{
	if (!GetGlobals())
		return g_cvarBossHudRate.Get();

	std::string szHudText = "";

	int showCount = 0;
	int health = 0;
	int percent = 0;
	bool singleDead = false;
	bool singleMinor = false;

	for (int i = 0; i < (g_pBossHudHandler->vecBossInstance).size(); i++)
	{
		std::shared_ptr<BossInstance> pBoss = g_pBossHudHandler->vecBossInstance[i];
		if (!pBoss || !pBoss->bActive)
			continue;

		if (!pBoss->bShow || (pBoss->flTimeout > 0.0 && GetGlobals()->curtime > pBoss->flLastChange + pBoss->flTimeout))
			continue;

		if (showCount == 1)
		{
			// Add first boss in a multiboss style
			// Name is already added
			// Health and percent are still the first boss values
			if (percent <= 33)
				szHudText.append(": <span color='#FF0000'>");
			else if (percent <= 66)
				szHudText.append(": <span color='#FFFF00'>");
			else
				szHudText.append(": <span color='#00FF00'>");

			szHudText.append(std::to_string(health));
			szHudText.append("</span>  (");
			szHudText.append(std::to_string(percent));
			szHudText.append("%%)");
		}

		if (pBoss->bDead)
		{
			if (showCount == 0)
			{
				szHudText.append(pBoss->szBossName);
				singleDead = true;
				singleMinor = pBoss->bUseMinorHud;
			}
			else
			{
				szHudText.append("<br>");
				szHudText.append(pBoss->szBossName);
				szHudText.append(": <span color='#FF0000'>DEAD</span>");
			}

			showCount++;
			continue;
		}

		health = static_cast<int>(pBoss->flHealth);
		percent = std::round(clamp((health / pBoss->flMaxHealth) * 100.0f, 0.0, 100.0));

		if (showCount == 0)
		{
			szHudText.append(pBoss->szBossName);
			singleMinor = pBoss->bUseMinorHud;
		}
		else
		{
			szHudText.append("<br>");
			szHudText.append(pBoss->szBossName);

			if (percent <= 33)
				szHudText.append(": <span color='#FF0000'>");
			else if (percent <= 66)
				szHudText.append(": <span color='#FFFF00'>");
			else
				szHudText.append(": <span color='#00FF00'>");

			szHudText.append(std::to_string(health));
			szHudText.append("</span>  (");
			szHudText.append(std::to_string(percent));
			szHudText.append("%%)");
		}

		showCount++;
	}

	if (showCount == 1)
	{
		if (singleMinor)
		{
			if (singleDead)
				szHudText.append(": <span color='#FF0000'>DEAD</span>");
			else
			{
				if (percent <= 33)
					szHudText.append(": <span color='#FF0000'>");
				else if (percent <= 66)
					szHudText.append(": <span color='#FFFF00'>");
				else
					szHudText.append(": <span color='#00FF00'>");

				szHudText.append(std::to_string(health));
				szHudText.append("</span>  (");
				szHudText.append(std::to_string(percent));
				szHudText.append("%%)");
			}
		}
		else if (singleDead)
		{
			szHudText.append("<br>");
			szHudText.append("<span class='fontSize-l' color='#FF0000'>DEAD</span>");
		}
		else
		{
			// Add boss in solo style
			// Name is already added
			if (percent <= 33)
				szHudText.append("<br><span class='fontSize-l' color='#FF0000'>");
			else if (percent <= 66)
				szHudText.append("<br><span class='fontSize-l' color='#FFFF00'>");
			else
				szHudText.append("<br><span class='fontSize-l' color='#00FF00'>");

			szHudText.append(std::to_string(health));
			szHudText.append("</span>  <span class='fontSize-m'>(");
			szHudText.append(std::to_string(percent));
			szHudText.append("%%)</span>");

			static int barCount = 14;
			int bars = clamp(std::ceil(barCount * (percent / 100.0f)), 0, barCount);
			szHudText.append("<br>");

			if (percent <= 33)
				szHudText.append("<span color='#FF0000'>");
			else if (percent <= 66)
				szHudText.append("<span color='#FFFF00'>");
			else
				szHudText.append("<span color='#00FF00'>");

			for (int i = 0; i < bars; i++)
				szHudText.append("&#9605;");

			szHudText.append("</span><span color='#303133'>");
			for (int i = 0; i < (barCount - bars); i++)
				szHudText.append("&#9605;");

			szHudText.append("</span>");
		}
	}

	if (szHudText != "")
	{
		for (int i = 0; i < GetGlobals()->maxClients; i++)
		{
			ZEPlayer* pPlayer = g_playerManager->GetPlayer(i);
			if (pPlayer)
			{
				BossHudMode mode = (BossHudMode)(pPlayer->GetBossHudMode());
				if (mode == BossHudMode::Display_All || mode == BossHudMode::Display_Hud)
					SendHudMessage(pPlayer, 1, EHudPriority::BossHud, szHudText.c_str());
			}
		}
	}

	return g_cvarBossHudRate.Get();
}

float BossHud_UpdateAssists()
{
	// it'll restart automatically
	if (!GetGlobals() || !g_cvarBossHudScoreboard.Get())
		return -1.0f;

	for (int i = 0; i < GetGlobals()->maxClients; i++)
	{
		CCSPlayerController* pController = CCSPlayerController::FromSlot(i);
		if (!pController)
			continue;

		int assists = pController->m_pActionTrackingServices->m_matchStats().m_iAssists();
		if (assists != g_pBossHudHandler->iAssists[i])
			pController->m_pActionTrackingServices->m_matchStats().m_iAssists = g_pBossHudHandler->iAssists[i];
	}

	return BHUD_ASSIST_TIMER_RATE;
}

//-----------------------------------------------------------------------------
// Purpose: Simple hud
//-----------------------------------------------------------------------------
void CSimpleHudHandler::ClearSimpleHud()
{
	vecSimpleInstance.clear();
}

void BossHud_FireOutput(const CEntityIOOutput* pThis, CEntityInstance* pActivator, CEntityInstance* pCaller, const CVariant* value, float flDelay)
{
	if (!BossHud_IsFireOutputHooked() || !pCaller)
		return;

	CBaseEntity* pEnt = (CBaseEntity*)pCaller;
	if (!pEnt)
		return;

	// Simple HUD, can display health if no config
	if (!g_pBossHudHandler || !g_pBossHudHandler->IsConfigLoaded())
	{
		if (g_cvarBossHudSimpleHud.Get())
			SimpleHud_FireOutput(pEnt, pThis->m_pDesc->m_pName);
		return;
	}

	std::string szTargetname = pEnt->GetName();
	std::string szHammerid = pEnt->m_sUniqueHammerID.Get().String();

	// Check trigger
	for (int i = 0; i < g_pBossHudHandler->vecBossConfig.size(); i++)
	{
		std::shared_ptr<BossConfig> pConfig = g_pBossHudHandler->vecBossConfig[i];
		if (!pConfig || pConfig->szTrigger == "" || (!pConfig->bMultiTrigger && pConfig->bTriggered))
			continue;

		// Check output
		if (V_stricmp(pThis->m_pDesc->m_pName, pConfig->szOutput.c_str()))
			continue;

		// If this entity is already in an instance of this boss, don't trigger another
		if (pConfig->bMultiTrigger)
		{
			bool skip = false;
			for (int j = 0; j < g_pBossHudHandler->vecBossInstance.size(); j++)
			{
				if (g_pBossHudHandler->vecBossInstance[j]->id == pConfig->id)
				{
					if (g_pBossHudHandler->vecBossInstance[j]->hTriggerEnt == pEnt)
					{
						skip = true;
						break;
					}
				}
			}

			if (skip)
				continue;
		}

		if (pConfig->bTriggerUseHammerid)
		{
			if (pConfig->szTrigger == szHammerid)
				pConfig->Trigger(pEnt->GetHandle());
		}
		else
		{
			if (pConfig->szTrigger == szTargetname)
				pConfig->Trigger(pEnt->GetHandle());
			else if (pConfig->bTemplated)
			{
				size_t found = szTargetname.find_last_of('_');
				if (found != std::string::npos)
				{
					if (pConfig->szTrigger == szTargetname.substr(0, found))
						pConfig->Trigger(pEnt->GetHandle(), GetTemplateSuffixNumber(szTargetname.c_str()));
				}
			}
		}
	}

	// Check if this output is a show/hurt/kill trigger for an active boss
	for (auto it = g_pBossHudHandler->vecBossInstance.begin(); it != g_pBossHudHandler->vecBossInstance.end(); it++)
	{
		std::shared_ptr<BossInstance> pBoss = *it;
		if (!pBoss || pBoss->type == BossHudType::Invalid || !pBoss->bTriggered || pBoss->bDead)
			continue;

		// Showtrigger (if not hitmarker only)
		if (!pBoss->bHitmarkerOnly && !pBoss->bShowTriggered)
		{
			if (pBoss->hShowTriggerEnt.Get() && pBoss->hShowTriggerEnt == pEnt && !V_stricmp(pThis->m_pDesc->m_pName, pBoss->szShowOutput.c_str()))
				pBoss->ShowTrigger();
		}

		// Hurt trigger
		if (pBoss->hHurtTriggerEnt.Get() && pBoss->hHurtTriggerEnt == pEnt && !V_stricmp(pThis->m_pDesc->m_pName, pBoss->szHurtOutput.c_str()))
			pBoss->Hurt((CBaseEntity*)pActivator);

		// Killtrigger
		if (!pBoss->bKillTriggered)
		{
			if (pBoss->hKillTriggerEnt.Get() && pBoss->hKillTriggerEnt == pEnt && !V_stricmp(pThis->m_pDesc->m_pName, pBoss->szKillOutput.c_str()))
				pBoss->KillTrigger();
		}
	}
}

void SimpleHud_FireOutput(CBaseEntity* pEntity, const char* sOutput)
{
	if (!g_setBossEnts.contains(pEntity->GetClassname()) || !g_setBossOutputs.contains(sOutput))
		return;

	bool bFound = false;
	for (int i = 0; i < g_pSimpleHudHandler->vecSimpleInstance.size(); i++)
	{
		std::shared_ptr<SimpleInstance> pInstance = g_pSimpleHudHandler->vecSimpleInstance[i];
		if (!pInstance)
			continue;

		if (pEntity->entindex() != pInstance->iEnt)
			continue;

		if (!pInstance->bActive)
			pInstance->bActive = true;

		pInstance->flLastHitTime = GetGlobals()->curtime;
		bFound = true;
		break;
	}

	if (bFound)
		return;

	std::shared_ptr<SimpleInstance> pInstance = std::make_shared<SimpleInstance>();
	pInstance->iEnt = pEntity->entindex();
	pInstance->flLastHitTime = GetGlobals()->curtime;
	pInstance->bActive = true;
	pInstance->bCounter = (!V_stricmp(pEntity->GetClassname(), "math_counter")) ? true : false;
	g_pSimpleHudHandler->vecSimpleInstance.push_back(pInstance);

	if (m_pBossHudTimer.expired())
		m_pBossHudTimer = CTimer::Create(g_cvarBossHudRate.Get(), TIMERFLAG_MAP | TIMERFLAG_ROUND, [] {
			return BossHud_UpdateSimpleHud();
		});
}

CBaseEntity* FindEntityByTargetname(CBaseEntity* pStartEnt, std::string szTargetname)
{
	if (!pStartEnt)
		return nullptr;

	if (szTargetname[0] == '#') // HammerID
	{
		std::string szHammerID = szTargetname.substr(1);
		CBaseEntity* pTarget = pStartEnt;
		while ((pTarget = UTIL_FindEntityByClassname(pTarget, "*")))
			if (!V_strcmp(pTarget->m_sUniqueHammerID().Get(), szHammerID.c_str()))
				return pTarget;
	}
	else // Targetname
	{
		int iWildcard = szTargetname.find('*');
		CBaseEntity* pTarget = pStartEnt;
		while ((pTarget = UTIL_FindEntityByClassname(pTarget, "*")))
			if (strncmp(szTargetname.c_str(), pTarget->GetName(), iWildcard) == 0)
				return pTarget;
	}
	return nullptr;
}

CBaseEntity* FindEntityByHammerid(CBaseEntity* pStartEnt, const char* szHammerID)
{
	CBaseEntity* pTarget = pStartEnt;
	while ((pTarget = UTIL_FindEntityByClassname(pTarget, "*")))
		if (!V_strcmp(pTarget->m_sUniqueHammerID().Get(), szHammerID))
			return pTarget;
	return nullptr;
}

CON_COMMAND_CHAT(bhud_dump, "- Prints the currently loaded BossHUD config to console")
{
	if (!g_cvarBossHudEnable.Get())
		return;

	if (!g_pBossHudHandler)
	{
		ClientPrint(player, HUD_PRINTTALK, BOSSHUD_PREFIX "There has been an error initialising BossHUD.");
		return;
	}

	g_pBossHudHandler->PrintLoadedConfig(player);
}

CON_COMMAND_CHAT_FLAGS(bhud_reload, "- Reloads the current map's BossHUD config", ADMFLAG_CONFIG)
{
	if (!g_cvarBossHudEnable.Get() || !GetGlobals())
		return;

	if (!g_pBossHudHandler)
	{
		ClientPrint(player, HUD_PRINTTALK, BOSSHUD_PREFIX "There has been an error initialising BossHUD.");
		return;
	}

	g_pBossHudHandler->LoadConfig(GetGlobals()->mapname.ToCStr());
	if (!g_pBossHudHandler->IsConfigLoaded())
	{
		ClientPrint(player, HUD_PRINTTALK, BOSSHUD_PREFIX "Error reloading config, check console log for details.");
		return;
	}

	ClientPrint(player, HUD_PRINTTALK, BOSSHUD_PREFIX "Config reloaded successfully.");
}

CON_COMMAND_CHAT(bosshud, "- Toggle BossHUD modes")
{
	BossHud_Toggle(player);
}

CON_COMMAND_CHAT(bhud, "- Toggle BossHUD modes")
{
	BossHud_Toggle(player);
}

void BossHud_Toggle(CCSPlayerController* player)
{
	if (!g_cvarBossHudEnable.Get())
		return;

	if (!player)
	{
		ClientPrint(player, HUD_PRINTTALK, BOSSHUD_PREFIX "Only usable in game.");
		return;
	}

	ZEPlayer* zpPlayer = g_playerManager->GetPlayer(player->GetPlayerSlot());
	if (!zpPlayer)
		return;

	// Toggle modes: Show hud/hitmarker -> Hud only -> Hitmarker only -> None
	BossHudMode mode = (BossHudMode)(zpPlayer->GetBossHudMode());
	switch (mode)
	{
		case BossHudMode::Display_None:
			zpPlayer->SetBossHudMode((int)BossHudMode::Display_All);
			ClientPrint(player, HUD_PRINTTALK, BOSSHUD_PREFIX "Showing both boss HUD and hitmarkers.");
			break;
		case BossHudMode::Display_All:
			zpPlayer->SetBossHudMode((int)BossHudMode::Display_Hud);
			ClientPrint(player, HUD_PRINTTALK, BOSSHUD_PREFIX "Showing boss HUD only.");
			break;
		case BossHudMode::Display_Hud:
			zpPlayer->SetBossHudMode((int)BossHudMode::Display_Hit);
			ClientPrint(player, HUD_PRINTTALK, BOSSHUD_PREFIX "Showing boss hitmarker only.");
			break;
		case BossHudMode::Display_Hit:
			zpPlayer->SetBossHudMode((int)BossHudMode::Display_None);
			ClientPrint(player, HUD_PRINTTALK, BOSSHUD_PREFIX "Disabled both boss HUD and hitmarkers.");
			break;
	}
}