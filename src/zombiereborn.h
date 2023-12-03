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

//extern bool g_ZR_ZOMBIE_SPAWN_READY;

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

extern bool g_bEnableZR;
extern EZRRoundState g_ZRRoundState;

void ZR_OnStartupServer();
void ZR_OnRoundPrestart(IGameEvent* pEvent);
void ZR_OnRoundStart(IGameEvent* pEvent);
void ZR_OnPlayerSpawn(IGameEvent* pEvent);
void ZR_OnPlayerHurt(IGameEvent* pEvent);
void ZR_OnPlayerDeath(IGameEvent* pEvent);
void ZR_OnRoundFreezeEnd(IGameEvent* pEvent);
bool ZR_Detour_TakeDamageOld(CCSPlayerPawn *pVictimPawn, CTakeDamageInfo *pInfo);

// need to replace with actual cvar someday
#define CON_ZR_CVAR(name, description, variable_name, variable_type, variable_default)					\
	CON_COMMAND_F(name, description, FCVAR_LINKED_CONCOMMAND | FCVAR_SPONLY)							\
	{																									\
		if (args.ArgC() < 2)																			\
			Msg("%s %i\n", args[0], variable_name);														\
		else																							\
			variable_name = V_StringTo##variable_type(args[1], variable_default);						\
	}
