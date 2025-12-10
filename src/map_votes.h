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
#undef snprintf
#include "vendor/nlohmann/json_fwd.hpp"
#include <deque>
#include <filesystem>
#include <optional>
#include <playerslot.h>
#include <string>
#include <vector>

class CMap;
using ordered_json = nlohmann::ordered_json;
using QueryCallback_t = std::function<void(std::shared_ptr<CMap>, CCSPlayerController*)>;

class CCooldown
{
public:
	CCooldown(std::string strMapName)
	{
		m_strMapName = strMapName;
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

class CMap : public std::enable_shared_from_this<CMap>
{
public:
	CMap(std::string strName, uint64 iWorkshopId, bool bMissingIdentifier = false, std::string strDisplayName = "", bool bIsEnabled = false, int iMinPlayers = 0, int iMaxPlayers = 64, float fCustomCooldown = 0.0f, std::optional<std::vector<std::string>> vecGroups = std::nullopt)
	{
		m_strName = strName;
		m_iWorkshopId = iWorkshopId;
		m_bMissingIdentifier = bMissingIdentifier;
		m_strDisplayName = strDisplayName;
		m_bIsEnabled = bIsEnabled;
		m_fCustomCooldown = fCustomCooldown;
		m_iMinPlayers = iMinPlayers;
		m_iMaxPlayers = iMaxPlayers;

		if (vecGroups.has_value())
			m_vecGroups = vecGroups.value();
	}

	const char* GetName() { return m_strName.c_str(); };
	const char* GetDisplayName() { return m_strDisplayName.empty() ? m_strName.c_str() : m_strDisplayName.c_str(); };
	uint64 GetWorkshopId() const { return m_iWorkshopId; };
	bool IsMissingIdentifier() { return m_bMissingIdentifier; }
	bool IsEnabled() { return m_bIsEnabled; };
	float GetCustomCooldown() { return m_fCustomCooldown; };
	int GetMinPlayers() { return m_iMinPlayers; };
	int GetMaxPlayers() { return m_iMaxPlayers; };
	std::vector<std::string> GetGroups() { return m_vecGroups; };
	bool HasGroup(std::string strGroup) { return std::find(m_vecGroups.begin(), m_vecGroups.end(), strGroup) != m_vecGroups.end(); };

	bool IsAvailable();
	bool Load();
	std::shared_ptr<CCooldown> GetCooldown();
	std::string GetCooldownText(bool bPlural);

	bool operator==(CMap& other)
	{
		return (this->GetName()[0] != '\0' && !V_strcasecmp(this->GetName(), other.GetName())) || (this->GetWorkshopId() != 0 && this->GetWorkshopId() == other.GetWorkshopId());
	}

private:
	std::string m_strName;
	std::string m_strDisplayName;
	uint64 m_iWorkshopId;
	bool m_bMissingIdentifier;
	bool m_bIsEnabled;
	int m_iMinPlayers;
	int m_iMaxPlayers;
	float m_fCustomCooldown;
	std::vector<std::string> m_vecGroups;
};

class CGroup
{
public:
	CGroup(std::string strName, bool bIsEnabled, float fCooldown)
	{
		m_strName = strName;
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

// Implementation is a bit hardcoded for HandlePlayerMapLookup use
class CWorkshopDetailsQuery : public std::enable_shared_from_this<CWorkshopDetailsQuery>
{
public:
	CWorkshopDetailsQuery(UGCQueryHandle_t hQuery, uint64 iWorkshopId, CCSPlayerController* pController, QueryCallback_t callbackSuccess) :
		m_hQuery(hQuery), m_iWorkshopId(iWorkshopId), m_callbackSuccess(callbackSuccess)
	{
		if (pController)
		{
			m_bConsole = false;
			m_hController = pController->GetHandle();
		}
		else
		{
			m_bConsole = true;
		}
	}

	static std::shared_ptr<CWorkshopDetailsQuery> Create(uint64 iWorkshopId, CCSPlayerController* pController, QueryCallback_t callbackSuccess);

private:
	void OnQueryCompleted(SteamUGCQueryCompleted_t* pCompletedQuery, bool bFailed);

	UGCQueryHandle_t m_hQuery;
	CCallResult<CWorkshopDetailsQuery, SteamUGCQueryCompleted_t> m_CallResult;
	uint64 m_iWorkshopId;
	bool m_bConsole;
	CHandle<CCSPlayerController> m_hController;
	QueryCallback_t m_callbackSuccess;
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
	bool LoadCooldowns();
	void OnLevelInit(const char* pszMapName);
	void StartVote();
	void FinishVote();
	bool RegisterPlayerVote(CPlayerSlot iPlayerSlot, int iVoteOption);
	std::vector<std::shared_ptr<CMap>> GetMapsFromSubstring(const char* pszMapSubstring);
	void HandlePlayerMapLookup(CCSPlayerController* pController, std::string strMapSubstring, bool bAdmin, QueryCallback_t callbackSuccess);
	std::shared_ptr<CMap> GetMapFromString(const char* pszMapString);
	std::shared_ptr<CGroup> GetGroupFromString(const char* pszName);
	std::shared_ptr<CCooldown> GetMapCooldown(const char* pszMapName);
	std::string GetMapCooldownText(const char* pszMapName, bool bPlural);
	void PutMapOnCooldown(const char* pszMapName, float fCooldown = 0.0f);
	void AttemptNomination(CCSPlayerController* pController, const char* pszMapSubstring);
	void PrintMapList(CCSPlayerController* pController);
	int GetTotalNominations(int iMapIndex);
	void ForceNextMap(CCSPlayerController* pController, const char* pszMapSubstring);
	std::vector<std::shared_ptr<CMap>> GetMapList() { return m_vecMapList; }
	int GetMapListSize() { return m_vecMapList.size(); };
	std::shared_ptr<CMap> GetMapByIndex(int iMapIndex) { return m_vecMapList[iMapIndex]; }
	std::pair<int, std::shared_ptr<CMap>> GetMapInfoByIdentifiers(const char* pszMapName = "", uint64 iWorkshopId = 0);
	const char* GetMapName(int iMapIndex) { return m_vecMapList[iMapIndex]->GetName(); };
	void ClearPlayerInfo(int iSlot);
	bool IsVoteOngoing() { return m_bIsVoteOngoing; }
	bool IsIntermissionAllowed(bool bCheckOnly = true);
	bool IsMapListLoaded() { return m_bMapListLoaded; }
	CUtlStringList CreateWorkshopMapGroup();
	void QueueMapDownload(PublishedFileId_t iWorkshopId);
	void PrintDownloadProgress();
	std::shared_ptr<CMap> GetCurrentMap() { return m_pCurrentMap; }
	void SetCurrentMap(std::shared_ptr<CMap> pCurrentMap) { m_pCurrentMap = pCurrentMap; }
	int GetDownloadQueueSize() { return m_DownloadQueue.size(); }
	void ClearInvalidNominations();
	std::shared_ptr<CMap> GetForcedNextMap() { return m_pForcedNextMap; }
	void SetForcedNextMap(std::shared_ptr<CMap> pForcedNextMap) { m_pForcedNextMap = pForcedNextMap; }
	std::unordered_map<int, int> GetNominatedMaps();
	void ApplyGameSettings(KeyValues* pKV);
	void OnLevelShutdown();
	std::vector<std::shared_ptr<CCooldown>> GetMapCooldowns() { return m_vecCooldowns; }
	std::string ConvertFloatToString(float fValue, int precision);
	void SetDisabledCooldowns(bool bValue) { g_bDisableCooldowns = bValue; } // Can be used by custom fork features, e.g. an auto-restart
	void ProcessGroupCooldowns();
	bool ReloadMapList(bool bReloadMap = true);
	bool ConvertCooldownsKVToJSON();
	void AddWorkshopDetailsQuery(std::shared_ptr<CWorkshopDetailsQuery> pQuery) { m_vecWorkshopDetailsQueries.push_back(pQuery); }
	void RemoveWorkshopDetailsQuery(std::shared_ptr<CWorkshopDetailsQuery> pQuery) { m_vecWorkshopDetailsQueries.erase(std::remove(m_vecWorkshopDetailsQueries.begin(), m_vecWorkshopDetailsQueries.end(), pQuery), m_vecWorkshopDetailsQueries.end()); }
	void SetPlayerNomination(int iPlayerSlot, int iMapIndex) { m_arrPlayerNominations[iPlayerSlot] = iMapIndex; }

private:
	int WinningMapIndex();
	bool UpdateWinningMap();
	std::vector<int> GetNominatedMapsForVote();
	bool WriteMapCooldownsToFile();

	STEAM_GAMESERVER_CALLBACK_MANUAL(CMapVoteSystem, OnMapDownloaded, DownloadItemResult_t, m_CallbackDownloadItemResult);
	std::deque<PublishedFileId_t> m_DownloadQueue;

	std::vector<std::shared_ptr<CMap>> m_vecMapList;
	std::vector<std::shared_ptr<CGroup>> m_vecGroups;
	std::vector<std::shared_ptr<CCooldown>> m_vecCooldowns;
	int m_arrPlayerNominations[MAXPLAYERS];
	std::shared_ptr<CMap> m_pForcedNextMap;
	int m_iRandomWinnerShift = 0;
	int m_arrPlayerVotes[MAXPLAYERS];
	bool m_bIsVoteOngoing = false;
	bool m_bMapListLoaded = false;
	bool m_bIntermissionStarted = false;
	std::shared_ptr<CMap> m_pCurrentMap;
	int m_iVoteSize = 0;
	bool g_bDisableCooldowns = false;
	std::filesystem::file_time_type m_timeMapListModified = std::filesystem::file_time_type::min();
	std::weak_ptr<CTimer> m_pDownloadProgressTimer;
	std::weak_ptr<CTimer> m_pRateLimitedDownloadTimer;
	std::vector<std::shared_ptr<CWorkshopDetailsQuery>> m_vecWorkshopDetailsQueries;
};

extern CMapVoteSystem* g_pMapVoteSystem;