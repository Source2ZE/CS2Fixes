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
#include "steam/isteamugc.h"
#include "steam/steam_api_common.h"
#include "utlqueue.h"
#include "utlstring.h"
#include "utlvector.h"
#include <playerslot.h>
#include <string>
#include <vector>

// Nomination constants, used as return codes for nomination commands
#ifndef NOMINATION_CONSTANTS_H
	#define NOMINATION_CONSTANTS_H
namespace NominationReturnCodes
{
	static const int VOTE_STARTED = -100;
	static const int NOMINATION_DISABLED = -101;
	static const int NOMINATION_RESET = -102;
	static const int NOMINATION_RESET_FAILED = -103;
	static const int MAP_NOT_FOUND = -104;
	static const int MAP_MULTIPLE = -105;
	static const int MAP_DISABLED = -106;
	static const int MAP_CURRENT = -107;
	static const int MAP_COOLDOWN = -108;
	static const int MAP_MINPLAYERS = -109;
	static const int MAP_MAXPLAYERS = -110;
	static const int MAP_NOMINATED = -111;
} // namespace NominationReturnCodes
#endif

class CMapInfo
{
public:
	CMapInfo(const char* pszName, uint64 iWorkshopId, bool bIsEnabled, int iMinPlayers, int iMaxPlayers, int iBaseCooldown, int iCurrentCooldown)
	{
		V_strcpy(m_pszName, pszName);
		m_iWorkshopId = iWorkshopId;
		m_bIsEnabled = bIsEnabled;
		m_iBaseCooldown = iBaseCooldown;
		m_iCurrentCooldown = iCurrentCooldown;
		m_iMinPlayers = iMinPlayers;
		m_iMaxPlayers = iMaxPlayers;
	}

	const char* GetName() { return (const char*)m_pszName; };
	uint64 GetWorkshopId() const { return m_iWorkshopId; };
	bool IsEnabled() { return m_bIsEnabled; };
	int GetBaseCooldown() { return m_iBaseCooldown; };
	int GetCooldown() { return m_iCurrentCooldown; };
	void ResetCooldownToBase() { m_iCurrentCooldown = m_iBaseCooldown; };
	void DecrementCooldown() { m_iCurrentCooldown = MAX(0, (m_iCurrentCooldown - 1)); }
	int GetMinPlayers() { return m_iMinPlayers; };
	int GetMaxPlayers() { return m_iMaxPlayers; };

private:
	char m_pszName[64];
	uint64 m_iWorkshopId;
	bool m_bIsEnabled;
	int m_iMinPlayers;
	int m_iMaxPlayers;
	int m_iBaseCooldown;
	int m_iCurrentCooldown;
};

typedef struct
{
	const char* name;
	int index;
} MapIndexPair;

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
	int GetMapIndexFromString(const char* sMapString);
	int GetCooldownMap(int iMapIndex) { return m_vecMapList[iMapIndex].GetCooldown(); };
	void PutMapOnCooldown(int iMapIndex) { m_vecMapList[iMapIndex].ResetCooldownToBase(); };
	void DecrementAllMapCooldowns();
	void SetMaxNominatedMaps(int iMaxNominatedMaps) { m_iMaxNominatedMaps = iMaxNominatedMaps; };
	int GetMaxNominatedMaps() { return m_iMaxNominatedMaps; };
	std::pair<int, std::vector<int>> AddMapNomination(CPlayerSlot iPlayerSlot, const char* sMapSubstring);
	bool IsMapIndexEnabled(int iMapIndex);
	int GetTotalNominations(int iMapIndex);
	std::pair<int, std::vector<int>> ForceNextMap(const char* sMapSubstring);
	int GetMapListSize() { return m_vecMapList.Count(); };
	const char* GetMapName(int iMapIndex) { return m_vecMapList[iMapIndex].GetName(); };
	uint64 GetMapWorkshopId(int iMapIndex) { return m_vecMapList[iMapIndex].GetWorkshopId(); };
	void ClearPlayerInfo(int iSlot);
	bool IsVoteOngoing() { return m_bIsVoteOngoing; }
	bool IsIntermissionAllowed();
	bool IsMapListLoaded() { return m_bMapListLoaded; }
	CUtlStringList CreateWorkshopMapGroup();
	void QueueMapDownload(PublishedFileId_t iWorkshopId);
	void PrintDownloadProgress();
	void SetCurrentWorkshopMap(uint64 iCurrentWorkshopMap)
	{
		m_iCurrentWorkshopMap = iCurrentWorkshopMap;
		m_strCurrentMap = "";
	}
	void SetCurrentMap(std::string strCurrentMap)
	{
		m_iCurrentWorkshopMap = 0;
		m_strCurrentMap = strCurrentMap;
	}
	uint64 GetCurrentWorkshopMap() { return m_iCurrentWorkshopMap; }
	const char* GetCurrentMap() { return m_strCurrentMap.c_str(); }
	int GetDownloadQueueSize() { return m_DownloadQueue.Count(); }
	int GetCurrentMapIndex() { return m_iCurrentMapIndex; }
	void SetCurrentMapIndex(int iMapIndex) { m_iCurrentMapIndex = iMapIndex; }
	int GetMapMinPlayers(int iMapIndex) { return m_vecMapList[iMapIndex].GetMinPlayers(); }
	int GetMapMaxPlayers(int iMapIndex) { return m_vecMapList[iMapIndex].GetMaxPlayers(); }
	bool GetMapEnabledStatus(int iMapIndex) { return m_vecMapList[iMapIndex].IsEnabled(); }
	int GetDefaultMapCooldown() { return m_iDefaultMapCooldown; }
	void SetDefaultMapCooldown(int iMapCooldown) { m_iDefaultMapCooldown = iMapCooldown; }
	void ClearInvalidNominations();
	int GetForcedNextMap() { return m_iForcedNextMapIndex; }

private:
	int WinningMapIndex();
	bool UpdateWinningMap();
	void GetNominatedMapsForVote(CUtlVector<int>& vecChosenNominatedMaps);
	bool WriteMapCooldownsToFile();

	STEAM_GAMESERVER_CALLBACK_MANUAL(CMapVoteSystem, OnMapDownloaded, DownloadItemResult_t, m_CallbackDownloadItemResult);
	CUtlQueue<PublishedFileId_t> m_DownloadQueue;

	CUtlVector<CMapInfo> m_vecMapList;
	int m_arrPlayerNominations[MAXPLAYERS];
	int m_iForcedNextMapIndex = -1;
	int m_iDefaultMapCooldown = 10;
	int m_iMaxNominatedMaps = 10;
	int m_iRandomWinnerShift = 0;
	int m_arrPlayerVotes[MAXPLAYERS];
	int m_iCurrentMapIndex;
	bool m_bIsVoteOngoing = false;
	bool m_bMapListLoaded = false;
	bool m_bIntermissionStarted = false;
	uint64 m_iCurrentWorkshopMap = 0;
	std::string m_strCurrentMap = "";
};

extern CMapVoteSystem* g_pMapVoteSystem;