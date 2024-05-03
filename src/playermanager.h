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
		m_iPlayerState = 1; // STATE_WELCOME is the initial state
		m_iLeaderIndex = 0;
		m_iLeaderTracerIndex = 0;
		m_flLeaderVoteTime = -30.0f;
		m_flSpeedMod = 1.f;
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
	void SetPlayerState(uint32 iPlayerState) { m_iPlayerState = iPlayerState; }
	void SetLeader(int leaderIndex);
	void SetLeaderTracer(int tracerIndex) { m_iLeaderTracerIndex = tracerIndex; }
	void SetLeaderVoteTime(float flCurtime) { m_flLeaderVoteTime = flCurtime; }
	void SetGlowModel(CBaseModelEntity *pModel) { m_hGlowModel.Set(pModel); }
	void SetSpeedMod(float flSpeedMod) { m_flSpeedMod = flSpeedMod; }

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
	ZEPlayerHandle GetHandle() { return m_Handle; }
	uint32 GetPlayerState() { return m_iPlayerState; }
	bool IsLeader() { return (bool) m_iLeaderIndex; }
	int GetLeaderIndex() { return m_iLeaderIndex; }
	int GetLeaderTracer() { return m_iLeaderTracerIndex; }
	int GetLeaderVoteCount();
	bool HasPlayerVotedLeader(ZEPlayer* pPlayer);
	float GetLeaderVoteTime() { return m_flLeaderVoteTime; }
	CBaseModelEntity *GetGlowModel() { return m_hGlowModel.Get(); }
	float GetSpeedMod() { return m_flSpeedMod; }
	
	void OnAuthenticated();
	void CheckAdmin();
	void CheckInfractions();
	void SpawnFlashLight();
	void ToggleFlashLight();
	void StartBeacon(Color color, ZEPlayerHandle Giver = 0);
	void EndBeacon();
	void AddLeaderVote(ZEPlayer* pPlayer);
	void PurgeLeaderVotes();
	void StartGlow(Color color, int duration);
	void EndGlow();

private:
	bool m_bAuthenticated;
	bool m_bConnected;
	const CSteamID* m_UnauthenticatedSteamID;
	const CSteamID* m_SteamID;
	CPlayerSlot m_slot;
	bool m_bFakeClient;
	bool m_bMuted;
	bool m_bGagged;
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
	ZEPlayerHandle m_Handle;
	uint32 m_iPlayerState;
	int m_iLeaderIndex;
	CUtlVector<ZEPlayerHandle> m_vecLeaderVotes;
	int m_iLeaderTracerIndex;
	float m_flLeaderVoteTime;
	CHandle<CBaseModelEntity> m_hGlowModel;
	float m_flSpeedMod;
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
	void OnSteamAPIActivated();
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

	void UpdatePlayerStates();

	STEAM_GAMESERVER_CALLBACK_MANUAL(CPlayerManager, OnValidateAuthTicket, ValidateAuthTicketResponse_t, m_CallbackValidateAuthTicketResponse);

private:
	ZEPlayer *m_vecPlayers[MAXPLAYERS];

	uint64 m_nUsingStopSound;
	uint64 m_nUsingSilenceSound;
	uint64 m_nUsingStopDecals;
};

extern CPlayerManager *g_playerManager;

void PrecacheBeaconParticle(IEntityResourceManifest* pResourceManifest);