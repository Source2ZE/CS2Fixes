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

#include "igameevents.h"
#include "networksystem/inetworkserializer.h"
#include <ISmmPlugin.h>
#include <iplayerinfo.h>
#include <iserver.h>
#include <sh_vector.h>

struct CTakeDamageInfoContainer;
class CCSPlayer_MovementServices;
class CServerSideClient;
struct TouchLinked_t;

class CS2Fixes : public ISmmPlugin, public IMetamodListener
{
public:
	bool Load(PluginId id, ISmmAPI* ismm, char* error, size_t maxlen, bool late);
	void Hook_PostEvent(CSplitScreenSlot nSlot, bool bLocalOnly, int nClientCount, const uint64* clients,
						INetworkMessageInternal* pEvent, const CNetMessage* pData, unsigned long nSize, NetChannelBufType_t bufType);
	bool Unload(char* error, size_t maxlen);
	bool Pause(char* error, size_t maxlen);
	bool Unpause(char* error, size_t maxlen);
	void AllPluginsLoaded();

public: // hooks
	void Hook_GameServerSteamAPIActivated();
	void Hook_GameServerSteamAPIDeactivated();
	void OnLevelInit(char const* pMapName,
					 char const* pMapEntities,
					 char const* pOldLevel,
					 char const* pLandmarkName,
					 bool loadGame,
					 bool background);
	void OnLevelShutdown();
	void Hook_GameFramePost(bool simulating, bool bFirstTick, bool bLastTick);
	void Hook_ClientActive(CPlayerSlot slot, bool bLoadGame, const char* pszName, uint64 xuid);
	void Hook_ClientDisconnect(CPlayerSlot slot, ENetworkDisconnectionReason reason, const char* pszName, uint64 xuid, const char* pszNetworkID);
	void Hook_ClientPutInServer(CPlayerSlot slot, char const* pszName, int type, uint64 xuid);
	void Hook_ClientSettingsChanged(CPlayerSlot slot);
	void Hook_OnClientConnected(CPlayerSlot slot, const char* pszName, uint64 xuid, const char* pszNetworkID, const char* pszAddress, bool bFakePlayer);
	bool Hook_ClientConnect(CPlayerSlot slot, const char* pszName, uint64 xuid, const char* pszNetworkID, bool unk1, CBufferString* pRejectReason);
	void Hook_ClientCommand(CPlayerSlot nSlot, const CCommand& _cmd);
	void Hook_CheckTransmit(CCheckTransmitInfo** ppInfoList, int infoCount, CBitVec<16384>& unionTransmitEdicts,
							const Entity2Networkable_t** pNetworkables, const uint16* pEntityIndicies, int nEntities, bool bEnablePVSBits);
	void Hook_DispatchConCommand(ConCommandHandle cmd, const CCommandContext& ctx, const CCommand& args);
	void Hook_CGamePlayerEquipUse(class InputData_t*);
	void Hook_StartupServer(const GameSessionConfiguration_t& config, ISource2WorldSession*, const char*);
	void Hook_ApplyGameSettings(KeyValues* pKV);
	void Hook_CreateWorkshopMapGroup(const char* name, const CUtlStringList& mapList);
	void Hook_GoToIntermission(bool bAbortedMatch);
	bool Hook_OnTakeDamage_Alive(CTakeDamageInfoContainer* pInfoContainer);
	void Hook_PhysicsTouchShuffle(CUtlVector<TouchLinked_t>* pList, bool unknown);
#ifdef PLATFORM_WINDOWS
	Vector* Hook_GetEyePosition(Vector*);
	QAngle* Hook_GetEyeAngles(QAngle*);
#else
	Vector Hook_GetEyePosition();
	QAngle Hook_GetEyeAngles();
#endif
	void Hook_CheckMovingGround(double frametime);
	int Hook_LoadEventsFromFile(const char* filename, bool bSearchAll);

public:
	const char* GetAuthor();
	const char* GetName();
	const char* GetDescription();
	const char* GetURL();
	const char* GetLicense();
	const char* GetVersion();
	const char* GetDate();
	const char* GetLogTag();
};

extern CS2Fixes g_CS2Fixes;

PLUGIN_GLOBALVARS();
