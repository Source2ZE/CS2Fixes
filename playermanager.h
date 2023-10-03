#pragma once

#include "utlvector.h"
#include <playerslot.h>
#include "common.h"

extern IVEngineServer2* g_pEngineServer2;

enum class ETargetType {
	NONE,
	ALL,
	T,
	CT,
	PLAYER
};

class ZEPlayer
{
public:
	ZEPlayer(bool m_bFakeClient = false): m_bFakeClient(m_bFakeClient)
	{ 
		m_bAuthenticated = false;
		m_bStopSound = false;
	}

	bool IsFakeClient() { return m_bFakeClient; }
	bool IsAuthenticated() { return m_bAuthenticated; }
	void SetAuthenticated() { m_bAuthenticated = true; }
	uint64 GetSteamId64() { return m_SteamID->ConvertToUint64(); }
	const CSteamID* GetSteamId() { return m_SteamID; }
	void SetSteamId(const CSteamID* steamID) { m_SteamID = steamID; }

	void ToggleStopSound() { m_bStopSound = !m_bStopSound; }
	bool IsUsingStopSound() { return m_bStopSound; }

private:
	bool m_bAuthenticated;
	const CSteamID* m_SteamID;
	bool m_bStopSound;
	bool m_bFakeClient;
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
	ETargetType TargetPlayerString(const char* target, int &iNumClients, CPlayerSlot *clients);
	ZEPlayer *GetPlayer(CPlayerSlot slot) { return m_vecPlayers[slot.Get()]; };

private:
	ZEPlayer* m_vecPlayers[64];
};

extern CPlayerManager *g_playerManager;