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
#include "cs2_sdk/entityio.h"
#include "khook.hpp"
#include <utlsymbollarge.h>

class CCheckTransmitInfo;
class IRecipientFilter;
class ISoundEmitterSystemBase;
class CBaseEntity;
class CBaseFilter;
class CCSPlayerController;
class CEntityIndex;
class CCommand;
class CTriggerPush;
class CTriggerGravity;
class CGameConfig;
class CGameRules;
class CTakeDamageInfo;
class CCSPlayer_WeaponServices;
class CCSPlayer_MovementServices;
class CCSPlayer_ItemServices;
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
class CNavMesh;
class CBaseModelEntity;
class Vector;
class QAngle;
class CEconItemView;
class CCSGameRules;
struct CTakeDamageResult;

enum class AcquireMethod
{
	PickUp,
	Buy,
};

enum class AcquireResult
{
	Allowed,
	InvalidItem,
	AlreadyOwned,
	AlreadyPurchased,
	ReachedGrenadeTypeLimit,
	ReachedGrenadeTotalLimit,
	NotAllowedByTeam,
	NotAllowedByMap,
	NotAllowedByMode,
	NotAllowedForPurchase,
	NotAllowedByProhibition,
};

void InitDetours(CGameConfig* gameConfig);

KHook::Return<int64> Detour_CBaseEntity_TakeDamageOld(CBaseEntity* pThis, CTakeDamageInfo* pInfo, CTakeDamageResult* pResult);
KHook::Return<int64> Detour_CBaseEntity_TakeDamageOld_Post(CBaseEntity* pThis, CTakeDamageInfo* pInfo, CTakeDamageResult* pResult);
KHook::Return<void> Detour_TriggerPush_Touch(CTriggerPush* pPush, CBaseEntity* pOther);
KHook::Return<bool> Detour_IsHearingClient(void*, int);
KHook::Return<void> Detour_UTIL_SayTextFilter(IRecipientFilter&, const char*, CCSPlayerController*, uint64);
KHook::Return<void> Detour_UTIL_SayText2Filter(IRecipientFilter&, CCSPlayerController*, uint64, const char*, const char*, const char*, const char*, const char*);
KHook::Return<bool> Detour_CCSPlayer_WeaponServices_CanUse(CCSPlayer_WeaponServices*, CBasePlayerWeapon*);
KHook::Return<void> Detour_CCSPlayer_WeaponServices_EquipWeapon(CCSPlayer_WeaponServices*, CBasePlayerWeapon*);
KHook::Return<bool> Detour_CEntityIdentity_AcceptInput(CEntityIdentity* pThis, CUtlSymbolLarge* pInputName, CEntityInstance* pActivator, CEntityInstance* pCaller, variant_t* value, void*, void*);
KHook::Return<void*> Detour_CNavMesh_GetNearestNavArea(CNavMesh* pNavMesh, float* unk2, unsigned int* unk3, unsigned int unk4, int64_t unk5, float unk6, int64_t unk7);
KHook::Return<void> Detour_ProcessMovement(CCSPlayer_MovementServices* pThis, void* pMove);
KHook::Return<void> Detour_ProcessMovement_Post(CCSPlayer_MovementServices* pThis, void* pMove);
KHook::Return<void*> Detour_ProcessUsercmds(CCSPlayerController* pController, CUserCmd* cmds, int numcmds, bool paused, float margin);
KHook::Return<void> Detour_CGamePlayerEquip_InputTriggerForAllPlayers(CGamePlayerEquip*, InputData_t*);
KHook::Return<void> Detour_CGamePlayerEquip_InputTriggerForActivatedPlayer(CGamePlayerEquip*, InputData_t*);
KHook::Return<void> Detour_CTriggerGravity_GravityTouch(CTriggerGravity* pEntity, CBaseEntity* pOther);
KHook::Return<CServerSideClient*> Detour_GetFreeClient(int64_t unk1, const __m128i* unk2, unsigned int unk3, int64_t unk4, char unk5, void* unk6);
KHook::Return<float> Detour_CCSPlayerPawn_GetMaxSpeed(CCSPlayerPawn*);
KHook::Return<int64> Detour_FindUseEntity(CCSPlayer_UseServices* pThis, float a2);
KHook::Return<int64> Detour_FindUseEntity_Post(CCSPlayer_UseServices* pThis, float a2);
KHook::Return<bool> Detour_TraceFunc(int64*, int*, float*, uint64);
KHook::Return<bool> Detour_TraceShape(int64*, int64, int64, int64, CTraceFilter*, int64);
KHook::Return<void> Detour_CEntityIOOutput_FireOutputInternal(const CEntityIOOutput* pThis, CEntityInstance* pActivator, CEntityInstance* pCaller, const CVariant* value, float flDelay, void*, void*);
#ifdef PLATFORM_WINDOWS
KHook::Return<Vector*> Detour_CBasePlayerPawn_GetEyePosition(CBasePlayerPawn*, Vector*);
KHook::Return<QAngle*> Detour_CBasePlayerPawn_GetEyeAngles(CBasePlayerPawn*, QAngle*);
#else
KHook::Return<Vector> Detour_CBasePlayerPawn_GetEyePosition(CBasePlayerPawn*);
KHook::Return<QAngle> Detour_CBasePlayerPawn_GetEyeAngles(CBasePlayerPawn*);
#endif
KHook::Return<void> Detour_CBaseFilter_InputTestActivator(CBaseFilter* pThis, InputData_t& inputdata);
KHook::Return<void> Detour_GameSystem_Think_CheckSteamBan_Post();
KHook::Return<AcquireResult> Detour_CCSPlayer_ItemServices_CanAcquire(CCSPlayer_ItemServices* pItemServices, CEconItemView* pEconItem, AcquireMethod iAcquireMethod, uint64_t unk4);
KHook::Return<void> Detour_CS_Script_SetModel(uint64_t unk1);
KHook::Return<void> Detour_CS_Script_SetModel_Post(uint64_t unk1);
KHook::Return<void> Detour_CBaseModelEntity_SetModel(CBaseModelEntity* pModel, const char* pszModel);
KHook::Return<void> Detour_CCSGameRules_GoToIntermission(CCSGameRules* pThis, bool bAbortedMatch);
