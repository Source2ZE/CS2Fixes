/**
 * =============================================================================
 * CS2Fixes
 * Copyright (C) 2023-2025 Source2ZE
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

#include "engine/igameeventsystem.h"
#include "entity/cgamerules.h"
#include "gamesystems/spawngroup_manager.h"
#include "igameevents.h"
#include "khook.hpp"
#include "networksystem/inetworkserializer.h"
#include "public/ics2fixes.h"
#include "steam/isteamhttp.h"
#include <ISmmPlugin.h>
#include <iplayerinfo.h>
#include <iserver.h>

#ifdef AMBUILD
	#include "version_gen.h"
#else
	#include "version_gen_placeholder.h"
#endif

class CCSPlayer_MovementServices;
class CServerSideClient;
struct TouchLinked_t;
class CCSPlayer_WeaponServices;
class CBasePlayerWeapon;
class IGameEventSystem;
class CGamePlayerEquip;
class CCSGameRules;
class CCSPlayerPawn;
class CVPhys2World;
class CTriggerGravity;

extern IGameEventSystem* g_gameEventSystem;
extern IGameEventManager2* g_gameEventManager;
extern CGameEntitySystem* g_pEntitySystem;
extern IVEngineServer2* g_pEngineServer2;
extern ISteamHTTP* g_http;
extern CSteamGameServerAPIContext g_steamAPI;
extern CCSGameRules* g_pGameRules;
extern CSpawnGroupMgrGameSystem* g_pSpawnGroupMgr;
extern double g_flUniversalTime;
extern INetworkGameServer* GetNetworkGameServer();
extern CGlobalVars* GetGlobals();
extern CConVar<bool> g_cvarDropMapWeapons;

class CS2Fixes : public ISmmPlugin, public IMetamodListener, public ICS2Fixes
{
public:
	bool Load(PluginId id, ISmmAPI* ismm, char* error, size_t maxlen, bool late);
	bool Unload(char* error, size_t maxlen);
	bool Pause(char* error, size_t maxlen);
	bool Unpause(char* error, size_t maxlen);
	void AllPluginsLoaded();
	void OnLevelInit(char const* pMapName,
					 char const* pMapEntities,
					 char const* pOldLevel,
					 char const* pLandmarkName,
					 bool loadGame,
					 bool background);
	void OnLevelShutdown();

public: // hooks
	KHook::Return<void> Hook_GameFrame_Post(IServerGameDLL* pThis, bool simulating, bool bFirstTick, bool bLastTick);
	KHook::Return<void> Hook_GameServerSteamAPIActivated(IServerGameDLL* pThis);
	KHook::Return<void> Hook_GameServerSteamAPIDeactivated(IServerGameDLL* pThis);
	KHook::Return<void> Hook_ApplyGameSettings(IServerGameDLL* pThis, KeyValues* pKV);
	KHook::Return<void> Hook_ClientActive_Post(IServerGameClients* pThis, CPlayerSlot slot, bool bLoadGame, const char* pszName, uint64 xuid);
	KHook::Return<void> Hook_ClientDisconnect_Post(IServerGameClients* pThis, CPlayerSlot slot, ENetworkDisconnectionReason reason, const char* pszName, uint64 xuid, const char* pszNetworkID);
	KHook::Return<void> Hook_ClientPutInServer_Post(IServerGameClients* pThis, CPlayerSlot slot, char const* pszName, int type, uint64 xuid);
	KHook::Return<void> Hook_ClientSettingsChanged(IServerGameClients* pThis, CPlayerSlot slot);
	KHook::Return<void> Hook_OnClientConnected(IServerGameClients* pThis, CPlayerSlot slot, const char* pszName, uint64 xuid, const char* pszNetworkID, const char* pszAddress, bool bFakePlayer);
	KHook::Return<bool> Hook_ClientConnect(IServerGameClients* pThis, CPlayerSlot slot, const char* pszName, uint64 xuid, const char* pszNetworkID, bool unk1, CBufferString* pRejectReason);
	KHook::Return<void> Hook_ClientCommand(IServerGameClients* pThis, CPlayerSlot nSlot, const CCommand& _cmd);
	KHook::Return<void> Hook_PostEventAbstract(IGameEventSystem* pThis, CSplitScreenSlot nSlot, bool bLocalOnly, int nClientCount, const uint64* clients,
						INetworkMessageInternal* pEvent, const CNetMessage* pData, unsigned long nSize, NetChannelBufType_t bufType);
	KHook::Return<void> Hook_StartupServer_Post(INetworkServerService* pThis, const GameSessionConfiguration_t& config, ISource2WorldSession*, const char*);
	KHook::Return<void> Hook_CheckTransmit_Post(ISource2GameEntities* pThis, CCheckTransmitInfo** ppInfoList, int infoCount, CBitVec<16384>& unionTransmitEdicts,
							CBitVec<16384>&, const Entity2Networkable_t** pNetworkables, const uint16* pEntityIndicies, int nEntities);
	KHook::Return<void> Hook_DispatchConCommand(ICvar* pThis, ConCommandRef cmd, const CCommandContext& ctx, const CCommand& args);
	KHook::Return<void> Hook_SetGameSpawnGroupMgr(INetworkGameServer* pThis, IGameSpawnGroupMgr* pSpawnGroupMgr);
	KHook::Return<void> Hook_CreateWorkshopMapGroup(IGameTypes* pThis, const char* name, CUtlStringList& mapList);
	KHook::Return<int> Hook_LoadEventsFromFile(IGameEventManager2* pThis, const char* filename, bool bSearchAll);
	KHook::Return<void> Hook_GetTouchingList_Post(CVPhys2World* pThis, CUtlVector<TouchLinked_t>* pList, bool unknown);
	KHook::Return<void> Hook_CheckMovingGround(CCSPlayer_MovementServices* pThis, double frametime);
	KHook::Return<void> Hook_DropWeapon_Post(CCSPlayer_WeaponServices* pThis, CBasePlayerWeapon* pWeapon, Vector* pVecTarget, Vector* pVelocity);
	KHook::Return<void> Hook_GoToIntermission(CCSGameRules* pThis, bool bAbortedMatch);
	KHook::Return<void> Hook_PlayerEquipUse(CGamePlayerEquip* pThis, class InputData_t*);
	KHook::Return<void> Hook_PlayerEquipPrecache_Post(CGamePlayerEquip* pThis, CEntityPrecacheContext*);
	KHook::Return<void> Hook_TriggerGravityPrecache_Post(CTriggerGravity* pThis, CEntityPrecacheContext* param);
	KHook::Return<void> Hook_TriggerGravityEndTouch_Post(CTriggerGravity* pThis, CBaseEntity* pOther);
	KHook::Return<bool> Hook_OnTakeDamage_Alive(CCSPlayerPawn* pPawn, CTakeDamageResult* pDamageResult);

public: // MetaMod API
	void* OnMetamodQuery(const char* iface, int* ret);
	std::uint64_t GetAdminFlags(std::uint64_t iSteam64ID) const override;
	bool SetAdminFlags(std::uint64_t iSteam64ID, std::uint64_t iFlags) override;
	int GetAdminImmunity(std::uint64_t iSteam64ID) const override;
	bool SetAdminImmunity(std::uint64_t iSteam64ID, std::uint32_t iImmunity) override;

public:
	const char* GetAuthor() { return PLUGIN_AUTHOR; }
	const char* GetName() { return PLUGIN_DISPLAY_NAME; }
	const char* GetDescription() { return PLUGIN_DESCRIPTION; }
	const char* GetURL() { return PLUGIN_URL; }
	const char* GetLicense() { return PLUGIN_LICENSE; }
	const char* GetVersion() { return PLUGIN_FULL_VERSION; }
	const char* GetDate() { return __DATE__; }
	const char* GetLogTag() { return PLUGIN_LOGTAG; }
};

extern CS2Fixes g_CS2Fixes;

PLUGIN_GLOBALVARS();
