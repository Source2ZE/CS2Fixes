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
#include "cdetour.h"
#include <utlsymbollarge.h>


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
class CCSPlayer_WeaponServices;
class CCSPlayer_MovementServices;
class CBasePlayerWeapon;
class INetworkSerializable;
class IEngineServiceMgr;
class CServerSideClient;
class INetChannel;
class CBasePlayerPawn;
class CUserCmd;
class CGamePlayerEquip;
class InputData_t;

bool InitDetours(CGameConfig *gameConfig);
void FlushAllDetours();

void FASTCALL Detour_UTIL_SayTextFilter(IRecipientFilter &, const char *, CCSPlayerController *, uint64);
void FASTCALL Detour_UTIL_SayText2Filter(IRecipientFilter &, CCSPlayerController *, uint64, const char *, const char *, const char *, const char *, const char *);
bool FASTCALL Detour_IsHearingClient(void*, int);
void FASTCALL Detour_TriggerPush_Touch(CTriggerPush* pPush, Z_CBaseEntity* pOther);
void FASTCALL Detour_CGameRules_Constructor(CGameRules *pThis);
void FASTCALL Detour_CBaseEntity_TakeDamageOld(Z_CBaseEntity *pThis, CTakeDamageInfo *inputInfo);
bool FASTCALL Detour_CCSPlayer_WeaponServices_CanUse(CCSPlayer_WeaponServices *, CBasePlayerWeapon *);
bool FASTCALL Detour_CEntityIdentity_AcceptInput(CEntityIdentity* pThis, CUtlSymbolLarge* pInputName, CEntityInstance* pActivator, CEntityInstance* pCaller, variant_t* value, int nOutputID);
void* FASTCALL Detour_CNavMesh_GetNearestNavArea(int64_t unk1, float* unk2, unsigned int* unk3, unsigned int unk4, int64_t unk5, int64_t unk6, float unk7, int64_t unk8);
void FASTCALL Detour_ProcessMovement(CCSPlayer_MovementServices *pThis, void *pMove);
void *FASTCALL Detour_ProcessUsercmds(CBasePlayerPawn *pawn, CUserCmd *cmds, int numcmds, bool paused, float margin);
void FASTCALL  Detour_CGamePlayerEquip_InputTriggerForAllPlayers(CGamePlayerEquip*, InputData_t*);
void FASTCALL  Detour_CGamePlayerEquip_InputTriggerForActivatedPlayer(CGamePlayerEquip*, InputData_t*);
int64_t* FASTCALL Detour_CCSGameRules_GoToIntermission(int64_t unk1, char unk2);