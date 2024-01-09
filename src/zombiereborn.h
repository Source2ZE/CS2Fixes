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

#include "eventlistener.h"
#include "ctimer.h"
#include "gamesystem.h"
#include "entity/ccsplayercontroller.h"
#include "entity/ccsplayerpawn.h"

#define ZR_PREFIX " \4[Zombie:Reborn]\1 "

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

//everything that human and zombie share
struct ZRClass
{
	std::string szClassName;
	int iHealth;
	std::string szModelPath;
	int iSkin;
	std::string szColor;
	float flScale;
	float flSpeed;
	float flGravity;
	ZRClass(ZRClass *pClass) :
		szClassName(pClass->szClassName),
		iHealth(pClass->iHealth),
		szModelPath(pClass->szModelPath),
		iSkin(pClass->iSkin),
		szColor(pClass->szColor),
		flScale(pClass->flScale),
		flSpeed(pClass->flSpeed),
		flGravity(pClass->flGravity){};

	ZRClass(KeyValues *pKeys) :
		szClassName(std::string(pKeys->GetName())),
		iHealth(pKeys->GetInt("health", 0)),
		szModelPath(std::string(pKeys->GetString("model", ""))),
		iSkin(pKeys->GetInt("skin", 0)),
		szColor(std::string(pKeys->GetString("color", ""))),
		flScale(pKeys->GetFloat("scale", 0)),
		flSpeed(pKeys->GetFloat("speed", 0)),
		flGravity(pKeys->GetFloat("gravity", 0)){};
	void PrintInfo()
	{
		Message(
			"%s:\n"
			"\thealth: %d\n"
			"\tmodel: %s\n"
			"\tskin: %i\n"
			"\tcolor: %s\n"
			"\tscale: %f\n"
			"\tspeed: %f\n"
			"\tgravity: %f\n",
			szClassName.c_str(),
			iHealth,
			szModelPath.c_str(),
			iSkin,
			szColor.c_str(),
			flScale,
			flSpeed,
			flGravity);
	};
	void Override(KeyValues *pKeys)
	{
		szClassName = std::string(pKeys->GetName());
		if (pKeys->FindKey("health"))
			iHealth = pKeys->GetInt("health", 0);
		if (pKeys->FindKey("model"))
			szModelPath = std::string(pKeys->GetString("model", ""));
		if (pKeys->FindKey("skin"))
			iSkin = pKeys->GetInt("skin", 0);
		if (pKeys->FindKey("color"))
			szColor = std::string(pKeys->GetString("color", ""));
		if (pKeys->FindKey("scale"))
			flScale = pKeys->GetFloat("scale", 0);
		if (pKeys->FindKey("speed"))
			flSpeed = pKeys->GetFloat("speed", 0);
		if (pKeys->FindKey("gravity"))
			flGravity = pKeys->GetFloat("gravity", 0);
	};
};


struct ZRHumanClass : ZRClass
{
	ZRHumanClass(ZRHumanClass *pClass) : ZRClass(pClass){};
	ZRHumanClass(KeyValues *pKeys) : ZRClass(pKeys){};
};

struct ZRZombieClass : ZRClass
{
	int iHealthRegenCount;
	float flHealthRegenInterval;
	ZRZombieClass(ZRZombieClass *pClass) :
		ZRClass(pClass), 
		iHealthRegenCount(pClass->iHealthRegenCount),
		flHealthRegenInterval(pClass->flHealthRegenInterval){};
	ZRZombieClass(KeyValues *pKeys) :
		ZRClass(pKeys),
		iHealthRegenCount(pKeys->GetInt("health_regen_count", 0)),
		flHealthRegenInterval(pKeys->GetFloat("health_regen_interval", 0)){};
	void PrintInfo()
	{
		Message(
			"%s:\n"
			"\thealth: %d\n"
			"\tmodel: %s\n"
			"\tskin: %i\n"
			"\tcolor: %s\n"
			"\tscale: %f\n"
			"\tspeed: %f\n"
			"\tgravity: %f\n"
			"\thealth_regen_count: %d\n"
			"\thealth_regen_interval: %f\n",
			szClassName.c_str(),
			iHealth,
			szModelPath.c_str(),
			iSkin,
			szColor.c_str(),
			flScale,
			flSpeed,
			flGravity,
			iHealthRegenCount,
			flHealthRegenInterval);
	};
	void Override(KeyValues *pKeys)
	{
		ZRClass::Override(pKeys);
		if (pKeys->FindKey("health_regen_count"))
			flSpeed = pKeys->GetInt("health_regen_count", 0);
		if (pKeys->FindKey("health_regen_interval"))
			flGravity = pKeys->GetFloat("health_regen_interval", 0);
	};
};

class CZRPlayerClassManager
{
public:
	CZRPlayerClassManager()
	{
		m_ZombieClassMap.SetLessFunc(DefLessFunc(uint32));
		m_HumanClassMap.SetLessFunc(DefLessFunc(uint32));
	};
	void LoadPlayerClass();
	ZRHumanClass* GetHumanClass(const char *pszClassName);
	void ApplyHumanClass(ZRHumanClass *pClass, CCSPlayerPawn *pPawn);
	void ApplyDefaultHumanClass(CCSPlayerPawn *pPawn);
	ZRZombieClass* GetZombieClass(const char*pszClassName);
	void ApplyZombieClass(ZRZombieClass *pClass, CCSPlayerPawn *pPawn);
	void ApplyDefaultZombieClass(CCSPlayerPawn *pPawn);
	void PrecacheModels(IEntityResourceManifest* pResourceManifest);
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
	static float s_flNextExecution;
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

// need to replace with actual cvar someday
#define CON_ZR_CVAR(name, description, variable_name, variable_type, variable_default)					\
	CON_COMMAND_F(name, description, FCVAR_LINKED_CONCOMMAND | FCVAR_SPONLY)							\
	{																									\
		if (args.ArgC() < 2)																			\
			Msg("%s %i\n", args[0], variable_name);														\
		else																							\
			variable_name = V_StringTo##variable_type(args[1], variable_default);						\
	}
