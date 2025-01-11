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

#include "map_votes.h"
#include "commands.h"
#include "common.h"
#include "ctimer.h"
#include "entity/cgamerules.h"
#include "eventlistener.h"
#include "idlemanager.h"
#include "playermanager.h"
#include "steam/steam_gameserver.h"
#include "strtools.h"
#include "utlstring.h"
#include "utlvector.h"
#include "votemanager.h"
#include <playerslot.h>
#include <random>
#include <stdio.h>

extern CGlobalVars* gpGlobals;
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
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Please wait for current map downloads to finish before loading map list again.");
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
	ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Map list reloaded!");
}

CON_COMMAND_F(cs2f_vote_maps_cooldown, "Default number of maps to wait until a map can be voted / nominated again i.e. cooldown.", FCVAR_LINKED_CONCOMMAND | FCVAR_SPONLY)
{
	if (!g_pMapVoteSystem)
	{
		Message("The map vote subsystem is not enabled.\n");
		return;
	}

	if (args.ArgC() < 2)
		Message("%s %d\n", args[0], g_pMapVoteSystem->GetDefaultMapCooldown());
	else
	{
		int iCurrentCooldown = g_pMapVoteSystem->GetDefaultMapCooldown();
		g_pMapVoteSystem->SetDefaultMapCooldown(V_StringToInt32(args[1], iCurrentCooldown));
	}
}

CON_COMMAND_F(cs2f_vote_max_nominations, "Number of nominations to include per vote, out of a maximum of 10.", FCVAR_LINKED_CONCOMMAND | FCVAR_SPONLY)
{
	if (!g_pMapVoteSystem)
	{
		Message("The map vote subsystem is not enabled.\n");
		return;
	}

	if (args.ArgC() < 2)
		Message("%s %d\n", args[0], g_pMapVoteSystem->GetMaxNominatedMaps());
	else
	{
		int iMaxNominatedMaps = g_pMapVoteSystem->GetMaxNominatedMaps();
		g_pMapVoteSystem->SetMaxNominatedMaps(V_StringToInt32(args[1], iMaxNominatedMaps));
	}
}

CON_COMMAND_CHAT_FLAGS(map, "<name/id> - Change map", ADMFLAG_CHANGEMAP)
{
	if (!g_bVoteManagerEnable)
		return;

	if (args.ArgC() < 2)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Usage: !map <name/id>");
		return;
	}

	std::string sMapInput = args[1];

	for (int i = 0; sMapInput[i]; i++)
	{
		// Injection prevention, because we may pass user input to ServerCommand
		if (sMapInput[i] == ';' || sMapInput[i] == '|')
			return;

		sMapInput[i] = tolower(sMapInput[i]);
	}

	const char* pszMapInput = sMapInput.c_str();

	if (g_pEngineServer2->IsMapValid(pszMapInput))
	{
		ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX "Changing map to \x06%s\x01...", pszMapInput);

		new CTimer(5.0f, false, true, [sMapInput]() {
			g_pEngineServer2->ChangeLevel(sMapInput.c_str(), nullptr);
			return -1.0f;
		});

		return;
	}

	std::string sCommand;
	std::string sMapName;
	uint64 iMap = g_pMapVoteSystem->HandlePlayerMapLookup(player, pszMapInput, true);

	if (iMap == -1)
		return;

	if (iMap > g_pMapVoteSystem->GetMapListSize())
	{
		sCommand = "host_workshop_map " + std::to_string(iMap);
		sMapName = std::to_string(iMap);
	}
	else
	{
		uint64 workshopId = g_pMapVoteSystem->GetMapWorkshopId(iMap);
		sMapName = g_pMapVoteSystem->GetMapName(iMap);

		if (workshopId == 0)
			sCommand = "map " + sMapName;
		else
			sCommand = "host_workshop_map " + std::to_string(workshopId);
	}

	ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX "Changing map to \x06%s\x01...", sMapName.c_str());

	new CTimer(5.0f, false, true, [sCommand]() {
		g_pEngineServer2->ServerCommand(sCommand.c_str());
		return -1.0f;
	});
}

CON_COMMAND_CHAT_FLAGS(setnextmap, "[name/id] - Force next map (empty to clear forced next map)", ADMFLAG_CHANGEMAP)
{
	if (!g_bVoteManagerEnable)
		return;

	g_pMapVoteSystem->ForceNextMap(player, args.ArgC() < 2 ? "" : args[1]);
}

static int __cdecl OrderStringsLexicographically(const MapIndexPair* a, const MapIndexPair* b)
{
	return V_strcasecmp(a->name, b->name);
}

CON_COMMAND_CHAT(nominate, "[mapname] - Nominate a map (empty to clear nomination or list all maps)")
{
	if (!g_bVoteManagerEnable || !player)
		return;

	g_pMapVoteSystem->AttemptNomination(player, args.ArgC() < 2 ? "" : args[1]);
}

CON_COMMAND_CHAT(nomlist, "- List the list of nominations")
{
	if (!g_bVoteManagerEnable)
		return;

	if (g_pMapVoteSystem->GetForcedNextMap() != -1)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Nominations are disabled because the next map has been forced to \x06%s\x01.", g_pMapVoteSystem->GetForcedNextMapName().c_str());
		return;
	}

	std::unordered_map<int, int> mapNominatedMaps;

	for (int i = 0; i < g_pMapVoteSystem->GetMapListSize(); i++)
	{
		if (!g_pMapVoteSystem->IsMapIndexEnabled(i)) continue;
		int iNumNominations = g_pMapVoteSystem->GetTotalNominations(i);
		if (iNumNominations == 0) continue;

		mapNominatedMaps[i] = iNumNominations;
	}

	if (mapNominatedMaps.size() == 0)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "No maps have been nominated yet!");
		return;
	}

	ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Current nominations:");

	for (auto pair : mapNominatedMaps)
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "- %s (%d times)\n", g_pMapVoteSystem->GetMapName(pair.first), pair.second);
}

CON_COMMAND_CHAT(mapcooldowns, "- List the maps currently in cooldown")
{
	if (!g_bVoteManagerEnable)
		return;

	int iMapCount = g_pMapVoteSystem->GetMapListSize();
	std::vector<std::pair<std::string, int>> vecCooldowns;

	for (int iMapIndex = 0; iMapIndex < iMapCount; iMapIndex++)
	{
		int iCooldown = g_pMapVoteSystem->GetCooldownMap(iMapIndex);

		if (iCooldown > 0 && g_pMapVoteSystem->GetMapEnabledStatus(iMapIndex))
			vecCooldowns.push_back(std::make_pair(g_pMapVoteSystem->GetMapName(iMapIndex), iCooldown));
	}

	if (vecCooldowns.size() == 0)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "There are no maps on cooldown!");
		return;
	}

	ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "The list of maps in cooldown will be shown in console.");
	ClientPrint(player, HUD_PRINTCONSOLE, "The list of maps in cooldown is:");

	std::sort(vecCooldowns.begin(), vecCooldowns.end(), [](auto& left, auto& right) {
		return left.second < right.second;
	});

	for (auto pair : vecCooldowns)
		ClientPrint(player, HUD_PRINTCONSOLE, "- %s (%d maps remaining)", pair.first.c_str(), pair.second);
}

CON_COMMAND_CHAT(nextmap, "- Check the next map if it was forced")
{
	if (!g_bVoteManagerEnable)
		return;

	if (g_pMapVoteSystem->GetForcedNextMap() == -1)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Next map is pending vote, no map has been forced.");
		return;
	}

	ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Next map is \x06%s\x01.", g_pMapVoteSystem->GetForcedNextMapName().c_str());
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
	if (GetCooldownMap(iMapIndex) > 0 || GetCurrentMapIndex() == iMapIndex) return false;
	if (!m_vecMapList[iMapIndex].IsEnabled()) return false;

	int iOnlinePlayers = g_playerManager->GetOnlinePlayerCount(false);
	bool bMeetsMaxPlayers = iOnlinePlayers <= m_vecMapList[iMapIndex].GetMaxPlayers();
	bool bMeetsMinPlayers = iOnlinePlayers >= m_vecMapList[iMapIndex].GetMinPlayers();
	return bMeetsMaxPlayers && bMeetsMinPlayers;
}

void CMapVoteSystem::OnLevelInit(const char* pMapName)
{
	if (!g_bVoteManagerEnable)
		return;

	m_bIsVoteOngoing = false;
	m_bIntermissionStarted = false;
	m_iForcedNextMap = -1;

	for (int i = 0; i < gpGlobals->maxClients; i++)
		ClearPlayerInfo(i);

	// Delay one tick to override any .cfg's
	new CTimer(0.02f, false, true, []() {
		g_pEngineServer2->ServerCommand("mp_match_end_changelevel 0");

		return -1.0f;
	});

	SetCurrentMapIndex(GetMapIndexFromString(pMapName));
}

void CMapVoteSystem::StartVote()
{
	m_bIsVoteOngoing = true;

	g_pIdleSystem->PauseIdleChecks();

	// Reset the player vote counts as the vote just started
	for (int i = 0; i < gpGlobals->maxClients; i++)
		m_arrPlayerVotes[i] = -1;

	// If we are forcing a map, just override all the vote options
	if (m_iForcedNextMap != -1)
	{
		for (int i = 0; i < 10; i++)
			if (m_iForcedNextMap > GetMapListSize())
				g_pGameRules->m_nEndMatchMapGroupVoteOptions[i] = -1;
			else
				g_pGameRules->m_nEndMatchMapGroupVoteOptions[i] = m_iForcedNextMap;

		new CTimer(6.0f, false, true, []() {
			g_pMapVoteSystem->FinishVote();
			return -1.0f;
		});

		return;
	}

	// Seed the randomness for the event
	m_iRandomWinnerShift = rand();

	// Select random maps not in cooldown, not disabled, and not nominated
	CUtlVector<int> vecPossibleMaps;
	CUtlVector<int> vecIncludedMaps;
	GetNominatedMapsForVote(vecIncludedMaps);
	for (int i = 0; i < m_vecMapList.Count(); i++)
	{
		if (!IsMapIndexEnabled(i)) continue;
		if (vecIncludedMaps.HasElement(i)) continue;
		vecPossibleMaps.AddToTail(i);
	}

	// Print all available maps out to console
	FOR_EACH_VEC(vecPossibleMaps, i)
	{
		int iPossibleMapIndex = vecPossibleMaps[i];
		Message("The %d-th possible map index %d is %s\n", i, iPossibleMapIndex, m_vecMapList[iPossibleMapIndex].GetName());
	}

	// Set the maps in the vote: merge nominated and possible maps, then randomly sort
	int iNumMapsInVote = vecPossibleMaps.Count() + vecIncludedMaps.Count();
	if (iNumMapsInVote >= 10) iNumMapsInVote = 10;
	while (vecIncludedMaps.Count() < iNumMapsInVote && vecPossibleMaps.Count() > 0)
	{
		int iMapToAdd = vecPossibleMaps[rand() % vecPossibleMaps.Count()];
		vecIncludedMaps.AddToTail(iMapToAdd);
		vecPossibleMaps.FindAndRemove(iMapToAdd);
	}

	// Randomly sort the chosen maps
	for (int i = 0; i < 10; i++)
	{
		if (i < iNumMapsInVote)
		{
			int iMapToAdd = vecIncludedMaps[rand() % vecIncludedMaps.Count()];
			g_pGameRules->m_nEndMatchMapGroupVoteOptions[i] = iMapToAdd;
			vecIncludedMaps.FindAndRemove(iMapToAdd);
		}
		else
		{
			g_pGameRules->m_nEndMatchMapGroupVoteOptions[i] = -1;
		}
	}

	// Print the maps chosen in the vote to console
	for (int i = 0; i < iNumMapsInVote; i++)
	{
		int iMapIndex = g_pGameRules->m_nEndMatchMapGroupVoteOptions[i];
		Message("The %d-th chosen map index %d is %s\n", i, iMapIndex, m_vecMapList[iMapIndex].GetName());
	}

	// Start the end-of-vote timer to finish the vote
	ConVar* cvar = g_pCVar->GetConVar(g_pCVar->FindConVar("mp_endmatch_votenextleveltime"));
	float flVoteTime = *(float*)&cvar->values;
	new CTimer(flVoteTime, false, true, []() {
		g_pMapVoteSystem->FinishVote();
		return -1.0;
	});
}

int CMapVoteSystem::GetTotalNominations(int iMapIndex)
{
	int iNumNominations = 0;
	for (int i = 0; i < gpGlobals->maxClients; i++)
	{
		auto pController = CCSPlayerController::FromSlot(i);
		if (pController && pController->IsConnected() && m_arrPlayerNominations[i] == iMapIndex)
			iNumNominations++;
	}
	return iNumNominations;
}

void CMapVoteSystem::FinishVote()
{
	if (!m_bIsVoteOngoing) return;

	// Clean up the ongoing voting state and variables
	m_bIsVoteOngoing = false;

	// Get the winning map
	bool bIsNextMapVoted = UpdateWinningMap();
	int iNextMapVoteIndex = WinningMapIndex();
	bool bIsNextMapForced = m_iForcedNextMap != -1;
	char buffer[256];
	uint64 iWinningMap; // Map index OR possibly workshop ID if next map was forced

	if (bIsNextMapForced)
	{
		iWinningMap = m_iForcedNextMap;
	}
	else
	{
		g_pGameRules->m_nEndMatchMapVoteWinner = iNextMapVoteIndex;
		iWinningMap = g_pGameRules->m_nEndMatchMapGroupVoteOptions[iNextMapVoteIndex];
	}

	// Print out the map we're changing to
	if (bIsNextMapForced)
		V_snprintf(buffer, sizeof(buffer), "The vote was overriden. \x06%s\x01 will be the next map!\n", GetForcedNextMapName().c_str());
	else if (bIsNextMapVoted)
		V_snprintf(buffer, sizeof(buffer), "The vote has ended. \x06%s\x01 will be the next map!\n", GetMapName(iWinningMap));
	else
		V_snprintf(buffer, sizeof(buffer), "No map was chosen. \x06%s\x01 will be the next map!\n", GetMapName(iWinningMap));

	ClientPrintAll(HUD_PRINTTALK, buffer);
	Message(buffer);

	// Print vote result information: how many votes did each map get?
	if (!bIsNextMapForced)
	{
		int arrMapVotes[10] = {0};
		Message("Map vote result --- total votes per map:\n");
		for (int i = 0; i < gpGlobals->maxClients; i++)
		{
			auto pController = CCSPlayerController::FromSlot(i);
			int iPlayerVotedIndex = m_arrPlayerVotes[i];
			if (pController && pController->IsConnected() && iPlayerVotedIndex >= 0)
				arrMapVotes[iPlayerVotedIndex]++;
		}
		for (int i = 0; i < 10; i++)
		{
			int iMapIndex = g_pGameRules->m_nEndMatchMapGroupVoteOptions[i];
			const char* sIsWinner = (i == iNextMapVoteIndex) ? "(WINNER)" : "";
			Message("- %s got %d votes\n", GetMapName(iMapIndex), arrMapVotes[i]);
		}
	}

	// Put the map on cooldown as we transition to the next map if map index is valid, also decrease cooldown remaining for others
	// Map index will be invalid for any map not added to maplist.cfg
	DecrementAllMapCooldowns();

	int iMapIndex = GetCurrentMapIndex();
	if (iMapIndex >= 0 && iMapIndex < GetMapListSize())
		PutMapOnCooldown(iMapIndex);

	WriteMapCooldownsToFile();

	// Wait a second and force-change the map
	new CTimer(1.0, false, true, [iWinningMap]() {
		char sChangeMapCmd[128];
		uint64 workshopId = iWinningMap > g_pMapVoteSystem->GetMapListSize() ? iWinningMap : g_pMapVoteSystem->GetMapWorkshopId(iWinningMap);

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
	if (iWinningMapIndex >= 0)
	{
		g_pGameRules->m_nEndMatchMapVoteWinner = iWinningMapIndex;
		return true;
	}
	return false;
}

int CMapVoteSystem::WinningMapIndex()
{
	// Count the votes of every player
	int arrMapVotes[10] = {0};
	for (int i = 0; i < gpGlobals->maxClients; i++)
	{
		auto pController = CCSPlayerController::FromSlot(i);
		if (pController && pController->IsConnected() && m_arrPlayerVotes[i] >= 0)
			arrMapVotes[m_arrPlayerVotes[i]]++;
	}

	// Identify the max. number of votes
	int iMaxVotes = 0;
	for (int i = 0; i < 10; i++)
		iMaxVotes = (arrMapVotes[i] > iMaxVotes) ? arrMapVotes[i] : iMaxVotes;

	// Identify how many maps are tied with the max number of votes
	int iMapsWithMaxVotes = 0;
	for (int i = 0; i < 10; i++)
		if (arrMapVotes[i] == iMaxVotes) iMapsWithMaxVotes++;

	// Break ties: 'random' map with the most votes
	int iWinningMapTieBreak = m_iRandomWinnerShift % iMapsWithMaxVotes;
	int iWinningMapCount = 0;
	for (int i = 0; i < 10; i++)
	{
		if (arrMapVotes[i] == iMaxVotes)
		{
			if (iWinningMapCount == iWinningMapTieBreak) return i;
			iWinningMapCount++;
		}
	}
	return -1;
}

void CMapVoteSystem::GetNominatedMapsForVote(CUtlVector<int>& vecChosenNominatedMaps)
{
	std::unordered_map<int, int> mapAvailableNominatedMaps;

	for (int i = 0; i < gpGlobals->maxClients; i++)
	{
		int iNominatedMapIndex = m_arrPlayerNominations[i];

		// Introduce nominated map indexes and count the total number
		if (iNominatedMapIndex != -1)
			++mapAvailableNominatedMaps[iNominatedMapIndex];
	}

	int iMapsToIncludeInNominate = (mapAvailableNominatedMaps.size() < m_iMaxNominatedMaps) ? mapAvailableNominatedMaps.size() : m_iMaxNominatedMaps;
	std::vector<int> vecTiedNominations;
	auto rng = std::default_random_engine{std::random_device{}()};

	// Select top maps by number of nominations
	while (vecChosenNominatedMaps.Count() < iMapsToIncludeInNominate)
	{
		if (vecTiedNominations.size() == 0)
		{
			// Find highest nomination count
			int iMostNominations = std::max_element(
									   mapAvailableNominatedMaps.begin(), mapAvailableNominatedMaps.end(),
									   [](const std::pair<int, int>& p1, const std::pair<int, int>& p2) {
										   return p1.second < p2.second;
									   })
									   ->second;

			// Copy the most nominated maps to a new vector
			for (auto pair : mapAvailableNominatedMaps)
				if (pair.second == iMostNominations)
					vecTiedNominations.push_back(pair.first);

			// Randomize the vector order
			std::ranges::shuffle(vecTiedNominations, rng);
		}

		// Pick map from front of vector, and remove from both sources
		vecChosenNominatedMaps.AddToTail(vecTiedNominations.front());
		mapAvailableNominatedMaps.erase(vecTiedNominations.front());
		vecTiedNominations.erase(vecTiedNominations.begin());
	}
}

std::vector<int> CMapVoteSystem::GetMapIndexesFromSubstring(const char* sMapSubstring)
{
	std::vector<int> vecMaps;

	FOR_EACH_VEC(m_vecMapList, i)
	{
		if (V_stristr(m_vecMapList[i].GetName(), sMapSubstring))
			vecMaps.push_back(i);
	}

	return vecMaps;
}

uint64 CMapVoteSystem::HandlePlayerMapLookup(CCSPlayerController* pController, const char* sMapSubstring, bool bAllowWorkshopID)
{
	if (bAllowWorkshopID)
	{
		uint64 iWorkshopID = V_StringToUint64(sMapSubstring, 0, NULL, NULL, PARSING_FLAG_SKIP_WARNING);

		// Check if input is numeric (workshop ID)
		// Not safe to expose to all admins until crashing on failed workshop addon downloads is fixed
		if ((!pController || pController->GetZEPlayer()->IsAdminFlagSet(ADMFLAG_RCON)) && iWorkshopID != 0)
		{
			// Try to get a head start on downloading the map if needed
			g_steamAPI.SteamUGC()->DownloadItem(iWorkshopID, false);

			return iWorkshopID;
		}
	}

	std::vector<int> foundIndexes = GetMapIndexesFromSubstring(sMapSubstring);

	if (foundIndexes.size() > 0)
	{
		if (foundIndexes.size() > 1)
		{
			ClientPrint(pController, HUD_PRINTTALK, CHAT_PREFIX "Multiple maps matched \x06%s\x01, try being more specific:", sMapSubstring);

			for (int i = 0; i < foundIndexes.size() && i < 5; i++)
				ClientPrint(pController, HUD_PRINTTALK, "- %s", GetMapName(foundIndexes[i]));
		}
		else
		{
			return foundIndexes[0];
		}
	}
	else
	{
		ClientPrint(pController, HUD_PRINTTALK, CHAT_PREFIX "Failed to find a map matching \x06%s\x01.", sMapSubstring);
	}

	return -1;
}

int CMapVoteSystem::GetMapIndexFromString(const char* sMapString)
{
	FOR_EACH_VEC(m_vecMapList, i)
	{
		if (!V_strcasecmp(m_vecMapList[i].GetName(), sMapString))
			return i;
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

void CMapVoteSystem::AttemptNomination(CCSPlayerController* pController, const char* sMapSubstring)
{
	int iSlot = pController->GetPlayerSlot();
	ZEPlayer* pPlayer = g_playerManager->GetPlayer(iSlot);

	if (!pPlayer)
		return;

	if (GetMaxNominatedMaps() == 0)
	{
		ClientPrint(pController, HUD_PRINTTALK, CHAT_PREFIX "Nominations are currently disabled.");
		return;
	}

	if (GetForcedNextMap() != -1)
	{
		ClientPrint(pController, HUD_PRINTTALK, CHAT_PREFIX "Nominations are disabled because the next map has been forced to \x06%s\x01.", GetForcedNextMapName().c_str());
		return;
	}

	if (IsVoteOngoing())
	{
		ClientPrint(pController, HUD_PRINTTALK, CHAT_PREFIX "Nominations are disabled because the vote has already started.");
		return;
	}

	if (sMapSubstring[0] == '\0')
	{
		if (m_arrPlayerNominations[iSlot] != -1)
		{
			ClearPlayerInfo(iSlot);
			ClientPrint(pController, HUD_PRINTTALK, CHAT_PREFIX "Your nomination was reset.");
		}
		else
		{
			PrintMapList(pController);
		}

		return;
	}

	int iFoundIndex = HandlePlayerMapLookup(pController, sMapSubstring);
	int iPlayerCount = g_playerManager->GetOnlinePlayerCount(false);

	if (iFoundIndex == -1)
		return;

	if (!GetMapEnabledStatus(iFoundIndex))
	{
		ClientPrint(pController, HUD_PRINTTALK, CHAT_PREFIX "Cannot nominate \x06%s\x01 because it's disabled.", GetMapName(iFoundIndex));
		return;
	}

	if (GetCurrentMapIndex() == iFoundIndex)
	{
		ClientPrint(pController, HUD_PRINTTALK, CHAT_PREFIX "Cannot nominate \x06%s\x01 because it's already the current map!", GetMapName(iFoundIndex));
		return;
	}

	if (GetCooldownMap(iFoundIndex) > 0)
	{
		ClientPrint(pController, HUD_PRINTTALK, CHAT_PREFIX "Cannot nominate \x06%s\x01 because it's on a %i map cooldown.", GetMapName(iFoundIndex), GetCooldownMap(iFoundIndex));
		return;
	}

	if (iPlayerCount < GetMapMinPlayers(iFoundIndex))
	{
		ClientPrint(pController, HUD_PRINTTALK, CHAT_PREFIX "Cannot nominate \x06%s\x01 because it needs %i more players.", GetMapName(iFoundIndex), GetMapMinPlayers(iFoundIndex) - iPlayerCount);
		return;
	}

	if (iPlayerCount > GetMapMaxPlayers(iFoundIndex))
	{
		ClientPrint(pController, HUD_PRINTTALK, CHAT_PREFIX "Cannot nominate \x06%s\x01 because it needs %i less players.", GetMapName(iFoundIndex), iPlayerCount - GetMapMaxPlayers(iFoundIndex));
		return;
	}

	if (pPlayer->GetNominateTime() + 60.0f > gpGlobals->curtime)
	{
		int iRemainingTime = (int)(pPlayer->GetNominateTime() + 60.0f - gpGlobals->curtime);
		ClientPrint(pController, HUD_PRINTTALK, CHAT_PREFIX "Wait %i seconds before you can nominate again.", iRemainingTime);
		return;
	}

	m_arrPlayerNominations[iSlot] = iFoundIndex;
	ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX "\x06%s \x01was nominated by %s. It now has %d nomination%s.", GetMapName(iFoundIndex), pController->GetPlayerName(), GetTotalNominations(iFoundIndex), GetTotalNominations(iFoundIndex) > 1 ? "s" : "");
	pPlayer->SetNominateTime(gpGlobals->curtime);
}

void CMapVoteSystem::PrintMapList(CCSPlayerController* pController)
{
	CUtlVector<MapIndexPair> vecMapNames;
	int iPlayerCount = g_playerManager->GetOnlinePlayerCount(false);

	for (int i = 0; i < GetMapListSize(); i++)
	{
		if (!GetMapEnabledStatus(i))
			continue;

		MapIndexPair map;
		map.name = GetMapName(i);
		map.index = i;
		vecMapNames.AddToTail(map);
	}

	vecMapNames.Sort(OrderStringsLexicographically);
	ClientPrint(pController, HUD_PRINTTALK, CHAT_PREFIX "The list of all maps will be shown in console.");
	ClientPrint(pController, HUD_PRINTCONSOLE, "The list of all maps is:");

	FOR_EACH_VEC(vecMapNames, i)
	{
		const char* name = vecMapNames[i].name;
		int mapIndex = vecMapNames[i].index;
		int cooldown = GetCooldownMap(mapIndex);
		int minPlayers = GetMapMinPlayers(mapIndex);
		int maxPlayers = GetMapMaxPlayers(mapIndex);

		if (cooldown > 0)
			ClientPrint(pController, HUD_PRINTCONSOLE, "- %s - Cooldown: %d", name, cooldown);
		else if (mapIndex == GetCurrentMapIndex())
			ClientPrint(pController, HUD_PRINTCONSOLE, "- %s - Current Map", name);
		else if (iPlayerCount < minPlayers)
			ClientPrint(pController, HUD_PRINTCONSOLE, "- %s - +%d Players", name, minPlayers - iPlayerCount);
		else if (iPlayerCount > maxPlayers)
			ClientPrint(pController, HUD_PRINTCONSOLE, "- %s - -%d Players", name, iPlayerCount - maxPlayers);
		else
			ClientPrint(pController, HUD_PRINTCONSOLE, "- %s", name);
	}
}

void CMapVoteSystem::ForceNextMap(CCSPlayerController* pController, const char* sMapSubstring)
{
	if (sMapSubstring[0] == '\0')
	{
		if (GetForcedNextMap() == -1)
		{
			ClientPrint(pController, HUD_PRINTTALK, CHAT_PREFIX "There is no next map to reset!");
		}
		else
		{
			ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX "\x06%s \x01is no longer the forced next map.\n", GetForcedNextMapName().c_str());
			m_iForcedNextMap = -1;
		}

		return;
	}

	uint64 iFoundMap = HandlePlayerMapLookup(pController, sMapSubstring, true);

	if (GetForcedNextMap() == iFoundMap)
	{
		ClientPrint(pController, HUD_PRINTTALK, CHAT_PREFIX "\x06%s\x01 is already the next map!", GetForcedNextMapName().c_str());
		return;
	}

	// When found, print the map and store the forced map
	m_iForcedNextMap = iFoundMap;
	ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX "\x06%s \x01has been forced as the next map.\n", GetForcedNextMapName().c_str());
}

static int __cdecl OrderMapsByWorkshopId(const CMapInfo* a, const CMapInfo* b)
{
	int valueA = a->GetWorkshopId();
	int valueB = b->GetWorkshopId();

	if (valueA < valueB)
		return -1;
	else if (valueA == valueB)
		return 0;
	else
		return 1;
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

void CMapVoteSystem::OnMapDownloaded(DownloadItemResult_t* pResult)
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

	const char* pszPath = "addons/cs2fixes/configs/maplist.cfg";
	if (!pKV->LoadFromFile(g_pFullFileSystem, pszPath))
	{
		Panic("Failed to load %s\n", pszPath);
		return false;
	}

	// Load map cooldowns from file
	KeyValues* pKVcooldowns = new KeyValues("cooldowns");
	KeyValues::AutoDelete autoDeleteKVcooldowns(pKVcooldowns);
	const char* pszCooldownFilePath = "addons/cs2fixes/data/cooldowns.txt";
	if (!pKVcooldowns->LoadFromFile(g_pFullFileSystem, pszCooldownFilePath))
		Message("Failed to load cooldown file at %s - resetting all cooldowns to 0\n", pszCooldownFilePath);

	// KV1 has some funny behaviour with capitalization, to ensure consistency we can't directly lookup case-sensitive key names
	std::unordered_map<std::string, int> mapCooldowns;

	for (KeyValues* pKey = pKVcooldowns->GetFirstSubKey(); pKey; pKey = pKey->GetNextKey())
	{
		std::string sMapName = pKey->GetName();
		int iCooldown = pKey->GetInt();

		for (int i = 0; sMapName[i]; i++)
			sMapName[i] = tolower(sMapName[i]);

		mapCooldowns[sMapName] = iCooldown;
	}

	for (KeyValues* pKey = pKV->GetFirstSubKey(); pKey; pKey = pKey->GetNextKey())
	{
		const char* pszName = pKey->GetName();
		std::string sName = pszName;

		for (int i = 0; sName[i]; i++)
			sName[i] = tolower(sName[i]);

		uint64 iWorkshopId = pKey->GetUint64("workshop_id");
		bool bIsEnabled = pKey->GetBool("enabled", true);
		int iMinPlayers = pKey->GetInt("min_players", 0);
		int iMaxPlayers = pKey->GetInt("max_players", 64);
		int iBaseCooldown = pKey->GetInt("cooldown", m_iDefaultMapCooldown);
		int iCurrentCooldown = mapCooldowns[sName];

		if (iWorkshopId != 0)
			QueueMapDownload(iWorkshopId);

		// We just append the maps to the map list
		m_vecMapList.AddToTail(CMapInfo(pszName, iWorkshopId, bIsEnabled, iMinPlayers, iMaxPlayers, iBaseCooldown, iCurrentCooldown));
	}

	new CTimer(0.f, true, true, []() {
		if (g_pMapVoteSystem->m_DownloadQueue.Count() == 0)
			return -1.f;

		g_pMapVoteSystem->PrintDownloadProgress();

		return 1.f;
	});

	// Sort the map list by the workshop id
	m_vecMapList.Sort(OrderMapsByWorkshopId);

	// Print all the maps to show the order
	FOR_EACH_VEC(m_vecMapList, i)
	{
		CMapInfo map = m_vecMapList[i];

		if (map.GetWorkshopId() == 0)
			ConMsg("Map %d is %s, which is %s. MinPlayers: %d MaxPlayers: %d Cooldown: %d\n", i, map.GetName(), map.IsEnabled() ? "enabled" : "disabled", map.GetMinPlayers(), map.GetMaxPlayers(), map.GetBaseCooldown());
		else
			ConMsg("Map %d is %s with workshop id %llu, which is %s. MinPlayers: %d MaxPlayers: %d Cooldown: %d\n", i, map.GetName(), map.GetWorkshopId(), map.IsEnabled() ? "enabled" : "disabled", map.GetMinPlayers(), map.GetMaxPlayers(), map.GetBaseCooldown());
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

void CMapVoteSystem::DecrementAllMapCooldowns()
{
	FOR_EACH_VEC(m_vecMapList, i)
	{
		CMapInfo* pMap = &m_vecMapList[i];
		pMap->DecrementCooldown();
	}
}

bool CMapVoteSystem::WriteMapCooldownsToFile()
{
	KeyValues* pKV = new KeyValues("cooldowns");
	KeyValues::AutoDelete autoDelete(pKV);

	const char* pszPath = "addons/cs2fixes/data/cooldowns.txt";

	FOR_EACH_VEC(m_vecMapList, i)
	{
		std::string mapName = m_vecMapList[i].GetName();
		const int mapCooldown = m_vecMapList[i].GetCooldown();

		for (int i = 0; mapName[i]; i++)
			mapName[i] = tolower(mapName[i]);

		if (mapCooldown > 0)
			pKV->AddInt(mapName.c_str(), mapCooldown);
	}

	if (!pKV->SaveToFile(g_pFullFileSystem, pszPath))
	{
		Panic("Failed to write cooldowns to file: %s\n", pszPath);
		return false;
	}

	return true;
}

void CMapVoteSystem::ClearInvalidNominations()
{
	if (!g_bVoteManagerEnable || m_bIsVoteOngoing)
		return;

	for (int i = 0; i < gpGlobals->maxClients; i++)
	{
		int iNominatedMapIndex = m_arrPlayerNominations[i];

		// Ignore unset nominations (negative index)
		if (iNominatedMapIndex < 0)
			continue;

		// Check if nominated index still meets criteria for nomination
		if (!IsMapIndexEnabled(iNominatedMapIndex))
		{
			ClearPlayerInfo(i);
			CCSPlayerController* pPlayer = CCSPlayerController::FromSlot(i);
			if (!pPlayer)
				continue;

			ClientPrint(pPlayer, HUD_PRINTTALK, CHAT_PREFIX "Your nomination for \x06%s \x01has been removed because the player count requirements are no longer met.", GetMapName(iNominatedMapIndex));
		}
	}
}