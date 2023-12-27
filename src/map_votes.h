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

#include "common.h"
#include <playerslot.h>
#include "utlstring.h"
#include "utlvector.h"


// Nomination constants, used as return codes for nomination commands
#ifndef NOMINATION_CONSTANTS_H
#define NOMINATION_CONSTANTS_H
namespace NominationReturnCodes
{
    static const int VOTE_STARTED = -100;
    static const int INVALID_INPUT = -101;
    static const int MAP_NOT_FOUND = -102;
    static const int INVALID_MAP = -103;
    static const int NOMINATION_DISABLED = -104;
}
#endif


class CMapInfo
{
public:
    CMapInfo(const char* pszName, uint64 iWorkshopId, bool bIsEnabled)
    {
        V_strcpy(m_pszName, pszName);
        m_iWorkshopId = iWorkshopId;
        m_bIsEnabled = bIsEnabled;
    }

    const char* GetName() { return (const char*)m_pszName; };
    uint64 GetWorkshopId() const { return m_iWorkshopId; };
    bool IsEnabled() { return m_bIsEnabled; };

private:
    char m_pszName[64];
    uint64 m_iWorkshopId;
    bool m_bIsEnabled;
};


class CMapVoteSystem
{
public:
    CMapVoteSystem()
    {
        LoadMapList();

        // Initialize the nomination / vote arrays to -1
        for (int i = 0; i < MAXPLAYERS; i++) {
            m_arrPlayerNominations[i] = -1;
            m_arrPlayerVotes[i] = -1;
        }
    }
    bool LoadMapList();
    void OnLevelInit(const char* pMapName);
    void StartVote();
    void FinishVote();
    void RegisterPlayerVote(CPlayerSlot iPlayerSlot, int iVoteOption);
    void SetMapCooldown(int iMapCooldown) { m_iMapCooldown = iMapCooldown; };
    int GetMapIndexFromSubstring(const char* sMapSubstring);
    int GetMapCooldown() { return m_iMapCooldown; };
    int GetMapsInCooldown() { return m_vecLastPlayedMapIndexes.Count(); }
    int GetCooldownMap(int iCooldownIndex) { return m_vecLastPlayedMapIndexes[iCooldownIndex]; };
    void PushMapIndexInCooldown(int iMapIndex) { m_vecLastPlayedMapIndexes.AddToTail(iMapIndex); };
    void SetMaxNominatedMaps(int iMaxNominatedMaps) { m_iMaxNominatedMaps = iMaxNominatedMaps; };
    int GetMaxNominatedMaps() { return m_iMaxNominatedMaps; };
    int AddMapNomination(CPlayerSlot iPlayerSlot, const char* sMapSubstring);
    bool IsMapIndexEnabled(int iMapIndex);
    int GetTotalNominations(int iMapIndex);
    int ForceNextMap(const char* sMapSubstring);
    int GetMapListSize() { return m_vecMapList.Count(); };
    const char* GetMapName(int iMapIndex) { return m_vecMapList[iMapIndex].GetName(); };
    void ClearPlayerInfo(int iSlot);

private:
    int WinningMapIndex();
    bool UpdateWinningMap();
    void GetNominatedMapsForVote(CUtlVector<int>& vecChosenNominatedMaps);

    CUtlVector<CMapInfo> m_vecMapList;
    CUtlVector<int> m_vecLastPlayedMapIndexes;
    int m_arrPlayerNominations[MAXPLAYERS];
    int m_iForcedNextMapIndex = -1;
    int m_iMapCooldown = 10;
    int m_iMaxNominatedMaps = 10;
    int m_iRandomWinnerShift = 0;
    int m_arrPlayerVotes[MAXPLAYERS];
    bool m_bIsVoteOngoing = false;
};

extern CMapVoteSystem* g_pMapVoteSystem;