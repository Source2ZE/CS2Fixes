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
#include "common.h"
#include "utlvector.h"
#include "steam/steam_api_common.h"
#include "steam/steamclientpublic.h"
#include "steam/isteamuser.h"
#include <playerslot.h>
#include "bitvec.h"
#include "entity/lights.h"
#include "entity/cparticlesystem.h"
#include "gamesystem.h"
#include "entity/services.h"

#define DECAL_PREF_KEY_NAME "hide_decals"
#define HIDE_DISTANCE_PREF_KEY_NAME "hide_distance"
#define SOUND_STATUS_PREF_KEY_NAME "sound_status"
#define INVALID_ZEPLAYERHANDLE_INDEX 0u

static uint32 iZEPlayerHandleSerial = 0u; // this should actually be 3 bytes large, but no way enough players join in servers lifespan for this to be an issue

enum class ETargetType {
	NONE,
	PLAYER,
	SELF,
	RANDOM,
	RANDOM_T,
	RANDOM_CT,
	ALL,
	SPECTATOR,
	T,
	CT,
};

class ZEPlayer;

class ZEPlayerHandle
{
public:
	ZEPlayerHandle();
	ZEPlayerHandle(CPlayerSlot slot); // used for initialization inside ZEPlayer constructor
	ZEPlayerHandle(const ZEPlayerHandle& other);
	ZEPlayerHandle(ZEPlayer *pZEPlayer);

	bool IsValid() const { return static_cast<bool>(Get()); }

	uint32 GetIndex() const { return m_Index; }
	uint32 GetPlayerSlot() const { return m_Parts.m_PlayerSlot; }
	uint32 GetSerial() const { return m_Parts.m_Serial; }

	bool operator==(const ZEPlayerHandle &other) const { return other.m_Index == m_Index; }
	bool operator!=(const ZEPlayerHandle &other) const { return other.m_Index != m_Index; }
	bool operator==(ZEPlayer *pZEPlayer) const;
	bool operator!=(ZEPlayer *pZEPlayer) const;

	void operator=(const ZEPlayerHandle &other) { m_Index = other.m_Index; }
	void operator=(ZEPlayer *pZEPlayer) { Set(pZEPlayer); }
	void Set(ZEPlayer *pZEPlayer);
	
	ZEPlayer *Get() const;

private:
	union
	{
		uint32 m_Index;
		struct
		{
			uint32 m_PlayerSlot : 6;
			uint32 m_Serial : 26;
		} m_Parts;
	};
};

class ZEPlayer
{
public:
	ZEPlayer(CPlayerSlot slot, bool m_bFakeClient = false): m_slot(slot), m_bFakeClient(m_bFakeClient), m_Handle(slot)
	{ 
		m_bAuthenticated = false;
		m_SteamID = nullptr;
		m_bConnected = false;
		m_iPlayerState = 1; // STATE_WELCOME is the initial state
		m_flSpeedMod = 1.f;
	}

	bool IsFakeClient() { return m_bFakeClient; }
	bool IsAuthenticated() { return m_bAuthenticated; }
	bool IsConnected() { return m_bConnected; }
	uint64 GetUnauthenticatedSteamId64() { return m_UnauthenticatedSteamID->ConvertToUint64(); }
	const CSteamID* GetUnauthenticatedSteamId() { return m_UnauthenticatedSteamID; }
	uint64 GetSteamId64() { return m_SteamID->ConvertToUint64(); }
	const CSteamID* GetSteamId() { return m_SteamID; }

	CMoveData *currentMoveData;
	bool processingMovement {};

	CCSPlayerController *GetController();
	CCSPlayerPawn *GetPawn();

	void SetVelocity(const Vector &velocity);
	void GetVelocity(Vector *velocity);
	void SetOrigin(const Vector &origin);
	void GetOrigin(Vector *origin);

	bool didTPM {};
	bool overrideTPM {};
	Vector tpmVelocity;
	Vector tpmOrigin;
	Vector lastValidPlane;
	
	void SetConnected() { m_bConnected = true; }
	void SetUnauthenticatedSteamId(const CSteamID* steamID) { m_UnauthenticatedSteamID = steamID; }
	void SetSteamId(const CSteamID* steamID) { m_SteamID = steamID; }
	void SetPlayerSlot(CPlayerSlot slot) { m_slot = slot; }
	void SetIpAddress(std::string strIp) { m_strIp = strIp; }
	void SetPlayerState(uint32 iPlayerState) { m_iPlayerState = iPlayerState; }

	CPlayerSlot GetPlayerSlot() { return m_slot; }
	const char* GetIpAddress() { return m_strIp.c_str(); }
	ZEPlayerHandle GetHandle() { return m_Handle; }
	uint32 GetPlayerState() { return m_iPlayerState; }
	
	void OnAuthenticated();

private:
	bool m_bAuthenticated;
	bool m_bConnected;
	const CSteamID* m_UnauthenticatedSteamID;
	const CSteamID* m_SteamID;
	CPlayerSlot m_slot;
	bool m_bFakeClient;
	std::string m_strIp;
	ZEPlayerHandle m_Handle;
	uint32 m_iPlayerState;
	float m_flSpeedMod;
};

class CPlayerManager
{
public:
	CPlayerManager(bool late = false)
	{
		m_vecPlayers = std::vector<ZEPlayer*>(MAXPLAYERS, nullptr);

		if (late)
			OnLateLoad();
	}

	bool OnClientConnected(CPlayerSlot slot, uint64 xuid);
	void OnLateLoad();
	CPlayerSlot GetSlotFromUserId(uint16 userid);
	ZEPlayer *GetPlayerFromUserId(uint16 userid);
	ZEPlayer *GetPlayerFromSteamId(uint64 steamid);

	ZEPlayer *GetPlayer(CPlayerSlot slot);

private:
	std::vector<ZEPlayer*> m_vecPlayers;
};

extern CPlayerManager *g_playerManager;