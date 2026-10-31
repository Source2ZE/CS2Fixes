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
#include "engine2/isource2engine.h"
#include "khook.hpp"
#include "map.h"

#include <set>

class CCheckTransmitInfo;
class IRecipientFilter;
class ISoundEmitterSystemBase;
class CBaseEntity;
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
class CCSPlayerPawn;
class CCSPlayer_UseServices;
class CTraceFilter;
class CNavMesh;
class CBaseModelEntity;
class Vector;
class QAngle;
class CEconItemView;
class CCSGameRules;
class CGameEntitySystem;
class CBeam;
class IWorldRendererMgr;
struct CPulseArgumentPack;
struct CTakeDamageResult;

extern std::set<SpawnGroupHandle_t> g_setUnloadingSpawnGroups;

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

KHook::Return<int64> Detour_CBaseEntity_TakeDamageOld(CBaseEntity* pThis, CTakeDamageInfo* pInfo, CTakeDamageResult* pResult);
KHook::Return<int64> Detour_CBaseEntity_TakeDamageOld_Post(CBaseEntity* pThis, CTakeDamageInfo* pInfo, CTakeDamageResult* pResult);
KHook::Return<void> Detour_TriggerPush_Touch(CTriggerPush* pPush, CBaseEntity* pOther);
KHook::Return<bool> Detour_CCSPlayer_WeaponServices_CanUse(CCSPlayer_WeaponServices*, CBasePlayerWeapon*);
KHook::Return<void> Detour_CCSPlayer_WeaponServices_EquipWeapon(CCSPlayer_WeaponServices*, CBasePlayerWeapon*);
KHook::Return<bool> Detour_CEntityIdentity_AcceptInput(CEntityIdentity* pThis, CUtlSymbolLarge* pInputName, CEntityInstance* pActivator, CEntityInstance* pCaller, variant_t* value, CPulseArgumentPack* pArgumentPack, CPulseInputParamMap* pParamMap);
KHook::Return<void*> Detour_CNavMesh_GetNearestNavArea(CNavMesh* pNavMesh, float* unk2, unsigned int* unk3, unsigned int unk4, int64_t unk5, float unk6, int64_t unk7);
KHook::Return<void> Detour_ProcessMovement(CCSPlayer_MovementServices* pThis, void* pMove);
KHook::Return<void> Detour_ProcessMovement_Post(CCSPlayer_MovementServices* pThis, void* pMove);
KHook::Return<void*> Detour_ProcessUsercmds(CCSPlayerController* pController, CUserCmd* cmds, int numcmds, bool paused, float margin);
KHook::Return<void> Detour_CTriggerGravity_GravityTouch(CTriggerGravity* pEntity, CBaseEntity* pOther);
KHook::Return<CServerSideClient*> Detour_GetFreeClient(int64_t unk1, const __m128i* unk2, unsigned int unk3, int64_t unk4, char unk5, void* unk6);
KHook::Return<float> Detour_CCSPlayerPawn_GetMaxSpeed(CCSPlayerPawn*);
KHook::Return<CBaseEntity*> Detour_FindUseEntity(CCSPlayer_UseServices* pThis, float unk2, int64_t unk3);
KHook::Return<CBaseEntity*> Detour_FindUseEntity_Post(CCSPlayer_UseServices* pThis, float unk2, int64_t unk3);
KHook::Return<bool> Detour_TraceFunc(int64*, int*, float*, uint64);
KHook::Return<bool> Detour_TraceShape(int64*, int64, int64, int64, CTraceFilter*, int64);
KHook::Return<void> Detour_CEntityIOOutput_FireOutputInternal(CEntityIOOutput* pThis, CEntityInstance* pActivator, CEntityInstance* pCaller, CPulseArgumentPack* pArgumentPack, float flDelay, CPulseInputParamMap* pParamMap, const CVariant* value);
#ifdef PLATFORM_WINDOWS
KHook::Return<Vector*> Detour_CBasePlayerPawn_GetEyePosition(CBasePlayerPawn*, Vector*);
KHook::Return<QAngle*> Detour_CBasePlayerPawn_GetEyeAngles(CBasePlayerPawn*, QAngle*);
#else
KHook::Return<Vector> Detour_CBasePlayerPawn_GetEyePosition(CBasePlayerPawn*);
KHook::Return<QAngle> Detour_CBasePlayerPawn_GetEyeAngles(CBasePlayerPawn*);
#endif
KHook::Return<void> Detour_GameSystem_Think_CheckSteamBan();
KHook::Return<void> Detour_GameSystem_Think_CheckSteamBan_Post();
KHook::Return<AcquireResult> Detour_CCSPlayer_ItemServices_CanAcquire(CCSPlayer_ItemServices* pItemServices, CEconItemView* pEconItem, AcquireMethod iAcquireMethod, uint64_t unk4);
KHook::Return<void> Detour_CS_Script_SetModel(uint64_t unk1);
KHook::Return<void> Detour_CS_Script_SetModel_Post(uint64_t unk1);
KHook::Return<void> Detour_CBaseModelEntity_SetModel(CBaseModelEntity* pModel, const char* pszModel);
KHook::Return<void> Detour_CCSGameRules_GoToIntermission(CCSGameRules* pThis, bool bAbortedMatch);
KHook::Return<void> Detour_SetBeamOrigin(CBeam* pThis, const Vector* pVecWorldPosition);
KHook::Return<void> Detour_SetBeamEndPos(CBeam* pThis, const Vector* pVecWorldPosition);
KHook::Return<CSingleWorldRep*> Detour_CWorldRendererMgr_CreateWorld_Internal_Post(IWorldRendererMgr* pThis, CSingleWorldRep* pSingleWorld);
KHook::Return<ILoadingSpawnGroup*> Detour_CGameEntitySystem_CreateEntities(CGameEntitySystem* pThis, SpawnGroupHandle_t hSpawnGroup, const CUtlVector<const CEntityKeyValues*>* pKeyValues, const matrix3x4a_t* pSpawnOffset, const char* pszFilter, CreateSpawnGroupType_t createType, const void* pNameFixup, CUtlVector<CEntityHandle>* pOutEntities, bool bUnk);
