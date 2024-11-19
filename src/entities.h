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

#pragma once
 // Bandaid needed for #include "string_t.h" to compile...
#ifndef NULL
	#define NULL 0
#endif

#include <string_t.h>
#include <entityhandle.h>
#include <entitysystem.h>
#include <entityinstance.h>

class InputData_t;
class CGamePlayerEquip;
class CBaseEntity;
class CGameUI;
class CPointViewControl;
class CCSPlayerPawn;

namespace CGamePlayerEquipHandler
{
void Use(CGamePlayerEquip* pEntity, InputData_t* pInput);
void TriggerForAllPlayers(CGamePlayerEquip* pEntity, InputData_t* pInput);
bool TriggerForActivatedPlayer(CGamePlayerEquip* pEntity, InputData_t* pInput);
} // namespace CGamePlayerEquipHandler

namespace CGameUIHandler
{
bool OnActivate(CGameUI* pEntity, CBaseEntity* pActivator);
bool OnDeactivate(CGameUI* pEntity, CBaseEntity* pActivator);
void RunThink(int tick);
} // namespace CGameUIHandler

namespace CPointViewControlHandler
{
bool OnEnable(CPointViewControl* pEntity, CBaseEntity* pActivator);
bool OnDisable(CPointViewControl* pEntity, CBaseEntity* pActivator);
bool OnEnableAll(CPointViewControl* pEntity);
bool OnDisableAll(CPointViewControl* pEntity);
bool IsViewControl(CCSPlayerPawn*);
} // namespace CPointViewControlHandler

void EntityHandler_OnGameFramePre(bool simulate, int tick);
void EntityHandler_OnGameFramePost(bool simulate, int tick);
void EntityHandler_OnRoundRestart();
void EntityHandler_OnEntitySpawned(CBaseEntity* pEntity);

struct EntityIOConnectionDesc_t
{
	string_t m_targetDesc;
	string_t m_targetInput;
	string_t m_valueOverride;
	CEntityHandle m_hTarget;
	EntityIOTargetType_t m_nTargetType;
	int32 m_nTimesToFire;
	float m_flDelay;
};

struct EntityIOConnection_t : EntityIOConnectionDesc_t
{
	bool m_bMarkedForRemoval;
	EntityIOConnection_t* m_pNext;
};

struct EntityIOOutputDesc_t
{
	const char* m_pName;
	uint32 m_nFlags;
	uint32 m_nOutputOffset;
};

class CEntityIOOutput
{
public:
	void* vtable;
	EntityIOConnection_t* m_pConnections;
	EntityIOOutputDesc_t* m_pDesc;
};

bool IsButtonWatchEnabled();
void ButtonWatch(const CEntityIOOutput* pThis, CEntityInstance* pActivator, CEntityInstance* pCaller, const CVariant* value, float flDelay);
bool SetupFireOutputInternalDetour();
void FASTCALL Detour_CEntityIOOutput_FireOutputInternal(const CEntityIOOutput* pThis, CEntityInstance* pActivator, CEntityInstance* pCaller, const CVariant* value, float flDelay);