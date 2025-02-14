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

#include "KeyValues.h"
#include "common.h"
#include "entity/ccsplayercontroller.h"
#include "steam/isteamugc.h"
#include "steam/steam_api_common.h"
#include "utlqueue.h"
#include "utlstring.h"
#include "utlvector.h"
#undef snprintf
#include "vendor/nlohmann/json_fwd.hpp"
#include <playerslot.h>
#include <string>
#include <vector>

using ordered_json = nlohmann::ordered_json;

class CMap
{
public:
	CMap(std::string sName, uint64 iWorkshopId, bool bIsEnabled, int iMinPlayers, int iMaxPlayers, float fCustomCooldown, std::vector<std::string> vecGroups)
	{
		m_strName = sName;
		m_iWorkshopId = iWorkshopId;
		m_bIsEnabled = bIsEnabled;
		m_fCustomCooldown = fCustomCooldown;
		m_iMinPlayers = iMinPlayers;
		m_iMaxPlayers = iMaxPlayers;
		m_vecGroups = vecGroups;
	}

	const char* GetName() { return m_strName.c_str(); };
	uint64 GetWorkshopId() const { return m_iWorkshopId; };
	bool IsEnabled() { return m_bIsEnabled; };
	float GetCustomCooldown() { return m_fCustomCooldown; };
	int GetMinPlayers() { return m_iMinPlayers; };
	int GetMaxPlayers() { return m_iMaxPlayers; };
	std::vector<std::string> GetGroups() { return m_vecGroups; };
	bool HasGroup(std::string strGroup) { return std::find(m_vecGroups.begin(), m_vecGroups.end(), strGroup) != m_vecGroups.end(); };

private:
	std::string m_strName;
	uint64 m_iWorkshopId;
	bool m_bIsEnabled;
	int m_iMinPlayers;
	int m_iMaxPlayers;
	float m_fCustomCooldown;
	std::vector<std::string> m_vecGroups;
};

class CGroup
{
public:
	CGroup(std::string sName, bool bIsEnabled, float fCooldown)
	{
		m_strName = sName;
		m_bIsEnabled = bIsEnabled;
		m_fCooldown = fCooldown;
	}

	const char* GetName() { return m_strName.c_str(); };
	bool IsEnabled() { return m_bIsEnabled; };
	float GetCooldown() { return m_fCooldown; };

private:
	std::string m_strName;
	bool m_bIsEnabled;
	float m_fCooldown;
};

class CCooldown
{
public:
	CCooldown(std::string sMapName)
	{
		m_strMapName = sMapName;
		m_timeCooldown = 0;
		m_fPendingCooldown = 0.0f;
	}

	const char* GetMapName() { return m_strMapName.c_str(); };
	time_t GetTimeCooldown() { return m_timeCooldown; };
	void SetTimeCooldown(time_t timeCooldown) { m_timeCooldown = timeCooldown; };
	float GetPendingCooldown() { return m_fPendingCooldown; };
	void SetPendingCooldown(float fPendingCooldown) { m_fPendingCooldown = fPendingCooldown; };
	bool IsOnCooldown() { return GetCurrentCooldown() > 0.0f; }
	bool IsPending() { return m_fPendingCooldown > 0.0f && m_fPendingCooldown == GetCurrentCooldown(); };
	float GetCurrentCooldown();

private:
	std::string m_strMapName;
	time_t m_timeCooldown;
	float m_fPendingCooldown;
};

class CMapVoteSystem
{
public:
	CMapVoteSystem()
	{
		// Initialize the nomination / vote arrays to -1
		for (int i = 0; i < MAXPLAYERS; i++)
		{
			m_arrPlayerNominations[i] = -1;
			m_arrPlayerVotes[i] = -1;
		}
	}
	bool LoadMapList();
	void OnLevelInit(const char* pMapName);
	void StartVote();
	void FinishVote();
	bool RegisterPlayerVote(CPlayerSlot iPlayerSlot, int iVoteOption);
	std::vector<int> GetMapIndexesFromSubstring(const char* sMapSubstring);
	uint64 HandlePlayerMapLookup(CCSPlayerController* pController, const char* sMapSubstring, bool bAllowWorkshopID = false);
	int GetMapIndexFromString(const char* pszMapString);
	std::shared_ptr<CGroup> GetGroupFromString(const char* pszName);
	std::shared_ptr<CCooldown> GetMapCooldown(const char* pszMapName);
	std::shared_ptr<CCooldown> GetMapCooldown(int iMapIndex) { return GetMapCooldown(GetMapName(iMapIndex)); };
	std::string GetMapCooldownText(const char* pszMapName, bool bPlural);
	std::string GetMapCooldownText(int iMapIndex, bool bPlural) { return GetMapCooldownText(GetMapName(iMapIndex), bPlural); };
	float GetMapCustomCooldown(int iMapIndex) { return m_vecMapList[iMapIndex]->GetCustomCooldown(); };
	void PutMapOnCooldown(const char* pszMapName, float fCooldown = 0.0f);
	void SetMaxNominatedMaps(int iMaxNominatedMaps) { m_iMaxNominatedMaps = iMaxNominatedMaps; };
	int GetMaxNominatedMaps() { return m_iMaxNominatedMaps; };
	void SetMaxVoteMaps(int iMaxVoteMaps) { m_iMaxVoteMaps = iMaxVoteMaps; };
	int GetMaxVoteMaps() { return m_iMaxVoteMaps; };
	void AttemptNomination(CCSPlayerController* pController, const char* sMapSubstring);
	void PrintMapList(CCSPlayerController* pController);
	bool IsMapIndexEnabled(int iMapIndex);
	int GetTotalNominations(int iMapIndex);
	void ForceNextMap(CCSPlayerController* pController, const char* sMapSubstring);
	int GetMapListSize() { return m_vecMapList.size(); };
	const char* GetMapName(int iMapIndex) { return m_vecMapList[iMapIndex]->GetName(); };
	uint64 GetMapWorkshopId(int iMapIndex) { return m_vecMapList[iMapIndex]->GetWorkshopId(); };
	void ClearPlayerInfo(int iSlot);
	bool IsVoteOngoing() { return m_bIsVoteOngoing; }
	bool IsIntermissionAllowed();
	bool IsMapListLoaded() { return m_bMapListLoaded; }
	CUtlStringList CreateWorkshopMapGroup();
	void QueueMapDownload(PublishedFileId_t iWorkshopId);
	void PrintDownloadProgress();
	void SetCurrentMapName(const char* pszCurrentMap) { m_strCurrentMap = pszCurrentMap; }
	const char* GetCurrentMapName() { return m_strCurrentMap.c_str(); }
	void SetCurrentWorkshopMap(uint64 iCurrentWorkshopMap) { m_iCurrentWorkshopMap = iCurrentWorkshopMap; }
	uint64 GetCurrentWorkshopMap() { return m_iCurrentWorkshopMap; }
	int GetDownloadQueueSize() { return m_DownloadQueue.Count(); }
	int GetCurrentMapIndex() { return m_iCurrentMapIndex; }
	void UpdateCurrentMapIndex();
	int GetMapMinPlayers(int iMapIndex) { return m_vecMapList[iMapIndex]->GetMinPlayers(); }
	int GetMapMaxPlayers(int iMapIndex) { return m_vecMapList[iMapIndex]->GetMaxPlayers(); }
	bool GetMapEnabledStatus(int iMapIndex) { return m_vecMapList[iMapIndex]->IsEnabled(); }
	float GetDefaultMapCooldown() { return m_fDefaultMapCooldown; }
	void SetDefaultMapCooldown(float fMapCooldown) { m_fDefaultMapCooldown = fMapCooldown; }
	void ClearInvalidNominations();
	uint64 GetForcedNextMap() { return m_iForcedNextMap; }
	std::string GetForcedNextMapName() { return GetForcedNextMap() > GetMapListSize() ? std::to_string(GetForcedNextMap()) : GetMapName(GetForcedNextMap()); }
	bool ConvertMapListKVToJSON();
	std::unordered_map<int, int> GetNominatedMaps();
	void ApplyGameSettings(KeyValues* pKV);
	void OnLevelShutdown();
	std::vector<std::shared_ptr<CCooldown>> GetMapCooldowns() { return m_vecCooldowns; }
	std::string ConvertFloatToString(float fValue, int precision);
	std::string StringToLower(std::string sValue);
	void SetDisabledCooldowns(bool bValue) { g_bDisableCooldowns = bValue; } // Can be used by custom fork features, e.g. an auto-restart
	void ProcessGroupCooldowns();

private:
	int WinningMapIndex();
	bool UpdateWinningMap();
	std::vector<int> GetNominatedMapsForVote();
	bool WriteMapCooldownsToFile();

	STEAM_GAMESERVER_CALLBACK_MANUAL(CMapVoteSystem, OnMapDownloaded, DownloadItemResult_t, m_CallbackDownloadItemResult);
	CUtlQueue<PublishedFileId_t> m_DownloadQueue;

	std::vector<std::shared_ptr<CMap>> m_vecMapList;
	std::vector<std::shared_ptr<CGroup>> m_vecGroups;
	std::vector<std::shared_ptr<CCooldown>> m_vecCooldowns;
	int m_arrPlayerNominations[MAXPLAYERS];
	uint64 m_iForcedNextMap = -1; // Can be a map index or a workshop ID
	float m_fDefaultMapCooldown = 6.0f;
	int m_iMaxNominatedMaps = 10;
	int m_iMaxVoteMaps = 10;
	int m_iRandomWinnerShift = 0;
	int m_arrPlayerVotes[MAXPLAYERS];
	int m_iCurrentMapIndex = -1;
	bool m_bIsVoteOngoing = false;
	bool m_bMapListLoaded = false;
	bool m_bIntermissionStarted = false;
	uint64 m_iCurrentWorkshopMap = 0;
	std::string m_strCurrentMap = "MISSING_MAP";
	int m_iVoteSize = 0;
	bool g_bDisableCooldowns = false;
};

extern CMapVoteSystem* g_pMapVoteSystem;