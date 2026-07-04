#pragma once
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

#pragma once

#include "ctimer.h"
#include "detours.h"
#include "vendor/nlohmann/json_fwd.hpp"

using ordered_json = nlohmann::ordered_json;

#define BOSSHUD_PREFIX " \2[BossHud]\1 "
#define BOSSHUD_PREF "bhud"

extern CConVar<bool> g_cvarBossHudEnable;
extern CConVar<CUtlString> g_cvarBossHudHitmarkerParticle;

enum BossHudMode
{
	Display_None, /* Display nothing */
	Display_All,  /* Display hitmarker and hud */
	Display_Hud,  /* Display hud only */
	Display_Hit,  /* Display hitmarker only */
};

enum BossHudType
{
	Invalid,		/* Invalid boss type */
	Breakable,		/* Breakable/physbox bosses */
	BreakableHPBar, /* Breakable + iterator counter + backup breakable bosses*/
	Counter,		/* Math_counter bosses */
	HPBar,			/* Main + iterator + backup counter bosses*/
};

enum BossHudHPBarType
{
	Double, /* Iterator + main only */
	Triple, /* Iterator + backup + main */
};

struct BossDamage
{
	int slot;
	float damage;
};

struct BossConfig
{
	int id; // config file order

	std::string szBossName;
	BossHudType type;

	bool bTriggerUseHammerid;
	std::string szTrigger;
	std::string szOutput;
	float flTriggerDelay;

	bool bShowTriggerUseHammerid;
	std::string szShowTrigger;
	std::string szShowOutput;
	float flShowTriggerDelay;
	bool bShowOnDecrease; /* Only show when hp decreases (only when trigger & showtrigger arent defined) */

	bool bKillTriggerUseHammerid;
	std::string szKillTrigger;
	std::string szKillOutput;
	float flKillTriggerDelay;

	bool bHurtTriggerUseHammerid;
	std::string szHurtTrigger;
	std::string szHurtOutput;

	bool bHitmarkerOnly;
	bool bUseMinorHud;
	bool bMultiTrigger;
	bool bTemplated;
	bool bShowBeaten;
	float flTimeout;
	float flOffset;
	float flOffsetIterator;
	float flMaxAllowedHp;

	std::string szBreakable;
	std::string szCounter;
	bool bCounterReverse;

	BossHudHPBarType hpBarType;
	std::string szIterator;
	bool bIteratorReverse;
	std::string szBackup;

	void SetDefaultValues();
	void SetDefaultTriggers();
	bool IsBreakable() { return type == BossHudType::Breakable; }
	bool IsBreakableHPBar() { return type == BossHudType::BreakableHPBar; }
	bool IsCounter() { return type == BossHudType::Counter; }
	bool IsHPBar() { return type == BossHudType::HPBar; }
	bool IsInvalidType() { return type == BossHudType::Invalid; }

public:
	BossConfig(int bossId);
	BossConfig(ordered_json jsonKeys, int _id);

	bool bTriggered;
	void Trigger(CHandle<CBaseEntity> hTriggerEntity, int templateNum);
};

struct BossInstance : BossConfig, public std::enable_shared_from_this<BossInstance>
{
	bool bCleanup;
	bool bProcessing;	 /* Whether boss is being processed */
	bool bActive;		 /* Whether boss is active */
	bool bShowTriggered; /* Whether showtrigger has been triggered */
	bool bShow;			 /* Whether boss should be displayed */
	bool bKillTriggered; /* Whether killtrigger has been triggered */
	bool bDead;			 /* Whether boss is dead/invalid */

	float flDirectHealth;  /* Actual entity health, used for accurate player damage tracking */
	float flHealth;		   /* Health of boss */
	float flMaxHealth;	   /* Max health of boss (starting health) */
	float flSegmentHealth; /* Max health of a segment (double hpbar setup) */
	int iSegmentsLeft;	   /* Number of segments left (double hpbar setup) */
	float flLastChange;	   /* The game time when boss health changed */

	int iTemplateNum;					/* Template number if namefixup */
	CHandle<CBaseEntity> hBreakableEnt; /* Breakable entity */
	CHandle<CBaseEntity> hCounterEnt;	/* Counter entity */
	CHandle<CBaseEntity> hIteratorEnt;	/* Iterator counter entity */
	CHandle<CBaseEntity> hBackupEnt;	/* Backup counter entity */

	CHandle<CBaseEntity> hTriggerEnt;
	CHandle<CBaseEntity> hShowTriggerEnt;
	CHandle<CBaseEntity> hHurtTriggerEnt;
	CHandle<CBaseEntity> hKillTriggerEnt;

	float flTotalDamage;
	BossDamage PlayerDamage[MAXPLAYERS + 1];

public:
	BossInstance(int bossId) :
		BossConfig(bossId),
		bCleanup(false),
		bProcessing(false),
		bActive(false),
		bShowTriggered(false),
		bShow(false),
		bKillTriggered(false),
		bDead(false),
		flDirectHealth(0.0),
		flHealth(0.0),
		flMaxHealth(0.0),
		flSegmentHealth(0.0),
		iSegmentsLeft(-1),
		flLastChange(0.0),
		iTemplateNum(-1),
		flTotalDamage(0.0)
	{
		for (int i = 0; i <= MAXPLAYERS; i++)
		{
			PlayerDamage[i].slot = i;
			PlayerDamage[i].damage = 0.0;
		}
	};

	bool FindBreakable();
	bool FindCounter();
	bool FindIterator();
	bool FindBackup();
	bool FindShowTrigger();
	bool FindHurtTrigger();
	bool FindKillTrigger();
	bool InitHealth();

	void Trigger();
	bool TriggerPost();
	void ShowTrigger();
	void ShowTriggerPost();
	void Hurt(CBaseEntity* pActivator);
	void KillTrigger();
	void KillTriggerPost();
};

struct SimpleInstance
{
	bool bActive;
	bool bCounter;
	int iEnt;
	float flLastHitTime;
	std::string szTargetname; // Can be hammerID if no targetname
};

class CSimpleHudHandler
{
public:
	void ClearSimpleHud();
	std::vector<std::shared_ptr<SimpleInstance>> vecSimpleInstance;
};

class CBossHudHandler
{
public:
	CBossHudHandler()
	{
		bLoadedConfig = false;
		for (int i = 0; i <= MAXPLAYERS; i++)
			iAssists[i] = 0;
	}

	bool bLoadedConfig;
	bool IsConfigLoaded() { return bLoadedConfig; }

	void UnloadConfig();
	void LoadConfig(const char* sMapName);

	void PrintLoadedConfig(CCSPlayerController* player);

	void ResetTriggeredBosses();
	void ResetScoreboard();
	void ClearBossInstances();
	void CleanupBosses();

	int iAssists[MAXPLAYERS + 1];
	void StartAssistTimer();

	std::string szHudText = "";

	std::vector<std::shared_ptr<BossConfig>> vecBossConfig;		/* Stores all boss configurations in the config */
	std::vector<std::shared_ptr<BossInstance>> vecBossInstance; /* Stores all spawned/triggered boss in the round */
};

extern CBossHudHandler* g_pBossHudHandler;
extern CSimpleHudHandler* g_pSimpleHudHandler;

void BossHud_OnLevelInit(const char* sMapName);
void BossHud_RoundPreStart();
void BossHud_OnEntityDeleted(CEntityInstance* pEntity);
void BossHud_PlayerDisconnect(int slot);
bool BossHud_IsFireOutputHooked();
void BossHud_FireOutput(const CEntityIOOutput* pThis, CEntityInstance* pActivator, CEntityInstance* pCaller, const CVariant* value, float flDelay);
void SimpleHud_FireOutput(CBaseEntity* pEntity, const char* sOutput);
void BossHud_Toggle(CCSPlayerController* player);
float BossHud_UpdateSimpleHud();
float BossHud_UpdateHud();
float BossHud_UpdateAssists();

CBaseEntity* FindEntityByTargetname(CBaseEntity* pStartEnt, std::string szTargetname);
CBaseEntity* FindEntityByHammerid(CBaseEntity* pStartEnt, const char* szHammerID);