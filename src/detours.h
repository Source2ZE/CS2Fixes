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
#include "cs2_sdk/entityio.h"
#include <utlsymbollarge.h>

class CCheckTransmitInfo;
class IRecipientFilter;
class ISoundEmitterSystemBase;
class CBaseEntity;
class CBaseEntity;
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
class INetworkMessageInternal;
class IEngineServiceMgr;
class CServerSideClient;
class INetChannel;
class CBasePlayerPawn;
class CUserCmd;
class CGamePlayerEquip;
class InputData_t;
class CCSPlayerPawn;
class CCSPlayer_UseServices;
class CTraceFilter;
class Vector;
class QAngle;

bool InitDetours(CGameConfig* gameConfig);
void FlushAllDetours();
bool SetupFireOutputInternalDetour();

// Add callback functions to this map that wish to hook into Detour_CEntityIOOutput_FireOutputInternal
// to make it more modular/cleaner than shoving everything into the detour (buttonwatch, entwatch, etc.)
extern std::map<std::string, std::function<void(const CEntityIOOutput*, CEntityInstance*, CEntityInstance*, const CVariant*, float)>> mapIOFunctions;

void FASTCALL Detour_UTIL_SayTextFilter(IRecipientFilter&, const char*, CCSPlayerController*, uint64);
void FASTCALL Detour_UTIL_SayText2Filter(IRecipientFilter&, CCSPlayerController*, uint64, const char*, const char*, const char*, const char*, const char*);
bool FASTCALL Detour_IsHearingClient(void*, int);
void FASTCALL Detour_TriggerPush_Touch(CTriggerPush* pPush, CBaseEntity* pOther);
void FASTCALL Detour_CBaseEntity_TakeDamageOld(CBaseEntity* pThis, CTakeDamageInfo* inputInfo);
bool FASTCALL Detour_CCSPlayer_WeaponServices_CanUse(CCSPlayer_WeaponServices*, CBasePlayerWeapon*);
bool FASTCALL Detour_CEntityIdentity_AcceptInput(CEntityIdentity* pThis, CUtlSymbolLarge* pInputName, CEntityInstance* pActivator, CEntityInstance* pCaller, variant_t* value, int nOutputID);
void* FASTCALL Detour_CNavMesh_GetNearestNavArea(int64_t unk1, float* unk2, unsigned int* unk3, unsigned int unk4, int64_t unk5, int64_t unk6, float unk7, int64_t unk8);
void FASTCALL Detour_ProcessMovement(CCSPlayer_MovementServices* pThis, void* pMove);
void* FASTCALL Detour_ProcessUsercmds(CCSPlayerController* pController, CUserCmd* cmds, int numcmds, bool paused, float margin);
void FASTCALL Detour_CGamePlayerEquip_InputTriggerForAllPlayers(CGamePlayerEquip*, InputData_t*);
void FASTCALL Detour_CGamePlayerEquip_InputTriggerForActivatedPlayer(CGamePlayerEquip*, InputData_t*);
CServerSideClient* FASTCALL Detour_GetFreeClient(int64_t unk1, const __m128i* unk2, unsigned int unk3, int64_t unk4, char unk5, void* unk6);
float FASTCALL Detour_CCSPlayerPawn_GetMaxSpeed(CCSPlayerPawn*);
int64 FASTCALL Detour_FindUseEntity(CCSPlayer_UseServices* pThis, float);
bool FASTCALL Detour_TraceFunc(int64*, int*, float*, uint64);
bool FASTCALL Detour_TraceShape(int64*, int64, int64, int64, CTraceFilter*, int64);
void FASTCALL Detour_CEntityIOOutput_FireOutputInternal(const CEntityIOOutput* pThis, CEntityInstance* pActivator, CEntityInstance* pCaller, const CVariant* value, float flDelay);
#ifdef PLATFORM_WINDOWS
Vector* FASTCALL Detour_CBasePlayerPawn_GetEyePosition(CBasePlayerPawn*, Vector*);
QAngle* FASTCALL Detour_CBasePlayerPawn_GetEyeAngles(CBasePlayerPawn*, QAngle*);
#else
Vector FASTCALL Detour_CBasePlayerPawn_GetEyePosition(CBasePlayerPawn*);
QAngle FASTCALL Detour_CBasePlayerPawn_GetEyeAngles(CBasePlayerPawn*);
#endif