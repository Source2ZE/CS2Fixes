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

#include "cs2_sdk/netmessages.h"
#include "engine/igameeventsystem.h"
#include "entity/cgamerules.h"
#include "igameevents.h"
#include "iserver.h"
#include "khook.hpp"

class CCSPlayer_MovementServices;
class CServerSideClient;
struct TouchLinked_t;
class CCSPlayer_WeaponServices;
class CBasePlayerWeapon;
class CGamePlayerEquip;
class CCSPlayerPawn;
class CVPhys2World;
class CTriggerGravity;
class CGameConfig;

void InitVirtualHooks();
void RemoveVirtualHooks();

KHook::Return<void> Hook_GameFrame_Post(IServerGameDLL* pThis, bool simulating, bool bFirstTick, bool bLastTick);
KHook::Return<void> Hook_GameServerSteamAPIActivated(IServerGameDLL* pThis);
KHook::Return<void> Hook_ApplyGameSettings(IServerGameDLL* pThis, KeyValues* pKV);
KHook::Return<void> Hook_ClientActive_Post(IServerGameClients* pThis, CPlayerSlot slot, bool bLoadGame, const char* pszName, uint64 xuid);
KHook::Return<void> Hook_ClientDisconnect_Post(IServerGameClients* pThis, CPlayerSlot slot, ENetworkDisconnectionReason reason, const char* pszName, uint64 xuid, const char* pszNetworkID);
KHook::Return<void> Hook_ClientPutInServer_Post(IServerGameClients* pThis, CPlayerSlot slot, const char* pszName, int type, uint64 xuid);
KHook::Return<void> Hook_ClientSettingsChanged(IServerGameClients* pThis, CPlayerSlot slot);
KHook::Return<void> Hook_OnClientConnected(IServerGameClients* pThis, CPlayerSlot slot, const char* pszName, uint64 xuid, const char* pszNetworkID, const char* pszAddress, bool bFakePlayer);
KHook::Return<bool> Hook_ClientConnect(IServerGameClients* pThis, CPlayerSlot slot, const char* pszName, uint64 xuid, const char* pszNetworkID, bool unk1, CBufferString* pRejectReason);
KHook::Return<void> Hook_ClientCommand(IServerGameClients* pThis, CPlayerSlot nSlot, const CCommand& _cmd);
KHook::Return<void> Hook_ClientSvcUserMessage(IServerGameClients* pThis, CPlayerSlot slot, int um_type, uint32 size, const void* buf);
KHook::Return<void> Hook_PostEventAbstract(IGameEventSystem* pThis, CSplitScreenSlot nSlot, bool bLocalOnly, int nClientCount, const uint64* clients,
										   INetworkMessageInternal* pEvent, const CNetMessage* pData, unsigned long nSize, NetChannelBufType_t bufType);
KHook::Return<void> Hook_StartupServer_Post(INetworkServerService* pThis, const GameSessionConfiguration_t& config, ISource2WorldSession*, const char*);
KHook::Return<void> Hook_CheckTransmit_Post(ISource2GameEntities* pThis, CCheckTransmitInfo** ppInfoList, int infoCount, CBitVec<16384>& unionTransmitEdicts,
											CBitVec<16384>&, const Entity2Networkable_t** pNetworkables, const uint16* pEntityIndicies, int nEntities);
KHook::Return<void> Hook_DispatchConCommand(ICvar* pThis, ConCommandRef cmd, const CCommandContext& ctx, const CCommand& args);
KHook::Return<void> Hook_CreateWorkshopMapGroup(IGameTypes* pThis, const char* name, const CUtlStringList& mapList);
KHook::Return<int> Hook_LoadEventsFromFile(IGameEventManager2* pThis, const char* filename, bool bSearchAll);
KHook::Return<bool> Hook_FireEvent(IGameEventManager2* pThis, IGameEvent* pEvent, bool bDontBroadcast);
KHook::Return<void> Hook_Spawn(CEntitySystem* pThis, int nCount, const EntitySpawnInfo_t* pInfo);
KHook::Return<bool> Hook_ProcessVoiceData(CServerSideClient* pClient, const CCLCMsg_VoiceData_t& msg);
KHook::Return<void> Hook_SetGameSpawnGroupMgr(INetworkGameServer* pThis, IGameSpawnGroupMgr* pSpawnGroupMgr);
KHook::Return<void> Hook_GetTouchingList_Post(CVPhys2World* pThis, CUtlVector<TouchLinked_t>* pList, bool unknown);
KHook::Return<void> Hook_CheckMovingGround(CCSPlayer_MovementServices* pThis, double frametime);
KHook::Return<void> Hook_DropWeapon_Post(CCSPlayer_WeaponServices* pThis, CBasePlayerWeapon* pWeapon, Vector* pVecTarget, Vector* pVelocity);
KHook::Return<void> Hook_PlayerEquipUse(CGamePlayerEquip* pThis, InputData_t* pInput);
KHook::Return<void> Hook_PlayerEquipPrecache_Post(CGamePlayerEquip* pThis, CEntityPrecacheContext*);
KHook::Return<void> Hook_TriggerGravityPrecache_Post(CTriggerGravity* pThis, CEntityPrecacheContext* param);
KHook::Return<void> Hook_TriggerGravityEndTouch_Post(CTriggerGravity* pThis, CBaseEntity* pOther);
KHook::Return<bool> Hook_OnTakeDamage_Alive(CCSPlayerPawn* pPawn, CTakeDamageResult* pDamageResult);
KHook::Return<void> Hook_CCSPlayerPawn_Teleport(CCSPlayerPawn* pPawn, const Vector* pPosition, const QAngle* pAngles, const Vector* pVelocity);
KHook::Return<void> Hook_CCSPlayerPawn_Teleport_Post(CCSPlayerPawn* pPawn, const Vector* pPosition, const QAngle* pAngles, const Vector* pVelocity);
