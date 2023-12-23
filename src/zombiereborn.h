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

struct ZRHumanClass
{
	int iHealth;
	std::string szModelPath;
};

struct ZRZombieClass
{
	int iHealth;
	std::string szModelPath;

	int iHealthRegenCount;
	float flHealthRegenInterval;
};

class CZRPlayerClassManager
{
public:
	CZRPlayerClassManager() {
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
private:
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
	static int s_iRegenTimerCount;
	static int s_vecRegenTimersIndex[MAXPLAYERS];
	static CZRRegenTimer *s_vecRegenTimers[MAXPLAYERS];
	int m_iRegenAmount;
    CHandle<CCSPlayerPawn> m_hPawnHandle;
};

extern CZRPlayerClassManager* g_pPlayerClassManager;

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
void ZR_Detour_CEntityIdentity_AcceptInput(CEntityIdentity* pThis, CUtlSymbolLarge* pInputName, CEntityInstance* pActivator, CEntityInstance* pCaller, variant_string_t* value, int nOutputID);
void ZR_Hook_ClientPutInServer(CPlayerSlot slot, char const *pszName, int type, uint64 xuid);

// need to replace with actual cvar someday
#define CON_ZR_CVAR(name, description, variable_name, variable_type, variable_default)					\
	CON_COMMAND_F(name, description, FCVAR_LINKED_CONCOMMAND | FCVAR_SPONLY)							\
	{																									\
		if (args.ArgC() < 2)																			\
			Msg("%s %i\n", args[0], variable_name);														\
		else																							\
			variable_name = V_StringTo##variable_type(args[1], variable_default);						\
	}
