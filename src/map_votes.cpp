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

#include "map_votes.h"
#include "commands.h"
#include "common.h"
#include "ctimer.h"
#include "eventlistener.h"
#include "entity/cgamerules.h"
#include "idlemanager.h"
#include "strtools.h"
#include <stdio.h>
#include "playermanager.h"
#include <playerslot.h>
#include "utlstring.h"
#include "utlvector.h"
#include "votemanager.h"
#include "steam/steam_gameserver.h"

extern CGlobalVars *gpGlobals;
extern CCSGameRules* g_pGameRules;
extern IVEngineServer2* g_pEngineServer2;
extern CSteamGameServerAPIContext g_steamAPI;
extern IGameTypes* g_pGameTypes;
extern CIdleSystem* g_pIdleSystem;

CMapVoteSystem* g_pMapVoteSystem = nullptr;

CON_COMMAND_CHAT_FLAGS(reload_map_list, "- Reload map list, also reloads current map on completion", ADMFLAG_ROOT)
{
	if (!g_bVoteManagerEnable)
		return;

	if (g_pMapVoteSystem->GetDownloadQueueSize() != 0)
	{
		Message("Please wait for current map downloads to finish before loading map list again\n");
		return;
	}

	g_pMapVoteSystem->LoadMapList();

	// A CUtlStringList param is also expected, but we build it in our CreateWorkshopMapGroup pre-hook anyways
	CALL_VIRTUAL(void, g_GameConfig->GetOffset("IGameTypes_CreateWorkshopMapGroup"), g_pGameTypes, "workshop");

	// Updating the mapgroup requires reloading the map for everything to load properly
	char sChangeMapCmd[128] = "";

	if (g_pMapVoteSystem->GetCurrentWorkshopMap() != 0)
		V_snprintf(sChangeMapCmd, sizeof(sChangeMapCmd), "host_workshop_map %llu", g_pMapVoteSystem->GetCurrentWorkshopMap());
	else if (g_pMapVoteSystem->GetCurrentMap()[0] != '\0')
		V_snprintf(sChangeMapCmd, sizeof(sChangeMapCmd), "map %s", g_pMapVoteSystem->GetCurrentMap());

	g_pEngineServer2->ServerCommand(sChangeMapCmd);
	Message("Map list reloaded\n");
}

CON_COMMAND_F(cs2f_vote_maps_cooldown, "Number of maps to wait until a map can be voted / nominated again i.e. cooldown.", FCVAR_LINKED_CONCOMMAND | FCVAR_SPONLY)
{
	if (!g_pMapVoteSystem) {
		Message("The map vote subsystem is not enabled.\n");
		return;
	}

	if (args.ArgC() < 2)
		Message("%s %d\n", args[0], g_pMapVoteSystem->GetMapCooldown());
	else {
		int iCurrentCooldown = g_pMapVoteSystem->GetMapCooldown();
		g_pMapVoteSystem->SetMapCooldown(V_StringToInt32(args[1], iCurrentCooldown));
	}
}

CON_COMMAND_F(cs2f_vote_max_nominations, "Number of nominations to include per vote, out of a maximum of 10.", FCVAR_LINKED_CONCOMMAND | FCVAR_SPONLY)
{
	if (!g_pMapVoteSystem) {
		Message("The map vote subsystem is not enabled.\n");
		return;
	}

	if (args.ArgC() < 2)
		Message("%s %d\n", args[0], g_pMapVoteSystem->GetMaxNominatedMaps());
	else {
		int iMaxNominatedMaps = g_pMapVoteSystem->GetMaxNominatedMaps();
		g_pMapVoteSystem->SetMaxNominatedMaps(V_StringToInt32(args[1], iMaxNominatedMaps));
	}
}

CON_COMMAND_CHAT_FLAGS(setnextmap, "[mapname] - Force next map (empty to clear forced next map)", ADMFLAG_CHANGEMAP)
{
	if (!g_bVoteManagerEnable)
		return;

	bool bIsClearingForceNextMap = args.ArgC() < 2;
	int iResponse = g_pMapVoteSystem->ForceNextMap(bIsClearingForceNextMap ? "" : args[1]);
	if (bIsClearingForceNextMap) {
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "You reset the forced next map.");
	}
	else {
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "You have forced the next map to %s.", g_pMapVoteSystem->GetMapName(iResponse));
	}
}

static int __cdecl OrderStringsLexicographically(const char* const* a, const char* const* b)
{
	return V_strcasecmp(*a, *b);
}

CON_COMMAND_CHAT_FLAGS(nominate, "[mapname] - Nominate a map (empty to clear nomination)", ADMFLAG_NONE)
{
	if (!g_bVoteManagerEnable || !player)
		return;

	int iResponse = g_pMapVoteSystem->AddMapNomination(player->GetPlayerSlot(), args.ArgC() < 2 ? "" : args[1]);
	ZEPlayer* pPlayer = g_playerManager->GetPlayer(player->GetPlayerSlot());

	if (!pPlayer)
		return;

	switch (iResponse) {
		case NominationReturnCodes::VOTE_STARTED:
			ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Could not nominate as the vote has already started.");
			break;
		case NominationReturnCodes::INVALID_INPUT:
			ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Could not nominate as the input is invalid. Usage: !nominate <map substring>");
			break;
		case NominationReturnCodes::MAP_NOT_FOUND:
			ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Could not nominate as no map matched '%s'.", args[1]);
			break;
		case NominationReturnCodes::INVALID_MAP:
			ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "The map matching '%s' is not available for nomination.", args[1]);
			break;
		case NominationReturnCodes::NOMINATION_DISABLED:
			ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Nomination is currently disabled.");
			break;
		case NominationReturnCodes::NOMINATION_RESET:
			ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Your nomination was reset.");
			break;
		case NominationReturnCodes::NOMINATION_RESET_FAILED:
		{
			ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "The list of all maps will be shown in console.");
			ClientPrint(player, HUD_PRINTCONSOLE, "The list of all maps is:");
			CUtlVector<const char*> vecMapNames;

			for (int i = 0; i < g_pMapVoteSystem->GetMapListSize(); i++)
				vecMapNames.AddToTail(g_pMapVoteSystem->GetMapName(i));

			vecMapNames.Sort(OrderStringsLexicographically);

			// TODO: print cooldown time here too (after rewrite)
			FOR_EACH_VEC(vecMapNames, i)
				ClientPrint(player, HUD_PRINTCONSOLE, "- %s", vecMapNames[i]);

			break;
		}
		default:
			if (pPlayer->GetNominateTime() + 60.0f > gpGlobals->curtime)
			{
				int iRemainingTime = (int)(pPlayer->GetNominateTime() + 60.0f - gpGlobals->curtime);
				ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Wait %i seconds before you can nominate again.", iRemainingTime);
				return;
			}
			else
			{
				const char* sPlayerName = player->GetPlayerName();
				const char* sMapName = g_pMapVoteSystem->GetMapName(iResponse);
				int iNumNominations = g_pMapVoteSystem->GetTotalNominations(iResponse);
				ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX " \x06%s \x01was nominated by %s. It now has %d nominations.", sMapName, sPlayerName, iNumNominations);
				pPlayer->SetNominateTime(gpGlobals->curtime);
			}
	}
}

CON_COMMAND_CHAT(nomlist, "- List the list of nominations")
{
	if (!g_bVoteManagerEnable)
		return;

	ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Current nominations:");
	for (int i = 0; i < g_pMapVoteSystem->GetMapListSize(); i++) {
		if (!g_pMapVoteSystem->IsMapIndexEnabled(i)) continue;
		int iNumNominations = g_pMapVoteSystem->GetTotalNominations(i);
		if (iNumNominations == 0) continue;
		const char* sMapName = g_pMapVoteSystem->GetMapName(i);
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "- %s (%d times)\n", sMapName, iNumNominations);
	}
}

// TODO: also merge this into nominate after cooldown system is rewritten
CON_COMMAND_CHAT(mapcooldowns, "- List the maps currently in cooldown")
{
	if (!g_bVoteManagerEnable)
		return;

	ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "The list of maps in cooldown will be shown in console.");
	ClientPrint(player, HUD_PRINTCONSOLE, "The list of maps in cooldown is:");
	int iMapsInCooldown = g_pMapVoteSystem->GetMapsInCooldown();
	for (int i = iMapsInCooldown - 1; i >= 0; i--) {
		int iMapIndex = g_pMapVoteSystem->GetCooldownMap(i);
		const char* sMapName = g_pMapVoteSystem->GetMapName(iMapIndex);
		ClientPrint(player, HUD_PRINTCONSOLE, "- %s (%d maps ago)", sMapName, iMapsInCooldown - i);
	}
}

GAME_EVENT_F(cs_win_panel_match)
{
	if (g_bVoteManagerEnable && !g_pMapVoteSystem->IsVoteOngoing())
		g_pMapVoteSystem->StartVote();
}

GAME_EVENT_F(endmatch_mapvote_selecting_map)
{
	if (g_bVoteManagerEnable)
		g_pMapVoteSystem->FinishVote();
}

bool CMapVoteSystem::IsMapIndexEnabled(int iMapIndex)
{
	if (iMapIndex >= m_vecMapList.Count() || iMapIndex < 0) return false;
	if (m_vecLastPlayedMapIndexes.HasElement(iMapIndex)) return false;
	return m_vecMapList[iMapIndex].IsEnabled();
}

void CMapVoteSystem::OnLevelInit(const char* pMapName)
{
	if (!g_bVoteManagerEnable)
		return;

	m_bIsVoteOngoing = false;
	m_bIntermissionStarted = false;

	// Delay one tick to override any .cfg's
	new CTimer(0.02f, false, true, []()
	{
		g_pEngineServer2->ServerCommand("mp_match_end_changelevel 0");

		return -1.0f;
	});

	int iLastCooldownIndex = GetMapsInCooldown() - 1;
	int iInitMapIndex = GetMapIndexFromSubstring(pMapName);
	if (iLastCooldownIndex >= 0 && iInitMapIndex >= 0 && GetCooldownMap(iLastCooldownIndex) != iInitMapIndex) {
		PushMapIndexInCooldown(iInitMapIndex);
	}
}

void CMapVoteSystem::StartVote() 
{
	m_bIsVoteOngoing = true;

	g_pIdleSystem->PauseIdleChecks();

	// If we are forcing a map, just set all vote options to that map
	if (m_iForcedNextMapIndex != -1) {
		for (int i = 0; i < 10; i++) {
			g_pGameRules->m_nEndMatchMapGroupVoteOptions[i] = m_iForcedNextMapIndex;
			return;
		}
	}

	// Seed the randomness for the event
	m_iRandomWinnerShift = rand();

	// Reset the player vote counts as the vote just started
	for (int i = 0; i < gpGlobals->maxClients; i++) {
		m_arrPlayerVotes[i] = -1;
	}

	// Select random maps not in cooldown, not disabled, and not nominated
	CUtlVector<int> vecPossibleMaps;
	CUtlVector<int> vecIncludedMaps;
	GetNominatedMapsForVote(vecIncludedMaps);
	for (int i = 0; i < m_vecMapList.Count(); i++) {
		if (!IsMapIndexEnabled(i)) continue;
		if (vecIncludedMaps.HasElement(i)) continue;
		vecPossibleMaps.AddToTail(i);
	}

	// Print all available maps out to console
	FOR_EACH_VEC(vecPossibleMaps, i) {
		int iPossibleMapIndex = vecPossibleMaps[i];
		Message("The %d-th possible map index %d is %s\n", i, iPossibleMapIndex, m_vecMapList[iPossibleMapIndex].GetName());
	}

	// Set the maps in the vote: merge nominated and possible maps, then randomly sort
	int iNumMapsInVote = vecPossibleMaps.Count() + vecIncludedMaps.Count();
	if (iNumMapsInVote >= 10) iNumMapsInVote = 10;
	while (vecIncludedMaps.Count() < iNumMapsInVote && vecPossibleMaps.Count() > 0) {
		int iMapToAdd = vecPossibleMaps[rand() % vecPossibleMaps.Count()];
		vecIncludedMaps.AddToTail(iMapToAdd);
		vecPossibleMaps.FindAndRemove(iMapToAdd);
	}

	// Randomly sort the chosen maps
	for (int i = 0; i < 10; i++) {
		if (i < iNumMapsInVote) {
			int iMapToAdd = vecIncludedMaps[rand() % vecIncludedMaps.Count()];
			g_pGameRules->m_nEndMatchMapGroupVoteOptions[i] = iMapToAdd;
			vecIncludedMaps.FindAndRemove(iMapToAdd);
		} else {
			g_pGameRules->m_nEndMatchMapGroupVoteOptions[i] = -1;
		}
	}

	// Print the maps chosen in the vote to console
	for (int i = 0; i < iNumMapsInVote; i++) {
		int iMapIndex = g_pGameRules->m_nEndMatchMapGroupVoteOptions[i];
		Message("The %d-th chosen map index %d is %s\n", i, iMapIndex, m_vecMapList[iMapIndex].GetName());
	}

	// Start the end-of-vote timer to finish the vote
	ConVar* cvar = g_pCVar->GetConVar(g_pCVar->FindConVar("mp_endmatch_votenextleveltime"));
	float flVoteTime = *(float *)&cvar->values;
	new CTimer(flVoteTime, false, true, []() {
		g_pMapVoteSystem->FinishVote();
		return -1.0;
	});
}

int CMapVoteSystem::GetTotalNominations(int iMapIndex)
{
	int iNumNominations = 0;
	for (int i = 0; i < gpGlobals->maxClients; i++) {
		auto pController = CCSPlayerController::FromSlot(i);
		if (pController && pController->IsConnected() && m_arrPlayerNominations[i] == iMapIndex) {
			iNumNominations++;
		}
	}
	return iNumNominations;
}

void CMapVoteSystem::FinishVote()
{
	if (!m_bIsVoteOngoing) return;

	// Clean up the ongoing voting state and variables
	m_bIsVoteOngoing = false;
	m_iForcedNextMapIndex = -1;

	// Get the winning map
	bool bIsNextMapVoted = UpdateWinningMap();
	int iNextMapVoteIndex = WinningMapIndex();

	// If we are forcing the map, show different text
	bool bIsNextMapForced = m_iForcedNextMapIndex != -1;
	if (bIsNextMapForced) {
		iNextMapVoteIndex = 0;
		g_pGameRules->m_nEndMatchMapGroupVoteOptions[0] = m_iForcedNextMapIndex;
		g_pGameRules->m_nEndMatchMapVoteWinner = iNextMapVoteIndex;
	}

	// Print out the winning map
	if (iNextMapVoteIndex < 0) iNextMapVoteIndex = -1;
	g_pGameRules->m_nEndMatchMapVoteWinner = iNextMapVoteIndex;
	int iWinningMap = g_pGameRules->m_nEndMatchMapGroupVoteOptions[iNextMapVoteIndex];
	if (bIsNextMapVoted) {
		ClientPrintAll(HUD_PRINTTALK, "The vote has ended. \x06%s\x01 will be the next map!\n", GetMapName(iWinningMap));
	}
	else if (bIsNextMapForced) {
		ClientPrintAll(HUD_PRINTTALK, "The vote was overriden. \x06%s\x01 will be the next map!\n", GetMapName(iWinningMap));
	}
	else {
		ClientPrintAll(HUD_PRINTTALK, "No map was chosen. \x06%s\x01 will be the next map!\n", GetMapName(iWinningMap));
	}

	// Print vote result information: how many votes did each map get?
	int arrMapVotes[10] = { 0 };
	ClientPrintAll(HUD_PRINTCONSOLE, "Map vote result --- total votes per map:\n");
	for (int i = 0; i < gpGlobals->maxClients; i++) {
		auto pController = CCSPlayerController::FromSlot(i);
		int iPlayerVotedIndex = m_arrPlayerVotes[i];
		if (pController && pController->IsConnected() && iPlayerVotedIndex >= 0) {
			arrMapVotes[iPlayerVotedIndex]++;
		}
	}
	for (int i = 0; i < 10; i++) {
		int iMapIndex = g_pGameRules->m_nEndMatchMapGroupVoteOptions[i];
		const char* sIsWinner = (i == iNextMapVoteIndex) ? "(WINNER)" : "";
		ClientPrintAll(HUD_PRINTCONSOLE, "- %s got %d votes\n", GetMapName(iMapIndex), arrMapVotes[i]);
	}

	// Store the winning map in the vector of played maps and pop until desired cooldown
	PushMapIndexInCooldown(iWinningMap);
	while (m_vecLastPlayedMapIndexes.Count() > m_iMapCooldown) {
		m_vecLastPlayedMapIndexes.Remove(0);
	}

	// Do the final clean-up
	for (int i = 0; i < gpGlobals->maxClients; i++)
		ClearPlayerInfo(i);

	// Wait a second and force-change the map
	new CTimer(1.0, false, true, [iWinningMap]() {
		char sChangeMapCmd[128];
		uint64 workshopId = g_pMapVoteSystem->GetMapWorkshopId(iWinningMap);

		if (workshopId == 0)
			V_snprintf(sChangeMapCmd, sizeof(sChangeMapCmd), "map %s", g_pMapVoteSystem->GetMapName(iWinningMap));
		else
			V_snprintf(sChangeMapCmd, sizeof(sChangeMapCmd), "host_workshop_map %llu", workshopId);

		g_pEngineServer2->ServerCommand(sChangeMapCmd);
		return -1.0;
	});
}

bool CMapVoteSystem::RegisterPlayerVote(CPlayerSlot iPlayerSlot, int iVoteOption)
{
	CCSPlayerController* pController = CCSPlayerController::FromSlot(iPlayerSlot);
	if (!pController || !m_bIsVoteOngoing) return false;
	if (iVoteOption < 0 || iVoteOption >= 10) return false;

	// Filter out votes on invalid maps
	int iMapIndexToVote = g_pGameRules->m_nEndMatchMapGroupVoteOptions[iVoteOption];
	if (iMapIndexToVote < 0 || iMapIndexToVote >= m_vecMapList.Count()) return false;

	// Set the vote for the player
	int iSlot = pController->GetPlayerSlot();
	m_arrPlayerVotes[iSlot] = iVoteOption;

	// Log vote to console
	const char* sMapName = GetMapName(iMapIndexToVote);
	Message("Adding vote to map %i (%s) for player %s (slot %i).\n", iVoteOption, sMapName, pController->GetPlayerName(), iSlot);

	// Update the winning map for every player vote
	UpdateWinningMap();

	// Vote was counted
	return true;
}

bool CMapVoteSystem::UpdateWinningMap()
{
	int iWinningMapIndex = WinningMapIndex();
	if (iWinningMapIndex >= 0) {
		g_pGameRules->m_nEndMatchMapVoteWinner = iWinningMapIndex;
		return true;
	}
	return false;
}

int CMapVoteSystem::WinningMapIndex()
{
	// Count the votes of every player
	int arrMapVotes[10] = {0};
	for (int i = 0; i < gpGlobals->maxClients; i++) {
		auto pController = CCSPlayerController::FromSlot(i);
		if (pController && pController->IsConnected() && m_arrPlayerVotes[i] >= 0) {
			arrMapVotes[m_arrPlayerVotes[i]]++;
		}
	}

	// Identify the max. number of votes
	int iMaxVotes = 0;
	for (int i = 0; i < 10; i++) {
		iMaxVotes = (arrMapVotes[i] > iMaxVotes) ? arrMapVotes[i] : iMaxVotes;
	}

	// Identify how many maps are tied with the max number of votes    
	int iMapsWithMaxVotes = 0;
	for (int i = 0; i < 10; i++) {
		if (arrMapVotes[i] == iMaxVotes) iMapsWithMaxVotes++;
	}

	// Break ties: 'random' map with the most votes
	int iWinningMapTieBreak = m_iRandomWinnerShift % iMapsWithMaxVotes;
	int iWinningMapCount = 0;
	for (int i = 0; i < 10; i++) {
		if (arrMapVotes[i] == iMaxVotes) {
			if (iWinningMapCount == iWinningMapTieBreak) return i;
			iWinningMapCount++;
		}
	}
	return -1;
}

void CMapVoteSystem::GetNominatedMapsForVote(CUtlVector<int>& vecChosenNominatedMaps)
{
	int iNumDistinctMaps = 0;
	CUtlVector<int> vecAvailableNominatedMaps;
	for (int i = 0; i < gpGlobals->maxClients; i++) {
		int iNominatedMapIndex = m_arrPlayerNominations[i];

		// Introduce nominated map indexes and count the total number
		if (iNominatedMapIndex != -1) {
			if (!vecAvailableNominatedMaps.HasElement(iNominatedMapIndex)) {
				iNumDistinctMaps++;
			}
			vecAvailableNominatedMaps.AddToTail(iNominatedMapIndex);
		}
	}

	// Randomly select maps out of the set of nominated maps
	// weighting by number of nominations, and returning a random order
	int iMapsToIncludeInNominate = (iNumDistinctMaps < m_iMaxNominatedMaps) ? iNumDistinctMaps : m_iMaxNominatedMaps;
	while (vecChosenNominatedMaps.Count() < iMapsToIncludeInNominate) {
		int iMapToAdd = vecAvailableNominatedMaps[rand() % vecAvailableNominatedMaps.Count()];
		vecChosenNominatedMaps.AddToTail(iMapToAdd);
		while (vecAvailableNominatedMaps.HasElement(iMapToAdd)) {
			vecAvailableNominatedMaps.FindAndRemove(iMapToAdd);
		}
	}
}

int CMapVoteSystem::GetMapIndexFromSubstring(const char* sMapSubstring)
{
    FOR_EACH_VEC(m_vecMapList, i) {
        if (V_stristr(m_vecMapList[i].GetName(), sMapSubstring)) {
            return i;
        }
    }
    return -1;
}

void CMapVoteSystem::ClearPlayerInfo(int iSlot)
{
	if (!g_bVoteManagerEnable)
		return;

	m_arrPlayerNominations[iSlot] = -1;
	m_arrPlayerVotes[iSlot] = -1;
}

int CMapVoteSystem::AddMapNomination(CPlayerSlot iPlayerSlot, const char* sMapSubstring)
{
	if (m_bIsVoteOngoing) return NominationReturnCodes::VOTE_STARTED;
	if (m_iForcedNextMapIndex != -1 || m_iMaxNominatedMaps == 0) return NominationReturnCodes::NOMINATION_DISABLED;

	CCSPlayerController* pController = CCSPlayerController::FromSlot(iPlayerSlot);
	if (!pController) return NominationReturnCodes::INVALID_INPUT;
	int iSlot = pController->GetPlayerSlot();

	if (sMapSubstring[0] == '\0')
	{
		// If we are resetting the nomination, return NOMINATION_RESET
		if (m_arrPlayerNominations[iSlot] != -1)
		{
			m_arrPlayerNominations[iSlot] = -1;
			return NominationReturnCodes::NOMINATION_RESET;
		}
		else
		{
			return NominationReturnCodes::NOMINATION_RESET_FAILED;
		}
	}

	// We are not reseting the nomination: is the map found? is it valid?
	int iFoundIndex = GetMapIndexFromSubstring(sMapSubstring);
	if (iFoundIndex == -1) return NominationReturnCodes::MAP_NOT_FOUND;
	if (!IsMapIndexEnabled(iFoundIndex)) return NominationReturnCodes::INVALID_MAP;
	m_arrPlayerNominations[iSlot] = iFoundIndex;
	return iFoundIndex;
}

int CMapVoteSystem::ForceNextMap(const char* sMapSubstring)
{
	if (sMapSubstring[0] == '\0') {
		ClientPrintAll(HUD_PRINTTALK, " \x06%s \x01 is no longer the forced next map.\n", m_vecMapList[m_iForcedNextMapIndex].GetName());
		m_iForcedNextMapIndex = -1;
		return 0;
	}

	int iFoundIndex = GetMapIndexFromSubstring(sMapSubstring);
	if (iFoundIndex == -1) return iFoundIndex;

	// When found, print the map and store the forced map
	m_iForcedNextMapIndex = iFoundIndex;
	ClientPrintAll(HUD_PRINTTALK, " \x06%s \x01 has been forced as next map.\n", m_vecMapList[iFoundIndex].GetName());
	return iFoundIndex;
}

static int __cdecl OrderMapsByWorkshopId(const CMapInfo* a, const CMapInfo* b)
{
	int valueA = a->GetWorkshopId();
	int valueB = b->GetWorkshopId();

	if (valueA < valueB)
		return -1;
	else if (valueA == valueB)
		return 0;
	else return 1;
}

void CMapVoteSystem::PrintDownloadProgress()
{
	if (m_DownloadQueue.Count() == 0)
		return;

	uint64 iBytesDownloaded = 0;
	uint64 iTotalBytes = 0;

	if (!g_steamAPI.SteamUGC()->GetItemDownloadInfo(m_DownloadQueue.Head(), &iBytesDownloaded, &iTotalBytes) || !iTotalBytes)
		return;

	double flMBDownloaded = (double)iBytesDownloaded / 1024 / 1024;
	double flTotalMB = (double)iTotalBytes / 1024 / 1024;

	double flProgress = (double)iBytesDownloaded / (double)iTotalBytes;
	flProgress *= 100.f;

	Message("Downloading map %lli: %.2f/%.2f MB (%.2f%%)\n", m_DownloadQueue.Head(), flMBDownloaded, flTotalMB, flProgress);
}

void CMapVoteSystem::OnMapDownloaded(DownloadItemResult_t *pResult)
{
	if (!m_DownloadQueue.Check(pResult->m_nPublishedFileId))
		return;

	m_DownloadQueue.RemoveAtHead();

	if (m_DownloadQueue.Count() == 0)
		return;

	g_steamAPI.SteamUGC()->DownloadItem(m_DownloadQueue.Head(), false);
}

void CMapVoteSystem::QueueMapDownload(PublishedFileId_t iWorkshopId)
{
	if (m_DownloadQueue.Check(iWorkshopId))
		return;

	m_DownloadQueue.Insert(iWorkshopId);

	if (m_DownloadQueue.Head() == iWorkshopId)
		g_steamAPI.SteamUGC()->DownloadItem(iWorkshopId, false);
}

bool CMapVoteSystem::LoadMapList()
{
	// This is called when the Steam API is init'd, now is the time to register this
	m_CallbackDownloadItemResult.Register(this, &CMapVoteSystem::OnMapDownloaded);

	m_vecMapList.Purge();
	KeyValues* pKV = new KeyValues("maplist");
	KeyValues::AutoDelete autoDelete(pKV);

	const char *pszPath = "addons/cs2fixes/configs/maplist.cfg";
	if (!pKV->LoadFromFile(g_pFullFileSystem, pszPath)) {
		Panic("Failed to load %s\n", pszPath);
		return false;
	}

	for (KeyValues* pKey = pKV->GetFirstSubKey(); pKey; pKey = pKey->GetNextKey()) {
		const char *pszName = pKey->GetName();
		uint64 iWorkshopId = pKey->GetUint64("workshop_id");
		bool bIsEnabled = pKey->GetBool("enabled", true);

		if (iWorkshopId != 0)
			QueueMapDownload(iWorkshopId);

		// We just append the maps to the map list
		m_vecMapList.AddToTail(CMapInfo(pszName, iWorkshopId, bIsEnabled));
	}

	new CTimer(0.f, true, true, [this]()
	{
		if (!this || m_DownloadQueue.Count() == 0)
			return -1.f;

		PrintDownloadProgress();

		return 1.f;
	});

	// Sort the map list by the workshop id
	m_vecMapList.Sort(OrderMapsByWorkshopId);

	// Print all the maps to show the order
	FOR_EACH_VEC(m_vecMapList, i) {
		CMapInfo map = m_vecMapList[i];

		if (map.GetWorkshopId() == 0)
			ConMsg("Map %d is %s, which is %s.\n", i, map.GetName(), map.IsEnabled() ? "enabled" : "disabled");
		else
			ConMsg("Map %d is %s with workshop id %llu, which is %s.\n", i, map.GetName(), map.GetWorkshopId(), map.IsEnabled()? "enabled" : "disabled");
	}

	m_bMapListLoaded = true;

	return true;
}

bool CMapVoteSystem::IsIntermissionAllowed()
{
	if (!g_bVoteManagerEnable)
		return true;

	// We need to prevent "ending the map twice" as it messes with ongoing map votes
	// This seems to be a CS2 bug that occurs when the round ends while already on the map end screen
	if (m_bIntermissionStarted)
		return false;

	m_bIntermissionStarted = true;
	return true;
}

CUtlStringList CMapVoteSystem::CreateWorkshopMapGroup()
{
	CUtlStringList mapList;

	for (int i = 0; i < GetMapListSize(); i++)
		mapList.CopyAndAddToTail(GetMapName(i));

	return mapList;
}