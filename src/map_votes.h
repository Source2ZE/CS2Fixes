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

class CMapInfo
{
public:
	CMapInfo(const char* pszName, uint64 iWorkshopId, bool bIsEnabled, int iMinPlayers, int iMaxPlayers, int iBaseCooldown, int iCurrentCooldown)
	{
		m_strName = pszName;
		m_iWorkshopId = iWorkshopId;
		m_bIsEnabled = bIsEnabled;
		m_iBaseCooldown = iBaseCooldown;
		m_iCurrentCooldown = iCurrentCooldown;
		m_iMinPlayers = iMinPlayers;
		m_iMaxPlayers = iMaxPlayers;
	}

	const char* GetName() { return m_strName.c_str(); };
	uint64 GetWorkshopId() const { return m_iWorkshopId; };
	bool IsEnabled() { return m_bIsEnabled; };
	int GetBaseCooldown() { return m_iBaseCooldown; };
	int GetCooldown() { return m_iCurrentCooldown; };
	void ResetCooldownToBase() { m_iCurrentCooldown = m_iBaseCooldown; };
	void DecrementCooldown() { m_iCurrentCooldown = MAX(0, (m_iCurrentCooldown - 1)); }
	int GetMinPlayers() { return m_iMinPlayers; };
	int GetMaxPlayers() { return m_iMaxPlayers; };

private:
	std::string m_strName;
	uint64 m_iWorkshopId;
	bool m_bIsEnabled;
	int m_iMinPlayers;
	int m_iMaxPlayers;
	int m_iBaseCooldown;
	int m_iCurrentCooldown;
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
	int GetCooldownMap(int iMapIndex) { return m_vecMapList[iMapIndex]->GetCooldown(); };
	void PutMapOnCooldown(int iMapIndex) { m_vecMapList[iMapIndex]->ResetCooldownToBase(); };
	void DecrementAllMapCooldowns();
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
	void SetCurrentWorkshopMap(uint64 iCurrentWorkshopMap) { m_iCurrentWorkshopMap = iCurrentWorkshopMap; }
	uint64 GetCurrentWorkshopMap() { return m_iCurrentWorkshopMap; }
	int GetDownloadQueueSize() { return m_DownloadQueue.Count(); }
	int GetCurrentMapIndex() { return m_iCurrentMapIndex; }
	void UpdateCurrentMapIndex(const char* pszMapName);
	int GetMapMinPlayers(int iMapIndex) { return m_vecMapList[iMapIndex]->GetMinPlayers(); }
	int GetMapMaxPlayers(int iMapIndex) { return m_vecMapList[iMapIndex]->GetMaxPlayers(); }
	bool GetMapEnabledStatus(int iMapIndex) { return m_vecMapList[iMapIndex]->IsEnabled(); }
	int GetDefaultMapCooldown() { return m_iDefaultMapCooldown; }
	void SetDefaultMapCooldown(int iMapCooldown) { m_iDefaultMapCooldown = iMapCooldown; }
	void ClearInvalidNominations();
	uint64 GetForcedNextMap() { return m_iForcedNextMap; }
	std::string GetForcedNextMapName() { return GetForcedNextMap() > GetMapListSize() ? std::to_string(GetForcedNextMap()) : GetMapName(GetForcedNextMap()); }
	bool ConvertMapListKVToJSON();
	std::unordered_map<int, int> GetNominatedMaps();
	void ApplyGameSettings(KeyValues* pKV);

private:
	int WinningMapIndex();
	bool UpdateWinningMap();
	std::vector<int> GetNominatedMapsForVote();
	bool WriteMapCooldownsToFile();

	STEAM_GAMESERVER_CALLBACK_MANUAL(CMapVoteSystem, OnMapDownloaded, DownloadItemResult_t, m_CallbackDownloadItemResult);
	CUtlQueue<PublishedFileId_t> m_DownloadQueue;

	std::vector<std::shared_ptr<CMapInfo>> m_vecMapList;
	int m_arrPlayerNominations[MAXPLAYERS];
	uint64 m_iForcedNextMap = -1; // Can be a map index or a workshop ID
	int m_iDefaultMapCooldown = 10;
	int m_iMaxNominatedMaps = 10;
	int m_iMaxVoteMaps = 10;
	int m_iRandomWinnerShift = 0;
	int m_arrPlayerVotes[MAXPLAYERS];
	int m_iCurrentMapIndex = -1;
	bool m_bIsVoteOngoing = false;
	bool m_bMapListLoaded = false;
	bool m_bIntermissionStarted = false;
	uint64 m_iCurrentWorkshopMap = 0;
	int m_iVoteSize = 0;
};

extern CMapVoteSystem* g_pMapVoteSystem;