#pragma once

#include "utlvector.h"
#include <playerslot.h>
#include "common.h"

extern IVEngineServer2* g_pEngineServer2;

class ZEPlayer
{
public:
	ZEPlayer()
	{ 
		m_bAuthenticated = false;
	}

	bool IsAuthenticated() { return m_bAuthenticated; }
	void SetAuthenticated() { m_bAuthenticated = true; }
	uint64 GetSteamId() { return m_iSteamID; }
	void SetSteamId(uint64 iSteamID) { m_iSteamID = iSteamID; }

private:
	bool m_bAuthenticated;
	uint64 m_iSteamID;
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
	void TryAuthenticate();

private:
	ZEPlayer* m_vecPlayers[64];
};