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
#include "bitvec.h"
#include "common.h"
#include "entity/cparticlesystem.h"
#include "entity/cpointworldtext.h"
#include "entity/lights.h"
#include "entity/viewmodels.h"
#include "gamesystem.h"
#include "steam/isteamuser.h"
#include "steam/steam_api_common.h"
#include "steam/steamclientpublic.h"
#include "utlvector.h"
#include <playerslot.h>

#define NO_TARGET_BLOCKS (0)
#define NO_RANDOM (1 << 1)
#define NO_MULTIPLE (1 << 2)
#define NO_SELF (1 << 3)
#define NO_BOT (1 << 4)
#define NO_HUMAN (1 << 5)
#define NO_UNAUTHENTICATED (1 << 6)
#define NO_DEAD (1 << 7)
#define NO_ALIVE (1 << 8)
#define NO_TERRORIST (1 << 9)
#define NO_COUNTER_TERRORIST (1 << 10)
#define NO_SPECTATOR (1 << 11)
#define NO_IMMUNITY (1 << 12)

#define DECAL_PREF_KEY_NAME "hide_decals"
#define HIDE_DISTANCE_PREF_KEY_NAME "hide_distance"
#define SOUND_STATUS_PREF_KEY_NAME "sound_status"
#define NO_SHAKE_PREF_KEY_NAME "no_shake"
#define BUTTON_WATCH_PREF_KEY_NAME "button_watch"
#define INVALID_ZEPLAYERHANDLE_INDEX 0u

static uint32 iZEPlayerHandleSerial = 0u; // this should actually be 3 bytes large, but no way enough players join in servers lifespan for this to be an issue

enum class ETargetType
{
	NONE,
	PLAYER,
	SELF,
	RANDOM,
	RANDOM_T,
	RANDOM_CT,
	RANDOM_SPEC,
	AIM,
	ALL,
	SPECTATOR,
	T,
	CT,
	DEAD,
	ALIVE,
	BOT,
	HUMAN,
	ALL_BUT_SELF,
	ALL_BUT_RANDOM,
	ALL_BUT_RANDOM_T,
	ALL_BUT_RANDOM_CT,
	ALL_BUT_RANDOM_SPEC,
	ALL_BUT_AIM,
	ALL_BUT_SPECTATOR,
	ALL_BUT_T,
	ALL_BUT_CT
};

enum class ETargetError
{
	NO_ERRORS,
	INVALID,
	CONNECTING,
	MULTIPLE_NAME_MATCHES,
	RANDOM,
	MULTIPLE,
	SELF,
	BOT,
	HUMAN,
	UNAUTHENTICATED,
	INSUFFICIENT_IMMUNITY_LEVEL,
	DEAD,
	ALIVE,
	TERRORIST,
	COUNTER_TERRORIST,
	SPECTATOR
};

class ZEPlayer;
struct ZRClass;
struct ZRModelEntry;

class ZEPlayerHandle
{
public:
	ZEPlayerHandle();
	ZEPlayerHandle(CPlayerSlot slot); // used for initialization inside ZEPlayer constructor
	ZEPlayerHandle(const ZEPlayerHandle& other);
	ZEPlayerHandle(ZEPlayer* pZEPlayer);

	bool IsValid() const { return static_cast<bool>(Get()); }

	uint32 GetIndex() const { return m_Index; }
	uint32 GetPlayerSlot() const { return m_Parts.m_PlayerSlot; }
	uint32 GetSerial() const { return m_Parts.m_Serial; }

	bool operator==(const ZEPlayerHandle& other) const { return other.m_Index == m_Index; }
	bool operator!=(const ZEPlayerHandle& other) const { return other.m_Index != m_Index; }
	bool operator==(ZEPlayer* pZEPlayer) const;
	bool operator!=(ZEPlayer* pZEPlayer) const;

	void operator=(const ZEPlayerHandle& other) { m_Index = other.m_Index; }
	void operator=(ZEPlayer* pZEPlayer) { Set(pZEPlayer); }
	void Set(ZEPlayer* pZEPlayer);

	ZEPlayer* Get() const;

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
	ZEPlayer(CPlayerSlot slot, bool m_bFakeClient = false) :
		m_slot(slot), m_bFakeClient(m_bFakeClient), m_Handle(slot)
	{
		m_bAuthenticated = false;
		m_iAdminFlags = 0;
		m_iAdminImmunity = 0;
		m_SteamID = nullptr;
		m_bGagged = false;
		m_bMuted = false;
		m_bEbanned = false;
		m_iHideDistance = 0;
		m_bConnected = false;
		m_iTotalDamage = 0;
		m_iTotalHits = 0;
		m_iTotalKills = 0;
		m_bVotedRTV = false;
		m_bVotedExtend = false;
		m_bIsInfected = false;
		m_flRTVVoteTime = -60.0f;
		m_flExtendVoteTime = 0;
		m_iFloodTokens = 0;
		m_flLastTalkTime = 0;
		m_bInGame = false;
		m_iMZImmunity = 0; // out of 100
		m_flNominateTime = -60.0f;
		m_iPlayerState = 1; // STATE_WELCOME is the initial state
		m_bIsLeader = false;
		m_handleMark = nullptr;
		m_colorLeader = Color(0, 0, 0, 0);
		m_colorTracer = Color(0, 0, 0, 0);
		m_colorGlow = Color(0, 0, 0, 0);
		m_colorBeacon = Color(0, 0, 0, 0);
		m_flLeaderVoteTime = -30.0f;
		m_flSpeedMod = 1.f;
		m_flMaxSpeed = 1.f;
		m_iLastInputs = IN_NONE;
		m_iLastInputTime = std::time(0);
		m_pActiveZRClass = nullptr;
		m_pActiveZRModel = nullptr;
		m_iButtonWatchMode = 0;
		m_iEntwatchHudMode = 0;
		m_bEntwatchClantags = true;
		m_colorEntwatchHud = Color(255, 255, 255, 255);
		m_flEntwatchHudX = -7.5f;
		m_flEntwatchHudY = -2.0f;
		m_flEntwatchHudSize = 60.0f;
	}

	~ZEPlayer()
	{
		CBarnLight* pFlashLight = m_hFlashLight.Get();

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
	void SetAdminFlags(uint64 iAdminFlags) { m_iAdminFlags = iAdminFlags; }
	void SetAdminImmunity(int iAdminImmunity) { m_iAdminImmunity = iAdminImmunity; }
	void SetPlayerSlot(CPlayerSlot slot) { m_slot = slot; }
	void SetMuted(bool muted) { m_bMuted = muted; }
	void SetGagged(bool gagged) { m_bGagged = gagged; }
	void SetEbanned(bool ebanned) { m_bEbanned = ebanned; }
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
	void SetFlashLight(CBarnLight* pLight) { m_hFlashLight.Set(pLight); }
	void SetBeaconParticle(CParticleSystem* pParticle) { m_hBeaconParticle.Set(pParticle); }
	void SetPlayerState(uint32 iPlayerState) { m_iPlayerState = iPlayerState; }
	void SetLeader(bool bIsLeader) { m_bIsLeader = bIsLeader; }
	void CreateMark(float fDuration, Vector vecOrigin);
	void SetLeaderColor(Color colorLeader) { m_colorLeader = colorLeader; }
	void SetTracerColor(Color colorTracer) { m_colorTracer = colorTracer; }
	void SetGlowColor(Color colorGlow) { m_colorGlow = colorGlow; }
	void SetBeaconColor(Color colorBeacon) { m_colorBeacon = colorBeacon; }
	void SetLeaderVoteTime(float flCurtime) { m_flLeaderVoteTime = flCurtime; }
	void SetGlowModel(CBaseModelEntity* pModel) { m_hGlowModel.Set(pModel); }
	void SetSpeedMod(float flSpeedMod) { m_flSpeedMod = flSpeedMod; }
	void SetLastInputs(uint64 iLastInputs) { m_iLastInputs = iLastInputs; }
	void UpdateLastInputTime() { m_iLastInputTime = std::time(0); }
	void SetMaxSpeed(float flMaxSpeed) { m_flMaxSpeed = flMaxSpeed; }
	void CycleButtonWatch();
	void ReplicateConVar(const char* pszName, const char* pszValue);
	void SetActiveZRClass(std::shared_ptr<ZRClass> pZRModel) { m_pActiveZRClass = pZRModel; }
	void SetActiveZRModel(std::shared_ptr<ZRModelEntry> pZRClass) { m_pActiveZRModel = pZRClass; }
	void SetEntwatchHudMode(int iMode);
	void SetEntwatchClangtags(bool bStatus);
	void SetEntwatchHud(CPointWorldText* pWorldText) { m_hEntwatchHud.Set(pWorldText); }
	void SetEntwatchHudColor(Color colorHud);
	void SetEntwatchHudPos(float x, float y);
	void SetEntwatchHudSize(float flSize);

	uint64 GetAdminFlags() { return m_iAdminFlags; }
	int GetAdminImmunity() { return m_iAdminImmunity; }
	bool IsMuted() { return m_bMuted; }
	bool IsGagged() { return m_bGagged; }
	bool IsEbanned() { return m_bEbanned; }
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
	CBarnLight* GetFlashLight() { return m_hFlashLight.Get(); }
	CParticleSystem* GetBeaconParticle() { return m_hBeaconParticle.Get(); }
	ZEPlayerHandle GetHandle() { return m_Handle; }
	uint32 GetPlayerState() { return m_iPlayerState; }
	bool IsLeader() { return m_bIsLeader; }
	Color GetLeaderColor() { return m_colorLeader; }
	Color GetTracerColor() { return m_colorTracer; }
	Color GetGlowColor() { return m_colorGlow; }
	Color GetBeaconColor() { return m_colorBeacon; }
	int GetLeaderVoteCount();
	bool HasPlayerVotedLeader(ZEPlayer* pPlayer);
	float GetLeaderVoteTime() { return m_flLeaderVoteTime; }
	CBaseModelEntity* GetGlowModel() { return m_hGlowModel.Get(); }
	float GetSpeedMod() { return m_flSpeedMod; }
	float GetMaxSpeed() { return m_flMaxSpeed; }
	uint64 GetLastInputs() { return m_iLastInputs; }
	std::time_t GetLastInputTime() { return m_iLastInputTime; }
	std::shared_ptr<ZRClass> GetActiveZRClass() { return m_pActiveZRClass; }
	std::shared_ptr<ZRModelEntry> GetActiveZRModel() { return m_pActiveZRModel; }
	int GetButtonWatchMode();
	CBaseViewModel* GetOrCreateCustomViewModel(CCSPlayerPawn* pPawn);
	int GetEntwatchHudMode();
	bool GetEntwatchClangtags() { return m_bEntwatchClantags; }
	CPointWorldText* GetEntwatchHud() { return m_hEntwatchHud.Get(); }
	Color GetEntwatchHudColor() { return m_colorEntwatchHud; }
	float GetEntwatchHudX() { return m_flEntwatchHudX; }
	float GetEntwatchHudY() { return m_flEntwatchHudY; }
	float GetEntwatchHudSize() { return m_flEntwatchHudSize; }

	void OnSpawn();
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
	void SetSteamIdAttribute();
	void CreateEntwatchHud();

private:
	bool m_bAuthenticated;
	bool m_bConnected;
	const CSteamID* m_UnauthenticatedSteamID;
	const CSteamID* m_SteamID;
	CPlayerSlot m_slot;
	bool m_bFakeClient;
	bool m_bMuted;
	bool m_bGagged;
	bool m_bEbanned;
	uint64 m_iAdminFlags;
	int m_iAdminImmunity;
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
	bool m_bIsLeader;
	CHandle<CBaseEntity> m_handleMark;
	Color m_colorLeader;
	Color m_colorTracer;
	Color m_colorGlow;
	Color m_colorBeacon;
	CUtlVector<ZEPlayerHandle> m_vecLeaderVotes;
	float m_flLeaderVoteTime;
	CHandle<CBaseModelEntity> m_hGlowModel;
	float m_flSpeedMod;
	float m_flMaxSpeed;
	uint64 m_iLastInputs;
	std::time_t m_iLastInputTime;
	std::shared_ptr<ZRClass> m_pActiveZRClass;
	std::shared_ptr<ZRModelEntry> m_pActiveZRModel;
	int m_iButtonWatchMode;
	CHandle<CPointWorldText> m_hEntwatchHud;
	int m_iEntwatchHudMode;
	bool m_bEntwatchClantags;
	Color m_colorEntwatchHud;
	float m_flEntwatchHudX;
	float m_flEntwatchHudY;
	float m_flEntwatchHudSize;
};

class CPlayerManager
{
public:
	CPlayerManager()
	{
		V_memset(m_vecPlayers, 0, sizeof(m_vecPlayers));
		m_nUsingStopSound = -1; // On by default
		m_nUsingSilenceSound = 0;
		m_nUsingStopDecals = -1; // On by default
		m_nUsingNoShake = 0;
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
	ZEPlayer* GetPlayerFromUserId(uint16 userid);
	ZEPlayer* GetPlayerFromSteamId(uint64 steamid);
	ETargetError GetPlayersFromString(CCSPlayerController* pPlayer, const char* pszTarget, int& iNumClients, int* clients, uint64 iBlockedFlags = NO_TARGET_BLOCKS);
	ETargetError GetPlayersFromString(CCSPlayerController* pPlayer, const char* pszTarget, int& iNumClients, int* clients, uint64 iBlockedFlags, ETargetType& nType);
	static std::string GetErrorString(ETargetError eType, int iSlot = 0);
	bool CanTargetPlayers(CCSPlayerController* pPlayer, const char* pszTarget, int& iNumClients, int* clients, uint64 iBlockedFlags = NO_TARGET_BLOCKS);
	bool CanTargetPlayers(CCSPlayerController* pPlayer, const char* pszTarget, int& iNumClients, int* clients, uint64 iBlockedFlags, ETargetType& nType);

	ZEPlayer* GetPlayer(CPlayerSlot slot);

	uint64 GetStopSoundMask() { return m_nUsingStopSound; }
	uint64 GetSilenceSoundMask() { return m_nUsingSilenceSound; }
	uint64 GetStopDecalsMask() { return m_nUsingStopDecals; }
	uint64 GetNoShakeMask() { return m_nUsingNoShake; }

	void SetPlayerStopSound(int slot, bool set);
	void SetPlayerSilenceSound(int slot, bool set);
	void SetPlayerStopDecals(int slot, bool set);
	void SetPlayerNoShake(int slot, bool set);

	void ResetPlayerFlags(int slot);

	bool IsPlayerUsingStopSound(int slot) { return m_nUsingStopSound & ((uint64)1 << slot); }
	bool IsPlayerUsingSilenceSound(int slot) { return m_nUsingSilenceSound & ((uint64)1 << slot); }
	bool IsPlayerUsingStopDecals(int slot) { return m_nUsingStopDecals & ((uint64)1 << slot); }
	bool IsPlayerUsingNoShake(int slot) { return m_nUsingNoShake & ((uint64)1 << slot); }

	void UpdatePlayerStates();
	int GetOnlinePlayerCount(bool bCountBots);

	STEAM_GAMESERVER_CALLBACK_MANUAL(CPlayerManager, OnValidateAuthTicket, ValidateAuthTicketResponse_t, m_CallbackValidateAuthTicketResponse);

private:
	ZEPlayer* m_vecPlayers[MAXPLAYERS];

	uint64 m_nUsingStopSound;
	uint64 m_nUsingSilenceSound;
	uint64 m_nUsingStopDecals;
	uint64 m_nUsingNoShake;
};

extern CPlayerManager* g_playerManager;

void PrecacheBeaconParticle(IEntityResourceManifest* pResourceManifest);