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
#include "steam/steamclientpublic.h"
#include <playerslot.h>
#include "bitvec.h"
#include "entity/lights.h"
#include "entity/cparticlesystem.h"

#define DECAL_PREF_KEY_NAME "hide_decals"
#define HIDE_DISTANCE_PREF_KEY_NAME "hide_distance"
#define SOUND_STATUS_PREF_KEY_NAME "sound_status"

struct ClientJoinInfo_t
{
	uint64 steamid;
	double signon_timestamp;
};

extern CUtlVector<ClientJoinInfo_t> g_ClientsPendingAddon;

void AddPendingClient(uint64 steamid);
ClientJoinInfo_t *GetPendingClient(uint64 steamid, int &index);
ClientJoinInfo_t *GetPendingClient(INetChannel *pNetChan);

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

class ZEPlayer
{
public:
	ZEPlayer(CPlayerSlot slot, bool m_bFakeClient = false): m_slot(slot), m_bFakeClient(m_bFakeClient)
	{ 
		m_bAuthenticated = false;
		m_iAdminFlags = 0;
		m_SteamID = nullptr;
		m_bGagged = false;
		m_bMuted = false;
		m_iHideDistance = 0;
		m_bConnected = false;
		m_iTotalDamage = 0;
		m_iTotalHits = 0;
		m_iTotalKills = 0;
		m_bVotedRTV = false;
		m_bVotedExtend = false;
		m_bIsInfected = false;
		m_flRTVVoteTime = 0;
		m_flExtendVoteTime = 0;
		m_iFloodTokens = 0;
		m_flLastTalkTime = 0;
		m_bInGame = false;
		m_iMZImmunity = 0; // out of 100
		m_flNominateTime = -60.0f;
	}

	~ZEPlayer()
	{
		CBarnLight *pFlashLight = m_hFlashLight.Get();

		if (pFlashLight)
			pFlashLight->Remove();
	}

	bool IsFakeClient() { return m_bFakeClient; }
	bool IsAuthenticated() { return m_bAuthenticated; }
	bool IsConnected() { return m_bConnected; }
	uint64 GetUnauthenticatedSteamId64() { return m_UnauthenticatedSteamID->ConvertToUint64(); }
	const CSteamID* GetUnauthenticatedSteamId() { return m_UnauthenticatedSteamID; }
	uint64 GetSteamId64() { return m_SteamID->ConvertToUint64(); }
	const CSteamID* GetSteamId() { return m_SteamID; }
	bool IsAdminFlagSet(uint64 iFlag);
	bool IsFlooding();
	
	void SetAuthenticated() { m_bAuthenticated = true; }
	void SetConnected() { m_bConnected = true; }
	void SetUnauthenticatedSteamId(const CSteamID* steamID) { m_UnauthenticatedSteamID = steamID; }
	void SetSteamId(const CSteamID* steamID) { m_SteamID = steamID; }
	uint64 GetAdminFlags() { return m_iAdminFlags; }
	void SetAdminFlags(uint64 iAdminFlags) { m_iAdminFlags = iAdminFlags; }
	void SetPlayerSlot(CPlayerSlot slot) { m_slot = slot; }
	void SetMuted(bool muted) { m_bMuted = muted; }
	void SetGagged(bool gagged) { m_bGagged = gagged; }
	void SetTransmit(int index, bool shouldTransmit) { shouldTransmit ? m_shouldTransmit.Set(index) : m_shouldTransmit.Clear(index); }
	void ClearTransmit() { m_shouldTransmit.ClearAll(); }
	void SetHideDistance(int distance);
	void SetTotalDamage(int damage) { m_iTotalDamage = damage; }
	void SetTotalHits(int hits) { m_iTotalHits = hits; }
	void SetTotalKills(int kills) { m_iTotalKills = kills; }
	void SetRTVVote(bool bRTVVote) { m_bVotedRTV = bRTVVote; }
	void SetRTVVoteTime(float flCurtime) { m_flRTVVoteTime = flCurtime; }
	void SetExtendVote(bool bExtendVote) { m_bVotedExtend = bExtendVote; }
	void SetInfectState(bool bInfectState) { m_bIsInfected = bInfectState; }
	void SetExtendVoteTime(float flCurtime) { m_flExtendVoteTime = flCurtime; }
	void SetIpAddress(std::string strIp) { m_strIp = strIp; }
	void SetInGame(bool bInGame) { m_bInGame = bInGame; }
	void SetImmunity(int iMZImmunity) { m_iMZImmunity = iMZImmunity; }
	void SetNominateTime(float flCurtime) { m_flNominateTime = flCurtime; }
	void SetFlashLight(CBarnLight *pLight) { m_hFlashLight.Set(pLight); }
	void SetBeaconParticle(CParticleSystem *pParticle) { m_hBeaconParticle.Set(pParticle); }

	bool IsMuted() { return m_bMuted; }
	bool IsGagged() { return m_bGagged; }
	bool ShouldBlockTransmit(int index) { return m_shouldTransmit.Get(index); }
	int GetHideDistance();
	CPlayerSlot GetPlayerSlot() { return m_slot; }
	int GetTotalDamage() { return m_iTotalDamage; }
	int GetTotalHits() { return m_iTotalHits; }
	int GetTotalKills() { return m_iTotalKills; }
	bool GetRTVVote() { return m_bVotedRTV; }
	float GetRTVVoteTime() { return m_flRTVVoteTime; }
	bool GetExtendVote() { return m_bVotedExtend; }
	bool IsInfected() { return m_bIsInfected; }
	float GetExtendVoteTime() { return m_flExtendVoteTime; }
	const char* GetIpAddress() { return m_strIp.c_str(); }
	bool IsInGame() { return m_bInGame; }
	int GetImmunity() { return m_iMZImmunity; }
	float GetNominateTime() { return m_flNominateTime; }
	CBarnLight *GetFlashLight() { return m_hFlashLight.Get(); }
	CParticleSystem *GetBeaconParticle() { return m_hBeaconParticle.Get(); }
	
	void OnAuthenticated();
	void CheckAdmin();
	void CheckInfractions();
	void SpawnFlashLight();
	void ToggleFlashLight();

private:
	bool m_bAuthenticated;
	bool m_bConnected;
	const CSteamID* m_UnauthenticatedSteamID;
	const CSteamID* m_SteamID;
	bool m_bFakeClient;
	bool m_bMuted;
	bool m_bGagged;
	CPlayerSlot m_slot;
	uint64 m_iAdminFlags;
	int m_iHideDistance;
	CBitVec<MAXPLAYERS> m_shouldTransmit;
	int m_iTotalDamage;
	int m_iTotalHits;
	int m_iTotalKills;
	bool m_bVotedRTV;
	float m_flRTVVoteTime;
	bool m_bVotedExtend;
	bool m_bIsInfected;
	float m_flExtendVoteTime;
	int m_iFloodTokens;
	float m_flLastTalkTime;
	std::string m_strIp;
	bool m_bInGame;
	int m_iMZImmunity;
	float m_flNominateTime;
	CHandle<CBarnLight> m_hFlashLight;
	CHandle<CParticleSystem> m_hBeaconParticle;
};

class CPlayerManager
{
public:
	CPlayerManager(bool late = false)
	{
		V_memset(m_vecPlayers, 0, sizeof(m_vecPlayers));
		m_nUsingStopSound = 0;
		m_nUsingSilenceSound = -1; // On by default
		m_nUsingStopDecals = -1; // On by default

		if (late)
			OnLateLoad();
	}

	bool OnClientConnected(CPlayerSlot slot, uint64 xuid, const char* pszNetworkID);
	void OnClientDisconnect(CPlayerSlot slot);
	void OnBotConnected(CPlayerSlot slot);
	void OnClientPutInServer(CPlayerSlot slot);
	void OnLateLoad();
	void TryAuthenticate();
	void CheckInfractions();
	void FlashLightThink();
	void CheckHideDistances();
	void SetupInfiniteAmmo();
	CPlayerSlot GetSlotFromUserId(uint16 userid);
	ZEPlayer *GetPlayerFromUserId(uint16 userid);
	ZEPlayer *GetPlayerFromSteamId(uint64 steamid);
	ETargetType TargetPlayerString(int iCommandClient, const char* target, int &iNumClients, int *clients);

	ZEPlayer *GetPlayer(CPlayerSlot slot);

	uint64 GetStopSoundMask() { return m_nUsingStopSound; }
	uint64 GetSilenceSoundMask() { return m_nUsingSilenceSound; }
	uint64 GetStopDecalsMask() { return m_nUsingStopDecals; }
	
	void SetPlayerStopSound(int slot, bool set);
	void SetPlayerSilenceSound(int slot, bool set);
	void SetPlayerStopDecals(int slot, bool set);

	void ResetPlayerFlags(int slot);

	bool IsPlayerUsingStopSound(int slot) { return m_nUsingStopSound & ((uint64)1 << slot); }
	bool IsPlayerUsingSilenceSound(int slot) { return m_nUsingSilenceSound & ((uint64)1 << slot); }
	bool IsPlayerUsingStopDecals(int slot) { return m_nUsingStopDecals & ((uint64)1 << slot); }

private:
	ZEPlayer *m_vecPlayers[MAXPLAYERS];

	uint64 m_nUsingStopSound;
	uint64 m_nUsingSilenceSound;
	uint64 m_nUsingStopDecals;
};

extern CPlayerManager *g_playerManager;