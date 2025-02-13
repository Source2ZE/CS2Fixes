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
#include "iserver.h"
#include "playermanager.h"
#include "steam/steam_gameserver.h"
#include "strtools.h"
#include "utlstring.h"
#include "utlvector.h"
#undef snprintf
#include "vendor/nlohmann/json.hpp"
#include "votemanager.h"
#include <fstream>
#include <playerslot.h>
#include <random>
#include <stdio.h>

extern CGlobalVars* GetGlobals();
extern CCSGameRules* g_pGameRules;
extern IVEngineServer2* g_pEngineServer2;
extern CSteamGameServerAPIContext g_steamAPI;
extern IGameTypes* g_pGameTypes;

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

	if (!g_pMapVoteSystem->LoadMapList() || !V_strcmp(g_pMapVoteSystem->GetCurrentMapName(), "MISSING_MAP"))
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Failed to reload map list!");
		return;
	}

	// A CUtlStringList param is also expected, but we build it in our CreateWorkshopMapGroup pre-hook anyways
	CALL_VIRTUAL(void, g_GameConfig->GetOffset("IGameTypes_CreateWorkshopMapGroup"), g_pGameTypes, "workshop");

	// Updating the mapgroup requires reloading the map for everything to load properly
	char sChangeMapCmd[128] = "";

	if (g_pMapVoteSystem->GetCurrentWorkshopMap() != 0)
		V_snprintf(sChangeMapCmd, sizeof(sChangeMapCmd), "host_workshop_map %llu", g_pMapVoteSystem->GetCurrentWorkshopMap());
	else
		V_snprintf(sChangeMapCmd, sizeof(sChangeMapCmd), "map %s", g_pMapVoteSystem->GetCurrentMapName());

	g_pEngineServer2->ServerCommand(sChangeMapCmd);
	ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Map list reloaded!");
}

CON_COMMAND_F(cs2f_vote_maps_cooldown, "Default number of hours until a map can be played again i.e. cooldown", FCVAR_LINKED_CONCOMMAND | FCVAR_SPONLY)
{
	float fCurrentCooldown = g_pMapVoteSystem->GetDefaultMapCooldown();

	if (args.ArgC() < 2)
		Msg("%s %f\n", args[0], fCurrentCooldown);
	else
		g_pMapVoteSystem->SetDefaultMapCooldown(V_StringToFloat32(args[1], fCurrentCooldown));
}

CON_COMMAND_F(cs2f_vote_max_nominations, "Number of nominations to include per vote, out of a maximum of 10", FCVAR_LINKED_CONCOMMAND | FCVAR_SPONLY)
{
	int iMaxNominatedMaps = g_pMapVoteSystem->GetMaxNominatedMaps();

	if (args.ArgC() < 2)
		Msg("%s %d\n", args[0], iMaxNominatedMaps);
	else
	{
		int iValue = V_StringToInt32(args[1], iMaxNominatedMaps);

		if (iValue < 0 || iValue > 10)
			Msg("Value must be between 0-10!\n");
		else
			g_pMapVoteSystem->SetMaxNominatedMaps(iValue);
	}
}

CON_COMMAND_F(cs2f_vote_max_maps, "Number of total maps to include per vote, including nominations, out of a maximum of 10", FCVAR_LINKED_CONCOMMAND | FCVAR_SPONLY)
{
	int iMaxVoteMaps = g_pMapVoteSystem->GetMaxVoteMaps();

	if (args.ArgC() < 2)
		Msg("%s %d\n", args[0], iMaxVoteMaps);
	else
	{
		int iValue = V_StringToInt32(args[1], iMaxVoteMaps);

		if (iValue < 2 || iValue > 10)
			Msg("Value must be between 2-10!\n");
		else
			g_pMapVoteSystem->SetMaxVoteMaps(iValue);
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

	std::string sMapInput = g_pMapVoteSystem->StringToLower(args[1]);

	for (int i = 0; sMapInput[i]; i++)
	{
		// Injection prevention, because we may pass user input to ServerCommand
		if (sMapInput[i] == ';' || sMapInput[i] == '|')
			return;
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

	std::unordered_map<int, int> mapNominatedMaps = g_pMapVoteSystem->GetNominatedMaps();

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

	// Use a new vector, because we want to sort the command output
	std::vector<std::pair<std::string, float>> vecCooldowns;

	for (std::shared_ptr<CCooldown> pCooldown : g_pMapVoteSystem->GetMapCooldowns())
	{
		// Only print maps that are added to maplist.cfg
		if (pCooldown->IsOnCooldown() && g_pMapVoteSystem->GetMapIndexFromString(pCooldown->GetMapName()) != -1)
			vecCooldowns.push_back(std::make_pair(pCooldown->GetMapName(), pCooldown->GetCurrentCooldown()));
	}

	if (vecCooldowns.size() == 0)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "There are no maps on cooldown!");
		return;
	}

	ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "The list of maps in cooldown will be shown in console.");
	ClientPrint(player, HUD_PRINTCONSOLE, "The list of maps in cooldown is:");

	std::sort(vecCooldowns.begin(), vecCooldowns.end(), [](auto left, auto right) {
		return left.second < right.second;
	});

	for (auto pair : vecCooldowns)
		ClientPrint(player, HUD_PRINTCONSOLE, "- %s (%s)", pair.first.c_str(), g_pMapVoteSystem->GetMapCooldownText(pair.first.c_str(), true).c_str());
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

CON_COMMAND_CHAT(maplist, "- List the maps in the server")
{
	g_pMapVoteSystem->PrintMapList(player);
}

bool CMapVoteSystem::IsMapIndexEnabled(int iMapIndex)
{
	if (iMapIndex >= GetMapListSize() || iMapIndex < 0) return false;
	if (GetMapCooldown(iMapIndex)->IsOnCooldown() || GetCurrentMapIndex() == iMapIndex) return false;
	if (!m_vecMapList[iMapIndex]->IsEnabled()) return false;

	int iOnlinePlayers = g_playerManager->GetOnlinePlayerCount(false);
	bool bMeetsMaxPlayers = iOnlinePlayers <= GetMapMaxPlayers(iMapIndex);
	bool bMeetsMinPlayers = iOnlinePlayers >= GetMapMinPlayers(iMapIndex);
	return bMeetsMaxPlayers && bMeetsMinPlayers;
}

void CMapVoteSystem::OnLevelInit(const char* pMapName)
{
	if (!g_bVoteManagerEnable)
		return;

	m_bIsVoteOngoing = false;
	m_bIntermissionStarted = false;
	m_iForcedNextMap = -1;

	for (int i = 0; i < MAXPLAYERS; i++)
		ClearPlayerInfo(i);

	// Delay one tick to override any .cfg's
	new CTimer(0.02f, false, true, []() {
		g_pEngineServer2->ServerCommand("mp_match_end_changelevel 0");
		g_pEngineServer2->ServerCommand("mp_endmatch_votenextmap 1");

		return -1.0f;
	});
}

void CMapVoteSystem::StartVote()
{
	if (!g_pGameRules)
		return;

	m_bIsVoteOngoing = true;

	// Select random maps that meet requirements to appear
	std::vector<int> vecPossibleMaps;
	for (int i = 0; i < GetMapListSize(); i++)
		if (IsMapIndexEnabled(i))
			vecPossibleMaps.push_back(i);

	m_iVoteSize = std::min((int)vecPossibleMaps.size(), GetMaxVoteMaps());
	bool bAbort = false;
	// CONVAR_TODO
	ConVar* pVoteCvar = g_pCVar->GetConVar(g_pCVar->FindConVar("mp_endmatch_votenextmap"));
	// HACK: values is actually the cvar value itself, hence this ugly cast.
	bool bVoteEnabled = *(bool*)&pVoteCvar->values;

	if (!bVoteEnabled)
	{
		m_bIsVoteOngoing = false;
		bAbort = true;
	}
	else if (m_iForcedNextMap != -1)
	{
		new CTimer(6.0f, false, true, []() {
			g_pMapVoteSystem->FinishVote();
			return -1.0f;
		});

		bAbort = true;
	}
	else if (m_iVoteSize < 2)
	{
		ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX "Not enough maps available for map vote, aborting! Please have an admin loosen map limits.");
		Message("Not enough maps available for map vote, aborting!\n");
		g_pEngineServer2->ServerCommand("mp_match_end_changelevel 1"); // Allow game to auto-switch map again
		m_bIsVoteOngoing = false;
		bAbort = true;
	}

	if (bAbort)
	{
		// Disable the map vote
		for (int i = 0; i < 10; i++)
		{
			g_pGameRules->m_nEndMatchMapGroupVoteTypes[i] = -1;
			g_pGameRules->m_nEndMatchMapGroupVoteOptions[i] = -1;
		}

		return;
	}

	// We're checking this later, so we can always disable the map vote if mp_endmatch_votenextmap is disabled
	if (!g_bVoteManagerEnable)
		return;

	// Reset the player vote counts as the vote just started
	for (int i = 0; i < MAXPLAYERS; i++)
		m_arrPlayerVotes[i] = -1;

	// Print all available maps out to console
	for (int i = 0; i < vecPossibleMaps.size(); i++)
	{
		int iPossibleMapIndex = vecPossibleMaps[i];
		Message("The %d-th possible map index %d is %s\n", i, iPossibleMapIndex, GetMapName(iPossibleMapIndex));
	}

	// Seed the randomness for the event
	m_iRandomWinnerShift = rand();

	// Set the maps in the vote: merge nominated and random possible maps
	std::vector<int> vecIncludedMaps = GetNominatedMapsForVote();

	while (vecIncludedMaps.size() < m_iVoteSize && vecPossibleMaps.size() > 0)
	{
		int iMapToAdd = vecPossibleMaps[rand() % vecPossibleMaps.size()];

		// Do we need to add this map? It may have already been included via nomination
		if (std::find(vecIncludedMaps.begin(), vecIncludedMaps.end(), iMapToAdd) == vecIncludedMaps.end())
			vecIncludedMaps.push_back(iMapToAdd);

		std::erase_if(vecPossibleMaps, [iMapToAdd](int i) { return i == iMapToAdd; });
	}

	// Randomly sort the chosen maps
	for (int i = 0; i < 10; i++)
	{
		if (i < m_iVoteSize)
		{
			int iMapToAdd = vecIncludedMaps[rand() % vecIncludedMaps.size()];
			g_pGameRules->m_nEndMatchMapGroupVoteTypes[i] = 0;
			g_pGameRules->m_nEndMatchMapGroupVoteOptions[i] = iMapToAdd;
			std::erase_if(vecIncludedMaps, [iMapToAdd](int i) { return i == iMapToAdd; });
		}
		else
		{
			g_pGameRules->m_nEndMatchMapGroupVoteTypes[i] = -1;
			g_pGameRules->m_nEndMatchMapGroupVoteOptions[i] = -1;
		}
	}

	// Print the maps chosen in the vote to console
	for (int i = 0; i < m_iVoteSize; i++)
	{
		int iMapIndex = g_pGameRules->m_nEndMatchMapGroupVoteOptions[i];
		Message("The %d-th chosen map index %d is %s\n", i, iMapIndex, GetMapName(iMapIndex));
	}

	// Start the end-of-vote timer to finish the vote
	// CONVAR_TODO
	ConVar* pVoteTimeCvar = g_pCVar->GetConVar(g_pCVar->FindConVar("mp_endmatch_votenextleveltime"));
	float flVoteTime = *(float*)&pVoteTimeCvar->values;
	new CTimer(flVoteTime, false, true, []() {
		g_pMapVoteSystem->FinishVote();
		return -1.0;
	});
}

int CMapVoteSystem::GetTotalNominations(int iMapIndex)
{
	int iNumNominations = 0;

	if (!GetGlobals())
		return iNumNominations;

	for (int i = 0; i < GetGlobals()->maxClients; i++)
	{
		auto pController = CCSPlayerController::FromSlot(i);
		if (pController && pController->IsConnected() && m_arrPlayerNominations[i] == iMapIndex)
			iNumNominations++;
	}
	return iNumNominations;
}

void CMapVoteSystem::FinishVote()
{
	if (!m_bIsVoteOngoing || !g_pGameRules)
		return;

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
		if (iNextMapVoteIndex == -1)
		{
			Panic("Failed to count map votes, file a bug\n");
			iNextMapVoteIndex = 0;
		}

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
	if (!bIsNextMapForced && GetGlobals())
	{
		int arrMapVotes[10] = {0};
		Message("Map vote result --- total votes per map:\n");
		for (int i = 0; i < GetGlobals()->maxClients; i++)
		{
			auto pController = CCSPlayerController::FromSlot(i);
			int iPlayerVotedIndex = m_arrPlayerVotes[i];
			if (pController && pController->IsConnected() && iPlayerVotedIndex >= 0)
				arrMapVotes[iPlayerVotedIndex]++;
		}
		for (int i = 0; i < m_iVoteSize; i++)
		{
			int iMapIndex = g_pGameRules->m_nEndMatchMapGroupVoteOptions[i];
			Message("- %s got %d votes\n", GetMapName(iMapIndex), arrMapVotes[i]);
		}
	}

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
	if (!pController || !m_bIsVoteOngoing || !g_pGameRules) return false;
	if (iVoteOption < 0 || iVoteOption >= m_iVoteSize) return false;

	// Filter out votes on invalid maps
	int iMapIndexToVote = g_pGameRules->m_nEndMatchMapGroupVoteOptions[iVoteOption];
	if (iMapIndexToVote < 0 || iMapIndexToVote >= GetMapListSize()) return false;

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
	if (iWinningMapIndex >= 0 && g_pGameRules)
	{
		g_pGameRules->m_nEndMatchMapVoteWinner = iWinningMapIndex;
		return true;
	}
	return false;
}

int CMapVoteSystem::WinningMapIndex()
{
	if (!GetGlobals())
		return -1;

	// Count the votes of every player
	int arrMapVotes[10] = {0};
	for (int i = 0; i < GetGlobals()->maxClients; i++)
	{
		auto pController = CCSPlayerController::FromSlot(i);
		if (pController && pController->IsConnected() && m_arrPlayerVotes[i] >= 0)
			arrMapVotes[m_arrPlayerVotes[i]]++;
	}

	// Identify the max. number of votes
	int iMaxVotes = 0;
	for (int i = 0; i < m_iVoteSize; i++)
		iMaxVotes = (arrMapVotes[i] > iMaxVotes) ? arrMapVotes[i] : iMaxVotes;

	// Identify how many maps are tied with the max number of votes
	int iMapsWithMaxVotes = 0;
	for (int i = 0; i < m_iVoteSize; i++)
		if (arrMapVotes[i] == iMaxVotes) iMapsWithMaxVotes++;

	// Break ties: 'random' map with the most votes
	int iWinningMapTieBreak = m_iRandomWinnerShift % iMapsWithMaxVotes;
	int iWinningMapCount = 0;
	for (int i = 0; i < m_iVoteSize; i++)
	{
		if (arrMapVotes[i] == iMaxVotes)
		{
			if (iWinningMapCount == iWinningMapTieBreak) return i;
			iWinningMapCount++;
		}
	}
	return -1;
}

std::unordered_map<int, int> CMapVoteSystem::GetNominatedMaps()
{
	std::unordered_map<int, int> mapNominatedMaps;

	if (!GetGlobals())
		return mapNominatedMaps;

	for (int i = 0; i < GetGlobals()->maxClients; i++)
	{
		CCSPlayerController* pController = CCSPlayerController::FromSlot(i);
		int iNominatedMapIndex = m_arrPlayerNominations[i];

		// Introduce nominated map indexes and count the total number
		if (iNominatedMapIndex != -1 && pController && pController->IsConnected() && IsMapIndexEnabled(iNominatedMapIndex))
			++mapNominatedMaps[iNominatedMapIndex];
	}

	return mapNominatedMaps;
}

std::vector<int> CMapVoteSystem::GetNominatedMapsForVote()
{
	std::unordered_map<int, int> mapOriginalNominatedMaps = GetNominatedMaps();		  // Original nominations map
	std::unordered_map<int, int> mapAvailableNominatedMaps(mapOriginalNominatedMaps); // A copy of the map that we can remove from without worry
	std::vector<int> vecTiedNominations;											  // Nominations with tied nom counts
	std::vector<int> vecChosenNominatedMaps;										  // Final vector of chosen nominations
	int iMapsToIncludeInNominate = std::min({(int)mapOriginalNominatedMaps.size(), GetMaxNominatedMaps(), GetMaxVoteMaps()});
	int iMostNominations;
	auto rng = std::default_random_engine{std::random_device{}()};

	// Select top maps by number of nominations
	while (vecChosenNominatedMaps.size() < iMapsToIncludeInNominate)
	{
		if (vecTiedNominations.size() == 0)
		{
			// Find highest nomination count
			iMostNominations = std::max_element(
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
		vecChosenNominatedMaps.push_back(vecTiedNominations.front());
		mapAvailableNominatedMaps.erase(vecTiedNominations.front());
		vecTiedNominations.erase(vecTiedNominations.begin());
	}

	if (!GetGlobals())
		return vecChosenNominatedMaps;

	// Notify nomination owners about the state of their nominations
	for (int i = 0; i < GetGlobals()->maxClients; i++)
	{
		int iNominatedMapIndex = m_arrPlayerNominations[i];
		CCSPlayerController* pController = CCSPlayerController::FromSlot(i);

		if (!pController || !pController->IsConnected())
			continue;

		// Ignore unset nominations (negative index)
		if (iNominatedMapIndex < 0)
			continue;

		int iNominations = mapOriginalNominatedMaps[iNominatedMapIndex];
		// At this point, iMostNominations represents nomination count of last map to make the map vote
		int iNominationsNeeded = iMostNominations - iNominations;

		// Bad RNG, needed 1 more for guaranteed selection then
		if (iNominationsNeeded == 0)
			iNominationsNeeded = 1;

		if (std::find(vecChosenNominatedMaps.begin(), vecChosenNominatedMaps.end(), iNominatedMapIndex) != vecChosenNominatedMaps.end())
			ClientPrint(pController, HUD_PRINTTALK, CHAT_PREFIX "Your \x06%s\x01 nomination made it to the map vote with \x06%i nomination%s\x01.", GetMapName(iNominatedMapIndex), iNominations, iNominations > 1 ? "s" : "");
		else
			ClientPrint(pController, HUD_PRINTTALK, CHAT_PREFIX "Your \x06%s\x01 nomination failed to make the map vote, it needed \x06%i more nomination%s\x01 for a total of \x06%i nominations\x01.",
						GetMapName(iNominatedMapIndex), iNominationsNeeded, iNominationsNeeded > 1 ? "s" : "", iNominations + iNominationsNeeded);
	}

	return vecChosenNominatedMaps;
}

std::vector<int> CMapVoteSystem::GetMapIndexesFromSubstring(const char* sMapSubstring)
{
	std::vector<int> vecMaps;

	for (int i = 0; i < GetMapListSize(); i++)
		if (V_stristr(GetMapName(i), sMapSubstring))
			vecMaps.push_back(i);

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

void CMapVoteSystem::ClearPlayerInfo(int iSlot)
{
	if (!g_bVoteManagerEnable)
		return;

	m_arrPlayerNominations[iSlot] = -1;
	m_arrPlayerVotes[iSlot] = -1;
}

int CMapVoteSystem::GetMapIndexFromString(const char* pszMapString)
{
	for (int i = 0; i < GetMapListSize(); i++)
		if (!V_strcasecmp(GetMapName(i), pszMapString))
			return i;

	return -1;
}

std::shared_ptr<CGroup> CMapVoteSystem::GetGroupFromString(const char* pszName)
{
	for (int i = 0; i < m_vecGroups.size(); i++)
		if (!V_strcmp(m_vecGroups[i]->GetName(), pszName))
			return m_vecGroups[i];

	return nullptr;
}

void CMapVoteSystem::AttemptNomination(CCSPlayerController* pController, const char* sMapSubstring)
{
	int iSlot = pController->GetPlayerSlot();
	ZEPlayer* pPlayer = g_playerManager->GetPlayer(iSlot);

	if (!pPlayer || !GetGlobals())
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

	if (GetMapCooldown(iFoundIndex)->IsOnCooldown())
	{
		ClientPrint(pController, HUD_PRINTTALK, CHAT_PREFIX "Cannot nominate \x06%s\x01 because it's on a %s cooldown.", GetMapName(iFoundIndex), GetMapCooldownText(iFoundIndex, false).c_str());
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

	if (pPlayer->GetNominateTime() + 60.0f > GetGlobals()->curtime)
	{
		int iRemainingTime = (int)(pPlayer->GetNominateTime() + 60.0f - GetGlobals()->curtime);
		ClientPrint(pController, HUD_PRINTTALK, CHAT_PREFIX "Wait %i seconds before you can nominate again.", iRemainingTime);
		return;
	}

	m_arrPlayerNominations[iSlot] = iFoundIndex;
	int iNominations = GetTotalNominations(iFoundIndex);

	ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX "\x06%s \x01was nominated by %s. It now has %d nomination%s.", GetMapName(iFoundIndex), pController->GetPlayerName(), iNominations, iNominations > 1 ? "s" : "");
	pPlayer->SetNominateTime(GetGlobals()->curtime);
}

void CMapVoteSystem::PrintMapList(CCSPlayerController* pController)
{
	std::vector<std::pair<int, std::string>> vecSortedMaps;
	int iPlayerCount = g_playerManager->GetOnlinePlayerCount(false);

	for (int i = 0; i < GetMapListSize(); i++)
		if (GetMapEnabledStatus(i))
			vecSortedMaps.push_back(std::make_pair(i, GetMapName(i)));

	std::sort(vecSortedMaps.begin(), vecSortedMaps.end(), [](auto left, auto right) {
		return V_strcasecmp(right.second.c_str(), left.second.c_str()) > 0;
	});

	ClientPrint(pController, HUD_PRINTTALK, CHAT_PREFIX "The list of all maps will be shown in console.");
	ClientPrint(pController, HUD_PRINTCONSOLE, "The list of all maps is:");

	for (std::pair<int, std::string> pair : vecSortedMaps)
	{
		int mapIndex = pair.first;
		const char* name = pair.second.c_str();
		int minPlayers = GetMapMinPlayers(mapIndex);
		int maxPlayers = GetMapMaxPlayers(mapIndex);

		if (mapIndex == GetCurrentMapIndex())
			ClientPrint(pController, HUD_PRINTCONSOLE, "- %s - Current Map", name);
		else if (GetMapCooldown(mapIndex)->IsOnCooldown())
			ClientPrint(pController, HUD_PRINTCONSOLE, "- %s - Cooldown: %s", name, GetMapCooldownText(mapIndex, true).c_str());
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
	m_vecMapList.clear();
	m_vecGroups.clear();
	m_vecCooldowns.clear();

	const char* pszJsonPath = "addons/cs2fixes/configs/maplist.jsonc";
	char szPath[MAX_PATH];
	V_snprintf(szPath, sizeof(szPath), "%s%s%s", Plat_GetGameDirectory(), "/csgo/", pszJsonPath);
	std::ifstream jsonFile(szPath);

	if (!jsonFile.is_open())
	{
		if (!ConvertMapListKVToJSON())
		{
			Panic("Failed to open %s and convert KV1 maplist.cfg to JSON format, map list not loaded!\n", pszJsonPath);
			return false;
		}

		jsonFile.open(szPath);
	}

	ordered_json jsonMaps = ordered_json::parse(jsonFile, nullptr, false, true);

	if (jsonMaps.is_discarded())
	{
		Panic("Failed parsing JSON from %s, map list not loaded!\n", pszJsonPath);
		return false;
	}

	// Load map cooldowns from file
	KeyValues* pKVcooldowns = new KeyValues("cooldowns");
	KeyValues::AutoDelete autoDeleteKVcooldowns(pKVcooldowns);
	const char* pszCooldownFilePath = "addons/cs2fixes/data/cooldowns.txt";
	if (!pKVcooldowns->LoadFromFile(g_pFullFileSystem, pszCooldownFilePath))
		Message("Failed to load cooldown file at %s - resetting all cooldowns to 0\n", pszCooldownFilePath);

	for (KeyValues* pKey = pKVcooldowns->GetFirstSubKey(); pKey; pKey = pKey->GetNextKey())
	{
		time_t timeCooldown = pKey->GetUint64();

		if (timeCooldown > std::time(0))
		{
			std::shared_ptr<CCooldown> pCooldown = std::make_shared<CCooldown>(pKey->GetName());

			pCooldown->SetTimeCooldown(timeCooldown);
			m_vecCooldowns.push_back(pCooldown);
		}
	}

	for (auto& [sSection, jsonSection] : jsonMaps.items())
	{
		for (auto& [sEntry, jsonEntry] : jsonSection.items())
		{
			if (sSection == "Groups")
			{
				m_vecGroups.push_back(std::make_shared<CGroup>(sEntry, jsonEntry.value("enabled", true), jsonEntry.value("cooldown", 0.0f)));
			}
			else if (sSection == "Maps")
			{
				// Seems like uint64 needs special handling
				uint64 iWorkshopId = 0;

				if (jsonEntry.contains("workshop_id"))
					iWorkshopId = jsonEntry["workshop_id"].get<uint64>();

				bool bIsEnabled = jsonEntry.value("enabled", true);
				int iMinPlayers = jsonEntry.value("min_players", 0);
				int iMaxPlayers = jsonEntry.value("max_players", 64);
				float fCooldown = jsonEntry.value("cooldown", 0.0f);
				std::vector<std::string> vecGroups;

				if (jsonEntry.contains("groups") && jsonEntry["groups"].size() > 0)
					for (auto& [key, group] : jsonEntry["groups"].items())
						vecGroups.push_back(group);

				if (iWorkshopId != 0)
					QueueMapDownload(iWorkshopId);

				// We just append the maps to the map list
				m_vecMapList.push_back(std::make_shared<CMap>(sEntry, iWorkshopId, bIsEnabled, iMinPlayers, iMaxPlayers, fCooldown, vecGroups));
			}
		}
	}

	new CTimer(0.f, true, true, []() {
		if (g_pMapVoteSystem->GetDownloadQueueSize() == 0)
			return -1.f;

		g_pMapVoteSystem->PrintDownloadProgress();

		return 1.f;
	});

	// Print all the maps
	for (int i = 0; i < GetMapListSize(); i++)
	{
		std::shared_ptr<CMap> map = m_vecMapList[i];
		std::string groups = "";

		for (std::string group : map->GetGroups())
			groups += group + ", ";

		if (groups != "")
			groups.erase(groups.length() - 2);

		if (map->GetWorkshopId() == 0)
			ConMsg("Map %d is %s, which is %s. MinPlayers: %d MaxPlayers: %d Custom Cooldown: %.2f Groups: %s\n", i, map->GetName(), map->IsEnabled() ? "enabled" : "disabled", map->GetMinPlayers(), map->GetMaxPlayers(), map->GetCustomCooldown(), groups.c_str());
		else
			ConMsg("Map %d is %s with workshop id %llu, which is %s. MinPlayers: %d MaxPlayers: %d Custom Cooldown: %.2f Groups: %s\n", i, map->GetName(), map->GetWorkshopId(), map->IsEnabled() ? "enabled" : "disabled", map->GetMinPlayers(), map->GetMaxPlayers(), map->GetCustomCooldown(), groups.c_str());
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

bool CMapVoteSystem::WriteMapCooldownsToFile()
{
	KeyValues* pKV = new KeyValues("cooldowns");
	KeyValues::AutoDelete autoDelete(pKV);

	const char* pszPath = "addons/cs2fixes/data/cooldowns.txt";

	for (std::shared_ptr<CCooldown> pCooldown : m_vecCooldowns)
		if (pCooldown->GetTimeCooldown() > std::time(0))
			pKV->AddUint64(pCooldown->GetMapName(), pCooldown->GetTimeCooldown());

	if (!pKV->SaveToFile(g_pFullFileSystem, pszPath))
	{
		Panic("Failed to write cooldowns to file: %s\n", pszPath);
		return false;
	}

	return true;
}

void CMapVoteSystem::ClearInvalidNominations()
{
	if (!g_bVoteManagerEnable || m_bIsVoteOngoing || !GetGlobals())
		return;

	for (int i = 0; i < GetGlobals()->maxClients; i++)
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

void CMapVoteSystem::UpdateCurrentMapIndex()
{
	for (int i = 0; i < GetMapListSize(); i++)
	{
		if (!V_strcasecmp(GetMapName(i), GetCurrentMapName()) || (GetCurrentWorkshopMap() != 0 && GetCurrentWorkshopMap() == GetMapWorkshopId(i)))
		{
			m_iCurrentMapIndex = i;
			return;
		}
	}

	m_iCurrentMapIndex = -1;
}

void CMapVoteSystem::ApplyGameSettings(KeyValues* pKV)
{
	if (!g_bVoteManagerEnable)
		return;

	if (pKV->FindKey("launchoptions") && pKV->FindKey("launchoptions")->FindKey("customgamemode"))
		SetCurrentWorkshopMap(pKV->FindKey("launchoptions")->GetUint64("customgamemode"));
	else
		SetCurrentWorkshopMap(0);

	if (pKV->FindKey("launchoptions") && pKV->FindKey("launchoptions")->FindKey("levelname"))
		SetCurrentMapName(pKV->FindKey("launchoptions")->GetString("levelname"));
	else
		SetCurrentMapName("MISSING_MAP");

	UpdateCurrentMapIndex();
	ProcessGroupCooldowns();
}

void CMapVoteSystem::OnLevelShutdown()
{
	// Put the map on cooldown as we transition to the next map
	PutMapOnCooldown(GetCurrentMapName());

	// Fully apply pending group cooldowns
	for (std::shared_ptr<CCooldown> pCooldown : m_vecCooldowns)
		if (pCooldown->GetPendingCooldown() > 0.0f)
			PutMapOnCooldown(pCooldown->GetMapName(), pCooldown->GetPendingCooldown());

	WriteMapCooldownsToFile();
}

std::string CMapVoteSystem::ConvertFloatToString(float fValue, int precision)
{
	std::stringstream stream;
	stream.precision(precision);
	stream << std::fixed << fValue;
	std::string str = stream.str();

	// Ensure that there is a decimal point somewhere (there should be)
	if (str.find('.') != std::string::npos)
	{
		// Remove trailing zeroes
		str = str.substr(0, str.find_last_not_of('0') + 1);
		// If the decimal point is now the last character, remove that as well
		if (str.find('.') == str.size() - 1)
			str = str.substr(0, str.size() - 1);
	}

	return str;
}

std::string CMapVoteSystem::StringToLower(std::string sValue)
{
	for (int i = 0; sValue[i]; i++)
		sValue[i] = tolower(sValue[i]);

	return sValue;
}

std::string CMapVoteSystem::GetMapCooldownText(const char* pszMapName, bool bPlural)
{
	std::shared_ptr<CCooldown> pCooldown = GetMapCooldown(pszMapName);
	float fValue;
	std::string response;

	if (pCooldown->GetCurrentCooldown() > 23.99f)
	{
		fValue = roundf(pCooldown->GetCurrentCooldown() / 24 * 100) / 100;
		response = ConvertFloatToString(fValue, 2) + " day";
	}
	else if (pCooldown->GetCurrentCooldown() <= 0.995f)
	{
		fValue = roundf(pCooldown->GetCurrentCooldown() * 60);

		// Rounding edge case
		if (fValue == 0.0f)
			fValue = 1.0f;

		response = ConvertFloatToString(fValue, 0) + " minute";
	}
	else
	{
		fValue = roundf(pCooldown->GetCurrentCooldown() * 100) / 100;
		response = ConvertFloatToString(fValue, 2) + " hour";
	}

	if (bPlural && fValue != 1.0f)
		response += "s";

	if (pCooldown->IsPending())
		response += " pending";

	return response;
}

void CMapVoteSystem::PutMapOnCooldown(const char* pszMapName, float fCooldown)
{
	if (g_bDisableCooldowns)
		return;

	int iMapIndex = GetMapIndexFromString(pszMapName);

	// If custom cooldown wasn't passed, use the normal cooldown for this map
	if (fCooldown == 0.0f)
	{
		if (iMapIndex != -1 && GetMapCustomCooldown(iMapIndex) != 0.0f)
			fCooldown = GetMapCustomCooldown(iMapIndex);
		else
			fCooldown = GetDefaultMapCooldown();
	}

	time_t timeCooldown = std::time(0) + (time_t)(fCooldown * 60 * 60);
	std::shared_ptr<CCooldown> pCooldown = GetMapCooldown(pszMapName);

	// Ensure we don't overwrite a longer cooldown
	if (pCooldown->GetTimeCooldown() < timeCooldown)
	{
		pCooldown->SetTimeCooldown(timeCooldown);

		// Pending cooldown should be invalidated at this point
		pCooldown->SetPendingCooldown(0.0f);
	}
}

void CMapVoteSystem::ProcessGroupCooldowns()
{
	int iCurrentMapIndex = GetCurrentMapIndex();

	if (iCurrentMapIndex == -1)
		return;

	std::vector<std::string> vecCurrentMapGroups = m_vecMapList[iCurrentMapIndex]->GetGroups();

	for (std::string groupName : vecCurrentMapGroups)
	{
		std::shared_ptr<CGroup> pGroup = GetGroupFromString(groupName.c_str());

		if (!pGroup)
		{
			Panic("Invalid group name %s defined for map %s\n", groupName.c_str(), GetMapName(iCurrentMapIndex));
			continue;
		}

		if (!pGroup->IsEnabled())
			continue;

		// Check entire map list for other maps in this group, and give them the group cooldown (pending)
		for (int i = 0; i < GetMapListSize(); i++)
		{
			if (iCurrentMapIndex != i && GetMapEnabledStatus(i) && m_vecMapList[i]->HasGroup(groupName))
			{
				float fCooldown = pGroup->GetCooldown() == 0.0f ? GetDefaultMapCooldown() : pGroup->GetCooldown();
				std::shared_ptr<CCooldown> pCooldown = GetMapCooldown(i);

				// Ensure we don't overwrite a longer cooldown
				if (pCooldown->GetPendingCooldown() < fCooldown)
					pCooldown->SetPendingCooldown(fCooldown);
			}
		}
	}
}

std::shared_ptr<CCooldown> CMapVoteSystem::GetMapCooldown(const char* pszMapName)
{
	for (std::shared_ptr<CCooldown> pCooldown : m_vecCooldowns)
		if (!V_strcasecmp(pszMapName, pCooldown->GetMapName()))
			return pCooldown;

	// Never been on cooldown, create new object
	std::shared_ptr<CCooldown> pCooldown = std::make_shared<CCooldown>(pszMapName);
	m_vecCooldowns.push_back(pCooldown);

	return pCooldown;
}

float CCooldown::GetCurrentCooldown()
{
	time_t timeCurrent = std::time(0);

	// Use pending cooldown first if present
	float fRemainingTime = GetPendingCooldown();

	// Calculate decimal hours
	float fCurrentRemainingTime = (GetTimeCooldown() - timeCurrent) / 60.0f / 60.0f;

	// Check if current cooldown should override
	if (GetTimeCooldown() > timeCurrent && fCurrentRemainingTime > fRemainingTime)
		fRemainingTime = fCurrentRemainingTime;

	return fRemainingTime;
}

// TODO: remove this once servers have been given at least a few months to update cs2fixes
bool CMapVoteSystem::ConvertMapListKVToJSON()
{
	Message("Attempting to convert KV1 maplist.cfg to JSON format...\n");

	const char* pszPath = "addons/cs2fixes/configs/maplist.cfg";

	KeyValues* pKV = new KeyValues("maplist");
	KeyValues::AutoDelete autoDelete(pKV);

	if (!pKV->LoadFromFile(g_pFullFileSystem, pszPath))
	{
		Panic("Failed to load %s\n", pszPath);
		return false;
	}

	ordered_json jsonMapList;

	jsonMapList["Groups"] = ordered_json(ordered_json::value_t::object);

	for (KeyValues* pKey = pKV->GetFirstSubKey(); pKey; pKey = pKey->GetNextKey())
	{
		ordered_json jsonMap;

		if (pKey->FindKey("enabled"))
			jsonMap["enabled"] = pKey->GetBool("enabled");
		if (pKey->FindKey("workshop_id"))
			jsonMap["workshop_id"] = pKey->GetUint64("workshop_id");
		if (pKey->FindKey("min_players"))
			jsonMap["min_players"] = pKey->GetInt("min_players");
		if (pKey->FindKey("max_players"))
			jsonMap["max_players"] = pKey->GetInt("max_players");
		if (pKey->FindKey("cooldown"))
			jsonMap["cooldown"] = pKey->GetInt("cooldown");

		jsonMapList["Maps"][pKey->GetName()] = jsonMap;
	}

	const char* pszJsonPath = "addons/cs2fixes/configs/maplist.jsonc";
	const char* pszKVConfigRenamePath = "addons/cs2fixes/configs/maplist_old.cfg";
	char szPath[MAX_PATH];
	V_snprintf(szPath, sizeof(szPath), "%s%s%s", Plat_GetGameDirectory(), "/csgo/", pszJsonPath);
	std::ofstream jsonFile(szPath);

	if (!jsonFile.is_open())
	{
		Panic("Failed to open %s\n", pszJsonPath);
		return false;
	}

	jsonFile << std::setfill('\t') << std::setw(1) << jsonMapList << std::endl;

	char szKVRenamePath[MAX_PATH];
	V_snprintf(szPath, sizeof(szPath), "%s%s%s", Plat_GetGameDirectory(), "/csgo/", pszPath);
	V_snprintf(szKVRenamePath, sizeof(szPath), "%s%s%s", Plat_GetGameDirectory(), "/csgo/", pszKVConfigRenamePath);

	std::rename(szPath, szKVRenamePath);

	// remove old cfg example if it exists
	const char* pszKVExamplePath = "addons/cs2fixes/configs/maplist.cfg.example";
	V_snprintf(szPath, sizeof(szPath), "%s%s%s", Plat_GetGameDirectory(), "/csgo/", pszKVExamplePath);
	std::remove(szPath);

	Message("Successfully converted KV1 maplist.cfg to JSON format at %s\n", pszJsonPath);
	return true;
}
