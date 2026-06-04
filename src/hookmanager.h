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
#include "interfaces/interfaces.h"
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
class INetworkServerService;
class IGameTypes;
class CEntitySystem;
class CVPhys2World;
class INetworkGameServer;
class IGameSpawnGroupMgr;
class IGameEventManager2;
class CBufferString;
class ISource2WorldSession;
class CEntityPrecacheContext;
class CNetMessage;
class CCommandContext;
class CPlayerSlot;
struct CSplitScreenSlot;
struct ConCommandRef;
struct EntitySpawnInfo_t;
struct Entity2Networkable_t;
struct GameSessionConfiguration_t;
class KeyValues;
class CUtlStringList;
template<int NUM_BITS> class CBitVec;
template<typename T> class CUtlVector;

enum ENetworkDisconnectionReason : int;
enum NetChannelBufType_t : signed char;

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

struct TouchLinked_t
{
	uint32_t TouchFlags;

private:
	uint8_t padding_0[20];

public:
	CBaseHandle SourceHandle;
	CBaseHandle TargetHandle;

private:
	uint8_t padding_1[224];

public:
	[[nodiscard]] bool IsUnTouching() const
	{
		return !!(TouchFlags & 0x10);
	}

	[[nodiscard]] bool IsTouching() const
	{
		return (!!(TouchFlags & 4)) || (!!(TouchFlags & 8));
	}
};
static_assert(sizeof(TouchLinked_t) == 256, "Touch_t size mismatch");

class CHookManager
{
public:
	CHookManager(CGameConfig* pGameConfig);
	~CHookManager();

	void CreateHooks(CGameConfig* gameConfig);
	void RemoveHooks();

public:
	KHook::Return<int64> Hook_TakeDamageOld(CBaseEntity*, CTakeDamageInfo*, CTakeDamageResult*);
	KHook::Return<int64> Hook_TakeDamageOld_Post(CBaseEntity*, CTakeDamageInfo*, CTakeDamageResult*);
	KHook::Return<void> Hook_TriggerPushTouch(CTriggerPush*, CBaseEntity*);
	KHook::Return<bool> Hook_IsHearingClient(void*, int);
	KHook::Return<void> Hook_SayTextFilter(IRecipientFilter&, const char*, CCSPlayerController*, uint64);
	KHook::Return<void> Hook_SayText2Filter(IRecipientFilter&, CCSPlayerController*, uint64, const char*, const char*, const char*, const char*, const char*);
	KHook::Return<bool> Hook_CanUse(CCSPlayer_WeaponServices*, CBasePlayerWeapon*);
	KHook::Return<void> Hook_EquipWeapon(CCSPlayer_WeaponServices*, CBasePlayerWeapon*);
	KHook::Return<bool> Hook_AcceptInput(CEntityIdentity*, CUtlSymbolLarge*, CEntityInstance*, CEntityInstance*, variant_t*, int, void*, void*);
	KHook::Return<void*> Hook_GetNearestNavArea(CNavMesh*, float*, unsigned int*, unsigned int, int64_t, float, int64_t);
	KHook::Return<void> Hook_ProcessMovement(CCSPlayer_MovementServices*, void*);
	KHook::Return<void> Hook_ProcessMovement_Post(CCSPlayer_MovementServices*, void*);
	KHook::Return<void*> Hook_ProcessUsercmds(CCSPlayerController*, CUserCmd*, int, bool, float);
	KHook::Return<void> Hook_InputTriggerForAllPlayers(CGamePlayerEquip*, InputData_t*);
	KHook::Return<void> Hook_InputTriggerForActivatedPlayer(CGamePlayerEquip*, InputData_t*);
	KHook::Return<void> Hook_GravityTouch(CTriggerGravity*, CBaseEntity*);
	KHook::Return<CServerSideClient*> Hook_GetFreeClient(int64_t, const __m128i*, unsigned int, int64_t, char, void*);
	KHook::Return<float> Hook_GetMaxSpeed(CCSPlayerPawn*);
	KHook::Return<int64> Hook_FindUseEntity(CCSPlayer_UseServices*, float);
	KHook::Return<int64> Hook_FindUseEntity_Post(CCSPlayer_UseServices*, float);
	KHook::Return<bool> Hook_TraceFunc(int64*, int*, float*, uint64);
	KHook::Return<bool> Hook_TraceShape(int64*, int64, int64, int64, CTraceFilter*, int64);
	KHook::Return<void> Hook_FireOutputInternal(const CEntityIOOutput*, CEntityInstance*, CEntityInstance*, const CVariant*, float, void*, void*);
#ifdef PLATFORM_WINDOWS
	KHook::Return<Vector*> Hook_GetEyePosition(CBasePlayerPawn*, Vector*);
	KHook::Return<QAngle*> Hook_GetEyeAngles(CBasePlayerPawn*, QAngle*);
#else
	KHook::Return<Vector> Hook_GetEyePosition(CBasePlayerPawn*);
	KHook::Return<QAngle> Hook_GetEyeAngles(CBasePlayerPawn*);
#endif
	KHook::Return<void> Hook_InputTestActivator(CBaseFilter*, InputData_t&);
	KHook::Return<void> Hook_CheckSteamBan_Post();
	KHook::Return<AcquireResult> Hook_CanAcquire(CCSPlayer_ItemServices*, CEconItemView*, AcquireMethod, uint64_t);
	KHook::Return<void> Hook_ScriptSetModel(uint64_t);
	KHook::Return<void> Hook_ScriptSetModel_Post(uint64_t);
	KHook::Return<void> Hook_SetModel(CBaseModelEntity*, const char*);
	KHook::Return<void> Hook_GoToIntermission(CCSGameRules*, bool);

	KHook::Return<void> Hook_GameFrame_Post(IServerGameDLL*, bool, bool, bool);
	KHook::Return<void> Hook_GameServerSteamAPIActivated(IServerGameDLL*);
	KHook::Return<void> Hook_ApplyGameSettings(IServerGameDLL*, KeyValues*);
	KHook::Return<void> Hook_ClientActive_Post(IServerGameClients*, CPlayerSlot, bool, const char*, uint64);
	KHook::Return<void> Hook_ClientDisconnect_Post(IServerGameClients*, CPlayerSlot, ENetworkDisconnectionReason, const char*, uint64, const char*);
	KHook::Return<void> Hook_ClientPutInServer_Post(IServerGameClients*, CPlayerSlot, const char*, int, uint64);
	KHook::Return<void> Hook_ClientSettingsChanged(IServerGameClients*, CPlayerSlot);
	KHook::Return<void> Hook_OnClientConnected(IServerGameClients*, CPlayerSlot, const char*, uint64, const char*, const char*, bool);
	KHook::Return<bool> Hook_ClientConnect(IServerGameClients*, CPlayerSlot, const char*, uint64, const char*, bool, CBufferString*);
	KHook::Return<void> Hook_ClientCommand(IServerGameClients*, CPlayerSlot, const CCommand&);
	KHook::Return<void> Hook_PostEventAbstract(IGameEventSystem*, CSplitScreenSlot, bool, int, const uint64*, INetworkMessageInternal*, const CNetMessage*, unsigned long, NetChannelBufType_t);
	KHook::Return<void> Hook_StartupServer_Post(INetworkServerService*, const GameSessionConfiguration_t&, ISource2WorldSession*, const char*);
	KHook::Return<void> Hook_CheckTransmit_Post(ISource2GameEntities*, CCheckTransmitInfo**, int, CBitVec<16384>&, CBitVec<16384>&, const Entity2Networkable_t**, const uint16*, int);
	KHook::Return<void> Hook_DispatchConCommand(ICvar*, ConCommandRef, const CCommandContext&, const CCommand&);
	KHook::Return<int> Hook_LoadEventsFromFile(IGameEventManager2*, const char*, bool);
	KHook::Return<void> Hook_Spawn_Post(CEntitySystem*, int, const EntitySpawnInfo_t*);
	KHook::Return<void> Hook_SetGameSpawnGroupMgr(INetworkGameServer*, IGameSpawnGroupMgr*);
	KHook::Return<void> Hook_CreateWorkshopMapGroup(IGameTypes*, const char*, const CUtlStringList&);
	KHook::Return<void> Hook_GetTouchingList_Post(CVPhys2World*, CUtlVector<TouchLinked_t>*, bool);
	KHook::Return<void> Hook_CheckMovingGround(CCSPlayer_MovementServices*, double);
	KHook::Return<void> Hook_DropWeapon_Post(CCSPlayer_WeaponServices*, CBasePlayerWeapon*, Vector*, Vector*);
	KHook::Return<void> Hook_PlayerEquipUse(CGamePlayerEquip*, InputData_t*);
	KHook::Return<void> Hook_PlayerEquipPrecache_Post(CGamePlayerEquip*, CEntityPrecacheContext*);
	KHook::Return<void> Hook_TriggerGravityPrecache_Post(CTriggerGravity*, CEntityPrecacheContext*);
	KHook::Return<void> Hook_TriggerGravityEndTouch_Post(CTriggerGravity*, CBaseEntity*);
	KHook::Return<bool> Hook_OnTakeDamage_Alive(CCSPlayerPawn*, CTakeDamageResult*);
	KHook::Return<void> Hook_CCSPlayerPawn_Teleport(CCSPlayerPawn*, const Vector*, const QAngle*, const Vector*);

protected:
	// Inline function hooks
	KHook::Member<CBaseEntity, int64, CTakeDamageInfo*, CTakeDamageResult*>* m_hTakeDamageOld;
	KHook::Member<CTriggerPush, void, CBaseEntity*>* m_hTriggerPushTouch;
	KHook::Function<bool, void*, int>* m_hIsHearingClient;
	KHook::Function<void, IRecipientFilter&, const char*, CCSPlayerController*, uint64>* m_hSayTextFilter;
	KHook::Function<void, IRecipientFilter&, CCSPlayerController*, uint64, const char*, const char*, const char*, const char*, const char*>* m_hSayText2Filter;
	KHook::Member<CCSPlayer_WeaponServices, bool, CBasePlayerWeapon*>* m_hCanUse;
	KHook::Member<CCSPlayer_WeaponServices, void, CBasePlayerWeapon*>* m_hEquipWeapon;
	KHook::Member<CEntityIdentity, bool, CUtlSymbolLarge*, CEntityInstance*, CEntityInstance*, variant_t*, int, void*, void*>* m_hAcceptInput;
	KHook::Member<CNavMesh, void*, float*, unsigned int*, unsigned int, int64_t, float, int64_t>* m_hGetNearestNavArea;
	KHook::Member<CCSPlayer_MovementServices, void, void*>* m_hProcessMovement;
	KHook::Member<CCSPlayerController, void*, CUserCmd*, int, bool, float>* m_hProcessUsercmds;
	KHook::Member<CGamePlayerEquip, void, InputData_t*>* m_hInputTriggerForAllPlayers;
	KHook::Member<CGamePlayerEquip, void, InputData_t*>* m_hInputTriggerForActivatedPlayer;
	KHook::Member<CTriggerGravity, void, CBaseEntity*>* m_hGravityTouch;
	KHook::Function<CServerSideClient*, int64_t, const __m128i*, unsigned int, int64_t, char, void*>* m_hGetFreeClient;
	KHook::Member<CCSPlayerPawn, float>* m_hGetMaxSpeed;
	KHook::Member<CCSPlayer_UseServices, int64, float>* m_hFindUseEntity;
	KHook::Function<bool, int64*, int*, float*, uint64>* m_hTraceFunc;
	KHook::Function<bool, int64*, int64, int64, int64, CTraceFilter*, int64>* m_hTraceShape;
	KHook::Member<CEntityIOOutput, void, CEntityInstance*, CEntityInstance*, const CVariant*, float, void*, void*>* m_hFireOutputInternal;
#ifdef PLATFORM_WINDOWS
	KHook::Member<CBasePlayerPawn, Vector*, Vector*>* m_hGetEyePosition;
	KHook::Member<CBasePlayerPawn, QAngle*, QAngle*>* m_hGetEyeAngles;
#else
	KHook::Member<CBasePlayerPawn, Vector>* m_hGetEyePosition;
	KHook::Member<CBasePlayerPawn, QAngle>* m_hGetEyeAngles;
#endif
	KHook::Member<CBaseFilter, void, InputData_t&>* m_hInputTestActivator;
	KHook::Function<void>* m_hCheckSteamBan;
	KHook::Member<CCSPlayer_ItemServices, AcquireResult, CEconItemView*, AcquireMethod, uint64_t>* m_hCanAcquire;
	KHook::Function<void, uint64_t>* m_hScriptSetModel;
	KHook::Member<CBaseModelEntity, void, const char*>* m_hSetModel;
	KHook::Member<CCSGameRules, void, bool>* m_hGoToIntermission;

	// Virtual function hooks
	KHook::Virtual<IServerGameDLL, void, bool, bool, bool>* m_hGameFrame;
	KHook::Virtual<IServerGameDLL, void>* m_hGameServerSteamAPIActivated;
	KHook::Virtual<IServerGameDLL, void, KeyValues*>* m_hApplyGameSettings;
	KHook::Virtual<IServerGameClients, void, CPlayerSlot, bool, const char*, uint64>* m_hClientActive;
	KHook::Virtual<IServerGameClients, void, CPlayerSlot, ENetworkDisconnectionReason, const char*, uint64, const char*>* m_hClientDisconnect;
	KHook::Virtual<IServerGameClients, void, CPlayerSlot, const char*, int, uint64>* m_hClientPutInServer;
	KHook::Virtual<IServerGameClients, void, CPlayerSlot>* m_hClientSettingsChanged;
	KHook::Virtual<IServerGameClients, void, CPlayerSlot, const char*, uint64, const char*, const char*, bool>* m_hOnClientConnected;
	KHook::Virtual<IServerGameClients, bool, CPlayerSlot, const char*, uint64, const char*, bool, CBufferString*>* m_hClientConnect;
	KHook::Virtual<IServerGameClients, void, CPlayerSlot, const CCommand&>* m_hClientCommand;
	KHook::Virtual<IGameEventSystem, void, CSplitScreenSlot, bool, int, const uint64*, INetworkMessageInternal*, const CNetMessage*, unsigned long, NetChannelBufType_t>* m_hPostEventAbstract;
	KHook::Virtual<INetworkServerService, void, const GameSessionConfiguration_t&, ISource2WorldSession*, const char*>* m_hStartupServer;
	KHook::Virtual<ISource2GameEntities, void, CCheckTransmitInfo**, int, CBitVec<16384>&, CBitVec<16384>&, const Entity2Networkable_t**, const uint16*, int>* m_hCheckTransmit;
	KHook::Virtual<ICvar, void, ConCommandRef, const CCommandContext&, const CCommand&>* m_hDispatchConCommand;
	KHook::Virtual<IGameEventManager2, int, const char*, bool>* m_hLoadEventsFromFile;
	KHook::Virtual<CEntitySystem, void, int, const EntitySpawnInfo_t*>* m_hSpawn;
	KHook::Virtual<INetworkGameServer, void, IGameSpawnGroupMgr*>* m_hSetGameSpawnGroupMgr;
	KHook::Virtual<IGameTypes, void, const char*, const CUtlStringList&>* m_hCreateWorkshopMapGroup;
	KHook::Virtual<CVPhys2World, void, CUtlVector<TouchLinked_t>*, bool>* m_hGetTouchingList;
	KHook::Virtual<CCSPlayer_MovementServices, void, double>* m_hCheckMovingGround;
	KHook::Virtual<CCSPlayer_WeaponServices, void, CBasePlayerWeapon*, Vector*, Vector*>* m_hDropWeapon;
	KHook::Virtual<CGamePlayerEquip, void, InputData_t*>* m_hPlayerEquipUse;
	KHook::Virtual<CGamePlayerEquip, void, CEntityPrecacheContext*>* m_hPlayerEquipPrecache;
	KHook::Virtual<CTriggerGravity, void, CEntityPrecacheContext*>* m_hTriggerGravityPrecache;
	KHook::Virtual<CTriggerGravity, void, CBaseEntity*>* m_hTriggerGravityEndTouch;
	KHook::Virtual<CCSPlayerPawn, bool, CTakeDamageResult*>* m_hOnTakeDamageAlive;
	KHook::Virtual<CCSPlayerPawn, void, const Vector*, const QAngle*, const Vector*>* m_hPlayerPawnTeleport;

protected:
	void* m_pCGameEventManagerVTable = nullptr;
	void* m_pCEntitySystemVTable = nullptr;
	void* m_pCVPhys2WorldVTable = nullptr;
	void* m_pCCSPlayer_MovementServicesVTable = nullptr;
	void* m_pCCSPlayer_WeaponServicesVTable = nullptr;
	void* m_pCGamePlayerEquipVTable = nullptr;
	void* m_pTriggerGravityVTable = nullptr;
	void* m_pCCSPlayerPawnVTable = nullptr;

private:
	float m_flStoreFrametime = 0.f;
	bool m_bFindingUseEntity = false;
	bool m_bInScriptSetModel = false;
};

extern CHookManager* g_pHookManager;
