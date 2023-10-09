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
#include "common.h"
#include "utlvector.h"
#include "steam/steamclientpublic.h"
#include <playerslot.h>

enum class ETargetType {
	NONE,
	PLAYER,
	SELF,
	RANDOM,
	RANDOM_T,
	RANDOM_CT,
	ALL,
	T,
	CT,
};

class ZEPlayer
{
public:
	ZEPlayer(CPlayerSlot slot, bool m_bFakeClient = false): m_slot(slot), m_bFakeClient(m_bFakeClient)
	{ 
		m_bAuthenticated = false;
		m_bStopSound = false;
		m_iAdminFlags = 0;
		m_SteamID = nullptr;
	}

	bool IsFakeClient() { return m_bFakeClient; }
	bool IsAuthenticated() { return m_bAuthenticated; }
	uint64 GetSteamId64() { return m_SteamID->ConvertToUint64(); }
	const CSteamID* GetSteamId() { return m_SteamID; }
	bool IsAdminFlagSet(uint64 iFlag);
	
	void SetAuthenticated() { m_bAuthenticated = true; }
	void SetSteamId(const CSteamID* steamID) { m_SteamID = steamID; }
	void SetAdminFlags(uint64 iAdminFlags) { m_iAdminFlags = iAdminFlags; }
	void SetPlayerSlot(CPlayerSlot slot) { m_slot = slot; }
	void SetMuted(bool muted) { m_bMuted = muted; }
	void SetGagged(bool gagged) { m_bGagged = gagged; }

	void ToggleStopSound() { m_bStopSound = !m_bStopSound; }
	bool IsUsingStopSound() { return m_bStopSound; }
	bool IsMuted() { return m_bMuted; }
	bool IsGagged() { return m_bGagged; }
	CPlayerSlot GetPlayerSlot() { return m_slot; }
	
	void OnAuthenticated();
	void CheckAdmin();
	void CheckInfractions();

private:
	bool m_bAuthenticated;
	const CSteamID* m_SteamID;
	bool m_bStopSound;
	bool m_bFakeClient;
	bool m_bMuted;
	bool m_bGagged;
	CPlayerSlot m_slot;
	uint64 m_iAdminFlags;
};

class CPlayerManager
{
public:
	CPlayerManager()
	{
		V_memset(m_vecPlayers, 0, sizeof(m_vecPlayers));
	}

	void OnClientConnected(CPlayerSlot slot);
	void OnClientDisconnect(CPlayerSlot slot);
	void OnBotConnected(CPlayerSlot slot);
	void TryAuthenticate();
	void CheckInfractions();
	ETargetType TargetPlayerString(int iCommandClient, const char* target, int &iNumClients, int *clients);
	ZEPlayer *GetPlayer(CPlayerSlot slot) { return m_vecPlayers[slot.Get()]; };

private:
	ZEPlayer* m_vecPlayers[MAXPLAYERS];
};

extern CPlayerManager *g_playerManager;