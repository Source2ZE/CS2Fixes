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

#pragma once
#include "adminsystem.h"
#include "eventlistener.h"
#include "ctimer.h"
#include "gamesystem.h"
#include "entity/ccsplayercontroller.h"
#include "entity/ccsplayerpawn.h"
#include "vendor/nlohmann/json_fwd.hpp"

using json = nlohmann::json;
using ordered_json = nlohmann::ordered_json;

#define ZR_PREFIX " \4[Zombie:Reborn]\1 "
#define HUMAN_CLASS_KEY_NAME "zr_human_class"
#define ZOMBIE_CLASS_KEY_NAME "zr_zombie_class"

enum class EZRRoundState
{
	ROUND_START,
	POST_INFECTION,
	ROUND_END,
};

enum EZRSpawnType
{
	IN_PLACE,
	RESPAWN,
};

// model entries in zr classes
struct ZRModelEntry
{
	std::string szModelPath;
	CUtlVector<int> vecSkin;
	std::string szColor;
	ZRModelEntry(ZRModelEntry* modelEntry);
	ZRModelEntry(json jModelEntry);
	int GetRandomSkin()
	{
		return vecSkin[rand() % vecSkin.Count()];
	}
};

//everything that human and zombie share
struct ZRClass
{
	bool bEnabled;
	std::string szClassName;
	int iHealth;
	CUtlVector<ZRModelEntry*> vecModels;
	float flScale;
	float flSpeed;
	float flGravity;
	uint64 iAdminFlag;
	ZRClass(ZRClass *pClass) :
		bEnabled(pClass->bEnabled),
		szClassName(pClass->szClassName),
		iHealth(pClass->iHealth),
		flScale(pClass->flScale),
		flSpeed(pClass->flSpeed),
		flGravity(pClass->flGravity),
		iAdminFlag(pClass->iAdminFlag)
		{
			vecModels.Purge();
			FOR_EACH_VEC(pClass->vecModels, i)
			{
				ZRModelEntry *modelEntry = new ZRModelEntry(pClass->vecModels[i]);
				vecModels.AddToTail(modelEntry);
			}
		};

	ZRClass(KeyValues *pKeys) :
		bEnabled(pKeys->GetBool("enabled", false)),
		szClassName(std::string(pKeys->GetName())),
		iHealth(pKeys->GetInt("health", 0)),
		flScale(pKeys->GetFloat("scale", 0)),
		flSpeed(pKeys->GetFloat("speed", 0)),
		flGravity(pKeys->GetFloat("gravity", 0)),
		iAdminFlag(g_pAdminSystem->ParseFlags(
			pKeys->GetString("admin_flag", "")
		)){};
	ZRClass(json jKeys, std::string szClassname);
	void PrintInfo()
	{
		std::string szModels = "";
		FOR_EACH_VEC(vecModels, i)
		{
			szModels += "\n\t\t" + vecModels[i]->szModelPath;
			szModels += ", Color: " + vecModels[i]->szColor;
			szModels += ", Skin: ";
			FOR_EACH_VEC(vecModels[i]->vecSkin, j)
			{
				szModels += std::to_string(vecModels[i]->vecSkin[j]) + ", ";
			}
		}
		Message(
			"%s:\n"
			"\tenabled: %d\n"
			"\thealth: %d\n"
			"\tmodel: %s\n"
			"\tscale: %f\n"
			"\tspeed: %f\n"
			"\tgravity: %f\n"
			"\tadmin flag: %llu\n",
			szClassName.c_str(),
			bEnabled,
			iHealth,
			szModels.c_str(),
			flScale,
			flSpeed,
			flGravity,
			iAdminFlag);
	};
	void Override(json jKeys, std::string szClassname);
	bool IsApplicableTo(CCSPlayerController *pController);
	ZRModelEntry *GetRandomModelEntry()
	{
		return vecModels[rand() % vecModels.Count()];
	};
};


struct ZRHumanClass : ZRClass
{
	ZRHumanClass(ZRHumanClass *pClass) : ZRClass(pClass){};
	ZRHumanClass(json jKeys, std::string szClassname);
};

struct ZRZombieClass : ZRClass
{
	int iHealthRegenCount;
	float flHealthRegenInterval;
	ZRZombieClass(ZRZombieClass *pClass) :
		ZRClass(pClass), 
		iHealthRegenCount(pClass->iHealthRegenCount),
		flHealthRegenInterval(pClass->flHealthRegenInterval){};
	ZRZombieClass(json jKeys, std::string szClassname);
	void PrintInfo()
	{
		std::string szModels = "";
		FOR_EACH_VEC(vecModels, i)
		{
			szModels += "\n\t\t" + vecModels[i]->szModelPath;
			szModels += ", Color: " + vecModels[i]->szColor;
			szModels += ", Skin: ";
			FOR_EACH_VEC(vecModels[i]->vecSkin, j)
			{
				szModels += std::to_string(vecModels[i]->vecSkin[j]) + ", ";
			}
		}
		Message(
			"%s:\n"
			"\tenabled: %d\n"
			"\thealth: %d\n"
			"\tmodel: %s\n"
			"\tscale: %f\n"
			"\tspeed: %f\n"
			"\tgravity: %f\n"
			"\tadmin flag: %d\n"
			"\thealth_regen_count: %d\n"
			"\thealth_regen_interval: %f\n",
			szClassName.c_str(),
			bEnabled,
			iHealth,
			szModels.c_str(),
			flScale,
			flSpeed,
			flGravity,
			iAdminFlag,
			iHealthRegenCount,
			flHealthRegenInterval);
	};
	void Override(json jKeys, std::string szClassname);
};

class CZRPlayerClassManager
{
public:
	CZRPlayerClassManager()
	{
		m_ZombieClassMap.SetLessFunc(DefLessFunc(uint32));
		m_HumanClassMap.SetLessFunc(DefLessFunc(uint32));
	};
	ordered_json KV1PlayerClassConfigToJson(KeyValues* pKV);
	void LoadPlayerClass();
	ZRHumanClass* GetHumanClass(const char *pszClassName);
	void ApplyHumanClass(ZRHumanClass *pClass, CCSPlayerPawn *pPawn);
	void ApplyPreferredOrDefaultHumanClass(CCSPlayerPawn *pPawn);
	ZRZombieClass* GetZombieClass(const char*pszClassName);
	void ApplyZombieClass(ZRZombieClass *pClass, CCSPlayerPawn *pPawn);
	void ApplyPreferredOrDefaultZombieClass(CCSPlayerPawn *pPawn);
	void PrecacheModels(IEntityResourceManifest* pResourceManifest);
	void GetZRClassList(const char* sTeam, CUtlVector<ZRClass*> &vecClasses);
private:
	void ApplyBaseClass(ZRClass* pClass, CCSPlayerPawn *pPawn);
	CUtlVector<ZRZombieClass*> m_vecZombieDefaultClass;
	CUtlVector<ZRHumanClass*> m_vecHumanDefaultClass;
	CUtlMap<uint32, ZRZombieClass*> m_ZombieClassMap;
	CUtlMap<uint32, ZRHumanClass*> m_HumanClassMap;
};

class CZRRegenTimer : public CTimerBase
{
public:
	CZRRegenTimer(float flRegenInterval, int iRegenAmount, CHandle<CCSPlayerPawn> hPawnHandle) :
		CTimerBase(flRegenInterval, false), m_iRegenAmount(iRegenAmount), m_hPawnHandle(hPawnHandle) {};

	bool Execute();
	static void StartRegen(float flRegenInterval, int iRegenAmount, CCSPlayerController *pController);
	static void StopRegen(CCSPlayerController *pController);
	static int GetIndex(CPlayerSlot slot);
	static void Tick();
	static void RemoveAllTimers();

private:
	static double s_flNextExecution;
	static CZRRegenTimer *s_vecRegenTimers[MAXPLAYERS];
	int m_iRegenAmount;
	CHandle<CCSPlayerPawn> m_hPawnHandle;
};

struct ZRWeapon
{
	float flKnockback;
};

class ZRWeaponConfig
{
public:
	ZRWeaponConfig()
	{
		m_WeaponMap.SetLessFunc(DefLessFunc(uint32));
	};
	void LoadWeaponConfig();
	ZRWeapon* FindWeapon(const char *pszWeaponName);
private:
	CUtlMap<uint32, ZRWeapon*> m_WeaponMap;
};

extern ZRWeaponConfig *g_pZRWeaponConfig;
extern CZRPlayerClassManager* g_pZRPlayerClassManager;

extern bool g_bEnableZR;
extern EZRRoundState g_ZRRoundState;

void ZR_OnStartupServer();
void ZR_OnRoundPrestart(IGameEvent* pEvent);
void ZR_OnRoundStart(IGameEvent* pEvent);
void ZR_OnPlayerSpawn(IGameEvent* pEvent);
void ZR_OnPlayerHurt(IGameEvent* pEvent);
void ZR_OnPlayerDeath(IGameEvent* pEvent);
void ZR_OnRoundFreezeEnd(IGameEvent* pEvent);
void ZR_OnRoundTimeWarning(IGameEvent* pEvent);
bool ZR_Detour_TakeDamageOld(CCSPlayerPawn *pVictimPawn, CTakeDamageInfo *pInfo);
bool ZR_Detour_CCSPlayer_WeaponServices_CanUse(CCSPlayer_WeaponServices *pWeaponServices, CBasePlayerWeapon* pPlayerWeapon);
void ZR_Detour_CEntityIdentity_AcceptInput(CEntityIdentity* pThis, CUtlSymbolLarge* pInputName, CEntityInstance* pActivator, CEntityInstance* pCaller, variant_t* value, int nOutputID);
void ZR_Hook_ClientPutInServer(CPlayerSlot slot, char const *pszName, int type, uint64 xuid);
void ZR_Hook_ClientCommand_JoinTeam(CPlayerSlot slot, const CCommand &args);
void ZR_Precache(IEntityResourceManifest* pResourceManifest);