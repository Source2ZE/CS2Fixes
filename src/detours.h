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
#include "cdetour.h"


class CCheckTransmitInfo;
class IRecipientFilter;
class ISoundEmitterSystemBase;
class CBaseEntity;
class Z_CBaseEntity;
class CCSPlayerController;
class CEntityIndex;
class CCommand;
class CTriggerPush;
class CGameConfig;
class CGameRules;
class CTakeDamageInfo;

bool InitDetours(CGameConfig *gameConfig);
void FlushAllDetours();

void FASTCALL Detour_UTIL_SayTextFilter(IRecipientFilter &, const char *, CCSPlayerController *, uint64);
void FASTCALL Detour_UTIL_SayText2Filter(IRecipientFilter &, CCSPlayerController *, uint64, const char *, const char *, const char *, const char *, const char *);
bool FASTCALL Detour_IsHearingClient(void*, int);
void FASTCALL Detour_CSoundEmitterSystem_EmitSound(ISoundEmitterSystemBase *, CEntityIndex *, IRecipientFilter &, uint32, void *);
//void FASTCALL Detour_CBaseEntity_Spawn(CBaseEntity *, void *);
void FASTCALL Detour_CCSWeaponBase_Spawn(CBaseEntity *, void *);
void FASTCALL Detour_TriggerPush_Touch(CTriggerPush* pPush, Z_CBaseEntity* pOther);
void FASTCALL Detour_CGameRules_Constructor(CGameRules *pThis);
void FASTCALL Detour_CBaseEntity_TakeDamageOld(Z_CBaseEntity *pThis, CTakeDamageInfo *inputInfo);
