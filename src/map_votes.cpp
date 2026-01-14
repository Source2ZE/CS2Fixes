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

CMapVoteSystem* g_pMapVoteSystem = nullptr;

CConVar<float> g_cvarVoteMapsCooldown("cs2f_vote_maps_cooldown", FCVAR_NONE, "Default number of hours until a map can be played again i.e. cooldown", 6.0f);
CConVar<float> g_cvarVoteMapsCooldownRng("cs2f_vote_maps_cooldown_rng", FCVAR_NONE, "Randomness range in both directions to apply to map cooldowns", 0.0f);
CConVar<int> g_cvarVoteMaxNominations("cs2f_vote_max_nominations", FCVAR_NONE, "Number of nominations to include per vote, out of a maximum of 10", 10, true, 0, true, 10);
CConVar<int> g_cvarVoteMaxMaps("cs2f_vote_max_maps", FCVAR_NONE, "Number of total maps to include per vote, including nominations, out of a maximum of 10", 10, true, 2, true, 10);

CON_COMMAND_CHAT_FLAGS(reload_map_list, "- Reload map list, also reloads current map on completion", ADMFLAG_ROOT)
{
	if (!g_cvarVoteManagerEnable.Get())
		return;

	if (g_pMapVoteSystem->ReloadMapList())
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Map list reloaded!");
	else
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Failed to reload map list!");
}

CON_COMMAND_CHAT_FLAGS(map, "<name/id> - Change map", ADMFLAG_CHANGEMAP)
{
	if (!g_cvarVoteManagerEnable.Get())
		return;

	if (args.ArgC() < 2)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Usage: !map <name/id>");
		return;
	}

	g_pMapVoteSystem->HandlePlayerMapLookup(player, args[1], true, [](std::shared_ptr<CMap> pMap, CCSPlayerController* pController) {
		CTimer::Create(5.0f, TIMERFLAG_MAP, [pMap]() {
			pMap->Load();
			return -1.0f;
		});

		ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX "Changing map to \x06%s\x01...", pMap->GetName());
	});
}

CON_COMMAND_CHAT_FLAGS(setnextmap, "[name/id] - Force next map (empty to clear forced next map)", ADMFLAG_CHANGEMAP)
{
	if (!g_cvarVoteManagerEnable.Get())
		return;

	g_pMapVoteSystem->ForceNextMap(player, args.ArgC() < 2 ? "" : args[1]);
}

CON_COMMAND_CHAT(nominate, "[mapname] - Nominate a map (empty to clear nomination or list all maps)")
{
	if (!g_cvarVoteManagerEnable.Get() || !player)
		return;

	g_pMapVoteSystem->AttemptNomination(player, args.ArgC() < 2 ? "" : args[1]);
}

CON_COMMAND_CHAT(nomlist, "- List the list of nominations")
{
	if (!g_cvarVoteManagerEnable.Get())
		return;

	if (g_pMapVoteSystem->GetForcedNextMap())
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Nominations are disabled because the next map has been forced to \x06%s\x01.", g_pMapVoteSystem->GetForcedNextMap()->GetName());
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
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "- %s (%d time%s)\n", g_pMapVoteSystem->GetMapName(pair.first), pair.second, pair.second > 1 ? "s" : "");
}

CON_COMMAND_CHAT(mapcooldowns, "- List the maps currently in cooldown")
{
	if (!g_cvarVoteManagerEnable.Get())
		return;

	// Use a new vector, because we want to sort the command output
	std::vector<std::pair<std::string, float>> vecCooldowns;

	for (std::shared_ptr<CCooldown> pCooldown : g_pMapVoteSystem->GetMapCooldowns())
	{
		// Only print maps that are added to maplist.cfg
		if (pCooldown->IsOnCooldown() && g_pMapVoteSystem->GetMapFromString(pCooldown->GetMapName()))
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
	if (!g_cvarVoteManagerEnable.Get())
		return;

	if (!g_pMapVoteSystem->GetForcedNextMap())
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Next map is pending vote, no map has been forced.");
		return;
	}

	ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Next map is \x06%s\x01.", g_pMapVoteSystem->GetForcedNextMap()->GetName());
}

CON_COMMAND_CHAT(maplist, "- List the maps in the server")
{
	g_pMapVoteSystem->PrintMapList(player);
}

bool CMap::IsAvailable()
{
	auto pThis = shared_from_this();

	if (GetCooldown()->IsOnCooldown())
		return false;

	if (*g_pMapVoteSystem->GetCurrentMap() == *pThis)
		return false;

	if (!IsEnabled())
		return false;

	int iOnlinePlayers = g_playerManager->GetOnlinePlayerCount(false);
	bool bMeetsMaxPlayers = iOnlinePlayers <= GetMaxPlayers();
	bool bMeetsMinPlayers = iOnlinePlayers >= GetMinPlayers();
	return bMeetsMaxPlayers && bMeetsMinPlayers;
}

bool CMap::Load()
{
	if (IsMissingIdentifier())
		return false;

	if (GetWorkshopId() == 0)
		g_pEngineServer2->ChangeLevel(GetName(), nullptr);
	else
		g_pEngineServer2->ServerCommand(("host_workshop_map " + std::to_string(GetWorkshopId())).c_str());

	return true;
}

std::shared_ptr<CCooldown> CMap::GetCooldown()
{
	return g_pMapVoteSystem->GetMapCooldown(GetName());
}

std::string CMap::GetCooldownText(bool bPlural)
{
	return g_pMapVoteSystem->GetMapCooldownText(GetName(), bPlural);
}

void CMapVoteSystem::OnLevelInit(const char* pszMapName)
{
	if (!g_cvarVoteManagerEnable.Get())
		return;

	m_bIsVoteOngoing = false;
	m_bIntermissionStarted = false;
	m_pForcedNextMap = nullptr;

	for (int i = 0; i < MAXPLAYERS; i++)
		ClearPlayerInfo(i);

	// Delay one tick to override any .cfg's
	CTimer::Create(0.02f, TIMERFLAG_MAP, []() {
		g_pEngineServer2->ServerCommand("mp_match_end_changelevel 0");
		g_pEngineServer2->ServerCommand("mp_endmatch_votenextmap 1");

		return -1.0f;
	});
}

void CMapVoteSystem::StartVote()
{
	if (!g_cvarVoteManagerEnable.Get() || !g_pGameRules)
		return;

	m_bIsVoteOngoing = true;

	// Select random maps that meet requirements to appear
	std::vector<int> vecPossibleMaps;
	for (int i = 0; i < GetMapListSize(); i++)
		if (GetMapByIndex(i)->IsAvailable())
			vecPossibleMaps.push_back(i);

	m_iVoteSize = std::min((int)vecPossibleMaps.size(), g_cvarVoteMaxMaps.Get());
	bool bAbort = false;

	if (GetForcedNextMap())
	{
		CTimer::Create(6.0f, TIMERFLAG_MAP, []() {
			g_pMapVoteSystem->FinishVote();
			return -1.0f;
		});

		bAbort = true;
	}
	else if (m_iVoteSize < 2)
	{
		ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX "Not enough maps available for map vote, aborting! Please have an admin loosen map limits.");
		Message("Not enough maps available for map vote, aborting!\n");
		m_bIsVoteOngoing = false;
		bAbort = true;

		// Reload the current map as a fallback
		// Previously we fell back to game behaviour which could choose a random map in mapgroup, but a crash bug with default map changes was introduced in 2025-05-07 CS2 update
		CTimer::Create(6.0f, TIMERFLAG_MAP, []() {
			g_pMapVoteSystem->GetCurrentMap()->Load();
			return -1.0f;
		});
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

	for (int i = 0; i < m_iVoteSize; i++)
	{
		int iMapIndex = g_pGameRules->m_nEndMatchMapGroupVoteOptions[i];
		Message("The %d-th chosen map index %d is %s\n", i, iMapIndex, GetMapName(iMapIndex));
	}

	static ConVarRefAbstract mp_endmatch_votenextleveltime("mp_endmatch_votenextleveltime");
	static ConVarRefAbstract mp_match_restart_delay("mp_match_restart_delay");

	// Game logic seems to be higher value wins
	float flVoteTime = std::max(mp_endmatch_votenextleveltime.GetFloat(), mp_match_restart_delay.GetFloat());

	// But it's also always 3 seconds off
	flVoteTime = flVoteTime - 3.0f;

	CTimer::Create(flVoteTime, TIMERFLAG_MAP, []() {
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
	char buffer[256];
	std::shared_ptr<CMap> pNextMap;

	if (GetForcedNextMap())
	{
		pNextMap = GetForcedNextMap();
	}
	else
	{
		if (iNextMapVoteIndex == -1)
		{
			Panic("Failed to count map votes, file a bug\n");
			iNextMapVoteIndex = 0;
		}

		g_pGameRules->m_nEndMatchMapVoteWinner = iNextMapVoteIndex;
		pNextMap = GetMapByIndex(g_pGameRules->m_nEndMatchMapGroupVoteOptions[iNextMapVoteIndex]);
	}

	// Print out the map we're changing to
	if (GetForcedNextMap())
		V_snprintf(buffer, sizeof(buffer), "The vote was overriden. \x06%s\x01 will be the next map!\n", pNextMap->GetName());
	else if (bIsNextMapVoted)
		V_snprintf(buffer, sizeof(buffer), "The vote has ended. \x06%s\x01 will be the next map!\n", pNextMap->GetName());
	else
		V_snprintf(buffer, sizeof(buffer), "No map was chosen. \x06%s\x01 will be the next map!\n", pNextMap->GetName());

	ClientPrintAll(HUD_PRINTTALK, buffer);
	Message(buffer);

	// Print vote result information: how many votes did each map get?
	if (!GetForcedNextMap() && GetGlobals())
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
	CTimer::Create(1.0, TIMERFLAG_MAP, [pNextMap]() {
		pNextMap->Load();
		return -1.0f;
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

	Message("Adding vote to map %i (%s) for player %s (slot %i).\n", iVoteOption, GetMapName(iMapIndexToVote), pController->GetPlayerName(), iSlot);

	// Update the winning map for every player vote
	UpdateWinningMap();

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
		if (iNominatedMapIndex != -1 && pController && pController->IsConnected() && GetMapByIndex(iNominatedMapIndex)->IsAvailable())
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
	int iMapsToIncludeInNominate = std::min({(int)mapOriginalNominatedMaps.size(), g_cvarVoteMaxNominations.Get(), g_cvarVoteMaxMaps.Get()});
	int iMostNominations = 0;
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

std::vector<std::shared_ptr<CMap>> CMapVoteSystem::GetMapsFromSubstring(const char* pszMapSubstring)
{
	std::vector<std::shared_ptr<CMap>> vecMaps;

	for (auto pMap : GetMapList())
		if (V_stristr(pMap->GetName(), pszMapSubstring))
			vecMaps.push_back(pMap);

	return vecMaps;
}

void CMapVoteSystem::HandlePlayerMapLookup(CCSPlayerController* pController, std::string strMapSubstring, bool bAdmin, QueryCallback_t callbackSuccess)
{
	strMapSubstring = StringToLower(strMapSubstring);
	const char* pszMapSubstring = strMapSubstring.c_str();
	auto vecFoundMaps = GetMapsFromSubstring(pszMapSubstring);

	// Don't list disabled maps in non-admin commands
	if (!bAdmin)
	{
		auto iterator = vecFoundMaps.begin();

		while (iterator != vecFoundMaps.end())
		{
			// Only erase if vector has multiple elements, so we can still give "map disabled" output in single-match scenarios
			if (!(*iterator)->IsEnabled() && vecFoundMaps.size() > 1)
				vecFoundMaps.erase(iterator);
			else
				iterator++;
		}
	}

	if (vecFoundMaps.size() > 0)
	{
		if (vecFoundMaps.size() > 1)
		{
			// If we have an exact match here, just use that
			for (auto pMap : vecFoundMaps)
			{
				if (!V_strcmp(pMap->GetName(), pszMapSubstring))
				{
					callbackSuccess(pMap, pController);
					return;
				}
			}

			ClientPrint(pController, HUD_PRINTTALK, CHAT_PREFIX "Multiple maps matched \x06%s\x01, try being more specific:", pszMapSubstring);

			for (int i = 0; i < vecFoundMaps.size() && i < 5; i++)
				ClientPrint(pController, HUD_PRINTTALK, "- %s", vecFoundMaps[i]->GetName());
		}
		else
		{
			callbackSuccess(vecFoundMaps[0], pController);
		}

		return;
	}

	if (bAdmin)
	{
		uint64 iWorkshopId = V_StringToUint64(pszMapSubstring, 0, NULL, NULL, PARSING_FLAG_SKIP_WARNING);

		// Check if input is numeric (workshop ID)
		if (iWorkshopId != 0)
		{
			CWorkshopDetailsQuery::Create(iWorkshopId, pController, callbackSuccess);
			return;
		}

		if (g_pEngineServer2->IsMapValid(pszMapSubstring))
		{
			callbackSuccess(std::make_shared<CMap>(pszMapSubstring, 0), pController);
			return;
		}
	}

	ClientPrint(pController, HUD_PRINTTALK, CHAT_PREFIX "Failed to find a map matching \x06%s\x01.", pszMapSubstring);
}

void CMapVoteSystem::ClearPlayerInfo(int iSlot)
{
	if (!g_cvarVoteManagerEnable.Get())
		return;

	m_arrPlayerNominations[iSlot] = -1;
	m_arrPlayerVotes[iSlot] = -1;
}

std::shared_ptr<CMap> CMapVoteSystem::GetMapFromString(const char* pszMapString)
{
	for (auto pMap : m_vecMapList)
		if (!V_strcasecmp(pMap->GetName(), pszMapString))
			return pMap;

	return nullptr;
}

std::shared_ptr<CGroup> CMapVoteSystem::GetGroupFromString(const char* pszName)
{
	for (int i = 0; i < m_vecGroups.size(); i++)
		if (!V_strcmp(m_vecGroups[i]->GetName(), pszName))
			return m_vecGroups[i];

	return nullptr;
}

void CMapVoteSystem::AttemptNomination(CCSPlayerController* pController, const char* pszMapSubstring)
{
	int iSlot = pController->GetPlayerSlot();

	if (!GetGlobals())
		return;

	if (g_cvarVoteMaxNominations.Get() == 0)
	{
		ClientPrint(pController, HUD_PRINTTALK, CHAT_PREFIX "Nominations are currently disabled.");
		return;
	}

	if (GetForcedNextMap())
	{
		ClientPrint(pController, HUD_PRINTTALK, CHAT_PREFIX "Nominations are disabled because the next map has been forced to \x06%s\x01.", GetForcedNextMap()->GetName());
		return;
	}

	if (IsVoteOngoing())
	{
		ClientPrint(pController, HUD_PRINTTALK, CHAT_PREFIX "Nominations are disabled because the vote has already started.");
		return;
	}

	if (pszMapSubstring[0] == '\0')
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

	g_pMapVoteSystem->HandlePlayerMapLookup(pController, pszMapSubstring, false, [](std::shared_ptr<CMap> pMap, CCSPlayerController* pController) {
		int iPlayerCount = g_playerManager->GetOnlinePlayerCount(false);
		int iMapIndex = g_pMapVoteSystem->GetMapInfoByIdentifiers(pMap->GetName(), pMap->GetWorkshopId()).first;

		int iSlot = pController->GetPlayerSlot();
		ZEPlayer* pPlayer = g_playerManager->GetPlayer(iSlot);

		if (!pMap->IsEnabled())
		{
			ClientPrint(pController, HUD_PRINTTALK, CHAT_PREFIX "Cannot nominate \x06%s\x01 because it's disabled.", pMap->GetName());
			return;
		}

		if (*pMap == *g_pMapVoteSystem->GetCurrentMap())
		{
			ClientPrint(pController, HUD_PRINTTALK, CHAT_PREFIX "Cannot nominate \x06%s\x01 because it's already the current map!", pMap->GetName());
			return;
		}

		if (pMap->GetCooldown()->IsOnCooldown())
		{
			ClientPrint(pController, HUD_PRINTTALK, CHAT_PREFIX "Cannot nominate \x06%s\x01 because it's on a %s cooldown.", pMap->GetName(), pMap->GetCooldownText(false).c_str());
			return;
		}

		if (iPlayerCount < pMap->GetMinPlayers())
		{
			ClientPrint(pController, HUD_PRINTTALK, CHAT_PREFIX "Cannot nominate \x06%s\x01 because it needs %i more players.", pMap->GetName(), pMap->GetMinPlayers() - iPlayerCount);
			return;
		}

		if (iPlayerCount > pMap->GetMaxPlayers())
		{
			ClientPrint(pController, HUD_PRINTTALK, CHAT_PREFIX "Cannot nominate \x06%s\x01 because it needs %i less players.", pMap->GetName(), iPlayerCount - pMap->GetMaxPlayers());
			return;
		}

		if (pPlayer->GetNominateTime() + 60.0f > GetGlobals()->curtime)
		{
			int iRemainingTime = (int)(pPlayer->GetNominateTime() + 60.0f - GetGlobals()->curtime);
			ClientPrint(pController, HUD_PRINTTALK, CHAT_PREFIX "Wait %i seconds before you can nominate again.", iRemainingTime);
			return;
		}

		g_pMapVoteSystem->SetPlayerNomination(iSlot, iMapIndex);
		int iNominations = g_pMapVoteSystem->GetTotalNominations(iMapIndex);

		ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX "\x06%s \x01was nominated by %s. It now has %d nomination%s.", pMap->GetName(), pController->GetPlayerName(), iNominations, iNominations > 1 ? "s" : "");
		pPlayer->SetNominateTime(GetGlobals()->curtime);
	});
}

void CMapVoteSystem::PrintMapList(CCSPlayerController* pController)
{
	std::vector<std::shared_ptr<CMap>> vecSortedMaps;
	int iPlayerCount = g_playerManager->GetOnlinePlayerCount(false);

	for (auto pMap : m_vecMapList)
		if (pMap->IsEnabled())
			vecSortedMaps.push_back(pMap);

	std::sort(vecSortedMaps.begin(), vecSortedMaps.end(), [](auto left, auto right) {
		return V_strcasecmp(right->GetName(), left->GetName()) > 0;
	});

	ClientPrint(pController, HUD_PRINTTALK, CHAT_PREFIX "The list of all maps will be shown in console.");
	ClientPrint(pController, HUD_PRINTCONSOLE, "The list of all maps is:");

	for (auto pMap : vecSortedMaps)
	{
		const char* name = pMap->GetName();
		int minPlayers = pMap->GetMinPlayers();
		int maxPlayers = pMap->GetMaxPlayers();

		if (*pMap == *GetCurrentMap())
			ClientPrint(pController, HUD_PRINTCONSOLE, "- %s - Current Map", name);
		else if (pMap->GetCooldown()->IsOnCooldown())
			ClientPrint(pController, HUD_PRINTCONSOLE, "- %s - Cooldown: %s", name, pMap->GetCooldownText(true).c_str());
		else if (iPlayerCount < minPlayers)
			ClientPrint(pController, HUD_PRINTCONSOLE, "- %s - +%d Players", name, minPlayers - iPlayerCount);
		else if (iPlayerCount > maxPlayers)
			ClientPrint(pController, HUD_PRINTCONSOLE, "- %s - -%d Players", name, iPlayerCount - maxPlayers);
		else
			ClientPrint(pController, HUD_PRINTCONSOLE, "- %s", name);
	}
}

void CMapVoteSystem::ForceNextMap(CCSPlayerController* pController, const char* pszMapSubstring)
{
	if (pszMapSubstring[0] == '\0')
	{
		if (!GetForcedNextMap())
		{
			ClientPrint(pController, HUD_PRINTTALK, CHAT_PREFIX "There is no next map to reset!");
		}
		else
		{
			ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX "\x06%s \x01is no longer the forced next map.\n", GetForcedNextMap()->GetName());
			SetForcedNextMap(nullptr);
		}

		return;
	}

	g_pMapVoteSystem->HandlePlayerMapLookup(pController, pszMapSubstring, true, [](std::shared_ptr<CMap> pMap, CCSPlayerController* pController) {
		if (g_pMapVoteSystem->GetForcedNextMap() && *pMap == *g_pMapVoteSystem->GetForcedNextMap())
		{
			ClientPrint(pController, HUD_PRINTTALK, CHAT_PREFIX "\x06%s\x01 is already the next map!", g_pMapVoteSystem->GetForcedNextMap()->GetName());
			return;
		}

		// When found, print the map and store the forced map
		g_pMapVoteSystem->SetForcedNextMap(pMap);
		ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX "\x06%s \x01has been forced as the next map.\n", g_pMapVoteSystem->GetForcedNextMap()->GetName());
	});
}

void CMapVoteSystem::PrintDownloadProgress()
{
	if (GetDownloadQueueSize() == 0)
		return;

	uint64 iBytesDownloaded = 0;
	uint64 iTotalBytes = 0;

	if (!g_steamAPI.SteamUGC()->GetItemDownloadInfo(m_DownloadQueue.front(), &iBytesDownloaded, &iTotalBytes) || !iTotalBytes)
		return;

	double flMBDownloaded = (double)iBytesDownloaded / 1024 / 1024;
	double flTotalMB = (double)iTotalBytes / 1024 / 1024;

	double flProgress = (double)iBytesDownloaded / (double)iTotalBytes;
	flProgress *= 100.f;

	Message("Downloading map %lli: %.2f/%.2f MB (%.2f%%)\n", m_DownloadQueue.front(), flMBDownloaded, flTotalMB, flProgress);
}

void CMapVoteSystem::OnMapDownloaded(DownloadItemResult_t* pResult)
{
	if (std::find(m_DownloadQueue.begin(), m_DownloadQueue.end(), pResult->m_nPublishedFileId) == m_DownloadQueue.end())
		return;

	// Some weird rate limiting that's been observed? Back off for a while then retry download
	if (pResult->m_eResult == k_EResultNoConnection)
	{
		PublishedFileId_t workshopID = m_DownloadQueue.front();
		Message("Addon %llu download failed with status code 3, retrying in 2 minutes\n", workshopID);

		m_pRateLimitedDownloadTimer = CTimer::Create(120.0f, TIMERFLAG_NONE, [workshopID]() {
			g_steamAPI.SteamUGC()->DownloadItem(workshopID, false);

			return -1.0f;
		});

		return;
	}

	m_DownloadQueue.pop_front();

	if (GetDownloadQueueSize() == 0)
		return;

	g_steamAPI.SteamUGC()->DownloadItem(m_DownloadQueue.front(), false);
}

void CMapVoteSystem::QueueMapDownload(PublishedFileId_t iWorkshopId)
{
	if (std::find(m_DownloadQueue.begin(), m_DownloadQueue.end(), iWorkshopId) != m_DownloadQueue.end())
		return;

	m_DownloadQueue.push_back(iWorkshopId);

	if (m_DownloadQueue.front() == iWorkshopId)
		g_steamAPI.SteamUGC()->DownloadItem(iWorkshopId, false);
}

bool CMapVoteSystem::LoadMapList()
{
	// This is called when the Steam API is init'd, now is the time to register this
	m_CallbackDownloadItemResult.Register(this, &CMapVoteSystem::OnMapDownloaded);

	m_vecMapList.clear();
	m_vecGroups.clear();
	m_vecCooldowns.clear();

	const char* pszMapListPath = "addons/cs2fixes/configs/maplist.jsonc";
	char szPath[MAX_PATH];
	V_snprintf(szPath, sizeof(szPath), "%s%s%s", Plat_GetGameDirectory(), "/csgo/", pszMapListPath);
	std::ifstream mapListFile(szPath);

	if (!mapListFile.is_open())
	{
		Panic("Failed to open %s, map list not loaded!\n", pszMapListPath);
		return false;
	}

	ordered_json jsonMaps = ordered_json::parse(mapListFile, nullptr, false, true);

	if (jsonMaps.is_discarded())
	{
		Panic("Failed parsing JSON from %s, map list not loaded!\n", pszMapListPath);
		return false;
	}

	m_timeMapListModified = std::filesystem::last_write_time(szPath);
	LoadCooldowns();

	for (auto& [strSection, jsonSection] : jsonMaps.items())
	{
		for (auto& [strEntry, jsonEntry] : jsonSection.items())
		{
			if (strSection == "Groups")
			{
				m_vecGroups.push_back(std::make_shared<CGroup>(strEntry, jsonEntry.value("enabled", true), jsonEntry.value("cooldown", 0.0f)));
			}
			else if (strSection == "Maps")
			{
				// Seems like uint64 needs special handling
				uint64 iWorkshopId = 0;

				if (jsonEntry.contains("workshop_id"))
					iWorkshopId = jsonEntry["workshop_id"].get<uint64>();

				bool bIsEnabled = jsonEntry.value("enabled", true);
				std::string strDisplayName = jsonEntry.value("display_name", "");
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
				m_vecMapList.push_back(std::make_shared<CMap>(strEntry, iWorkshopId, false, strDisplayName, bIsEnabled, iMinPlayers, iMaxPlayers, fCooldown, vecGroups));
			}
		}
	}

	m_pDownloadProgressTimer = CTimer::Create(0.f, TIMERFLAG_NONE, []() {
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

bool CMapVoteSystem::LoadCooldowns()
{
	const char* pszCooldownsFilePath = "addons/cs2fixes/data/cooldowns.jsonc";
	char szPath[MAX_PATH];
	V_snprintf(szPath, sizeof(szPath), "%s%s%s", Plat_GetGameDirectory(), "/csgo/", pszCooldownsFilePath);
	std::ifstream cooldownsFile(szPath);

	if (!cooldownsFile.is_open())
	{
		if (!ConvertCooldownsKVToJSON())
		{
			Message("Failed to open %s and convert KV1 cooldowns.txt to JSON format, resetting all cooldowns to 0\n", pszCooldownsFilePath);
			return false;
		}

		cooldownsFile.open(szPath);
	}

	ordered_json jsonCooldownsRoot = ordered_json::parse(cooldownsFile, nullptr, false, true);

	if (jsonCooldownsRoot.is_discarded())
	{
		Message("Failed parsing JSON from %s, resetting all cooldowns to 0\n", pszCooldownsFilePath);
		return false;
	}

	ordered_json jsonCooldowns = jsonCooldownsRoot.value("Cooldowns", ordered_json());

	for (auto& [strMapName, iCooldown] : jsonCooldowns.items())
	{
		time_t timeCooldown = iCooldown;

		if (timeCooldown > std::time(0))
		{
			std::shared_ptr<CCooldown> pCooldown = std::make_shared<CCooldown>(strMapName);

			pCooldown->SetTimeCooldown(timeCooldown);
			m_vecCooldowns.push_back(pCooldown);
		}
	}

	return true;
}

bool CMapVoteSystem::IsIntermissionAllowed(bool bCheckOnly)
{
	// We need to prevent "ending the map twice" as it messes with ongoing map votes
	// This seems to be a CS2 bug that occurs when the round ends while already on the map end screen
	if (m_bIntermissionStarted)
		return false;

	// Should be false in GoToIntermission hook, true if checking anywhere else
	if (!bCheckOnly)
		m_bIntermissionStarted = true;

	return true;
}

CUtlStringList CMapVoteSystem::CreateWorkshopMapGroup()
{
	CUtlStringList mapList;

	for (auto pMap : m_vecMapList)
		mapList.CopyAndAddToTail(pMap->GetDisplayName());

	return mapList;
}

bool CMapVoteSystem::WriteMapCooldownsToFile()
{
	const char* pszJsonPath = "addons/cs2fixes/data/cooldowns.jsonc";
	char szPath[MAX_PATH];
	V_snprintf(szPath, sizeof(szPath), "%s%s%s", Plat_GetGameDirectory(), "/csgo/", pszJsonPath);
	std::ofstream jsonFile(szPath);
	ordered_json jsonCooldowns;

	if (!jsonFile.is_open())
	{
		Panic("Failed to open %s\n", pszJsonPath);
		return false;
	}

	jsonCooldowns["Cooldowns"] = ordered_json(ordered_json::value_t::object);

	for (std::shared_ptr<CCooldown> pCooldown : m_vecCooldowns)
		if (pCooldown->GetTimeCooldown() > std::time(0))
			jsonCooldowns["Cooldowns"][pCooldown->GetMapName()] = pCooldown->GetTimeCooldown();

	jsonFile << std::setfill('\t') << std::setw(1) << jsonCooldowns << std::endl;
	return true;
}

void CMapVoteSystem::ClearInvalidNominations()
{
	if (!g_cvarVoteManagerEnable.Get() || m_bIsVoteOngoing || !GetGlobals())
		return;

	for (int i = 0; i < GetGlobals()->maxClients; i++)
	{
		int iNominatedMapIndex = m_arrPlayerNominations[i];

		// Ignore unset nominations (negative index)
		if (iNominatedMapIndex < 0)
			continue;

		auto pMap = GetMapByIndex(iNominatedMapIndex);

		// Check if nominated index still meets criteria for nomination
		if (!pMap->IsEnabled())
		{
			ClearPlayerInfo(i);
			CCSPlayerController* pPlayer = CCSPlayerController::FromSlot(i);
			if (!pPlayer)
				continue;

			ClientPrint(pPlayer, HUD_PRINTTALK, CHAT_PREFIX "Your nomination for \x06%s \x01has been removed because the player count requirements are no longer met.", pMap->GetName());
		}
	}
}

void CMapVoteSystem::ApplyGameSettings(KeyValues* pKV)
{
	if (!g_cvarVoteManagerEnable.Get())
		return;

	const char* pszMapName;
	uint64 iWorkshopId;

	if (pKV->FindKey("launchoptions") && pKV->FindKey("launchoptions")->FindKey("levelname"))
		pszMapName = pKV->FindKey("launchoptions")->GetString("levelname");
	else
		pszMapName = "";

	if (pKV->FindKey("launchoptions") && pKV->FindKey("launchoptions")->FindKey("customgamemode"))
		iWorkshopId = pKV->FindKey("launchoptions")->GetUint64("customgamemode");
	else
		iWorkshopId = 0;

	auto pair = GetMapInfoByIdentifiers(pszMapName, iWorkshopId);

	if (pair.first != -1)
		SetCurrentMap(pair.second);
	else
		SetCurrentMap(std::make_shared<CMap>(pszMapName, iWorkshopId, pszMapName[0] == '\0' && iWorkshopId == 0));

	ProcessGroupCooldowns();
}

void CMapVoteSystem::OnLevelShutdown()
{
	// Put the map on cooldown as we transition to the next map
	PutMapOnCooldown(GetCurrentMap()->GetName());

	// Fully apply pending group cooldowns
	for (std::shared_ptr<CCooldown> pCooldown : m_vecCooldowns)
	{
		if (pCooldown->GetPendingCooldown() > 0.0f)
		{
			PutMapOnCooldown(pCooldown->GetMapName(), pCooldown->GetPendingCooldown());
			pCooldown->SetPendingCooldown(0.0f);
		}
	}

	if (IsMapListLoaded())
	{
		WriteMapCooldownsToFile();

		char szPath[MAX_PATH];
		V_snprintf(szPath, sizeof(szPath), "%s%s", Plat_GetGameDirectory(), "/csgo/addons/cs2fixes/configs/maplist.jsonc");

		// If maplist.jsonc was updated, automatically reload the map list without a map change (map is about to change anyways)
		if (m_timeMapListModified != std::filesystem::last_write_time(szPath))
			ReloadMapList(false);
	}
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

	auto pMap = GetMapFromString(pszMapName);

	// If custom cooldown wasn't passed, use the normal cooldown for this map
	if (fCooldown == 0.0f)
	{
		if (pMap && pMap->GetCustomCooldown() != 0.0f)
			fCooldown = pMap->GetCustomCooldown();
		else
			fCooldown = g_cvarVoteMapsCooldown.Get();

		// Add randomness if applicable
		if (g_cvarVoteMapsCooldown.Get() != 0.0f)
		{
			float flRandomValue = ((float)rand() / RAND_MAX) * g_cvarVoteMapsCooldownRng.Get();

			if (rand() % 2)
				fCooldown += flRandomValue;
			else
				fCooldown -= flRandomValue;
		}
	}

	time_t timeCooldown = std::time(0) + (time_t)(fCooldown * 60 * 60);
	std::shared_ptr<CCooldown> pCooldown = GetMapCooldown(pszMapName);

	// Ensure we don't overwrite a longer cooldown
	if (pCooldown->GetTimeCooldown() < timeCooldown)
		pCooldown->SetTimeCooldown(timeCooldown);
}

void CMapVoteSystem::ProcessGroupCooldowns()
{
	std::vector<std::string> vecCurrentMapGroups = GetCurrentMap()->GetGroups();

	for (std::string groupName : vecCurrentMapGroups)
	{
		std::shared_ptr<CGroup> pGroup = GetGroupFromString(groupName.c_str());

		if (!pGroup)
		{
			Panic("Invalid group name %s defined for map %s\n", groupName.c_str(), GetCurrentMap()->GetName());
			continue;
		}

		if (!pGroup->IsEnabled())
			continue;

		// Check entire map list for other maps in this group, and give them the group cooldown (pending)
		for (auto pMap : m_vecMapList)
		{
			if (*GetCurrentMap() != *pMap && pMap->IsEnabled() && pMap->HasGroup(groupName))
			{
				float fCooldown = pGroup->GetCooldown() == 0.0f ? g_cvarVoteMapsCooldown.Get() : pGroup->GetCooldown();
				std::shared_ptr<CCooldown> pCooldown = pMap->GetCooldown();

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

bool CMapVoteSystem::ReloadMapList(bool bReloadMap)
{
	if (g_pMapVoteSystem->GetDownloadQueueSize() != 0)
	{
		m_DownloadQueue.clear();

		if (!m_pDownloadProgressTimer.expired())
			m_pDownloadProgressTimer.lock()->Cancel();

		if (!m_pRateLimitedDownloadTimer.expired())
			m_pRateLimitedDownloadTimer.lock()->Cancel();
	}

	if (!g_pMapVoteSystem->LoadMapList())
		return false;

	// A CUtlStringList param is also expected, but we build it in our CreateWorkshopMapGroup pre-hook anyways
	CALL_VIRTUAL(void, g_GameConfig->GetOffset("IGameTypes_CreateWorkshopMapGroup"), g_pGameTypes, "workshop");

	// Updating the mapgroup requires reloading the map for everything to load properly
	if (bReloadMap)
		return GetCurrentMap()->Load();

	return true;
}

std::pair<int, std::shared_ptr<CMap>> CMapVoteSystem::GetMapInfoByIdentifiers(const char* pszMapName, uint64 iWorkshopId)
{
	for (int i = 0; i < GetMapListSize(); i++)
	{
		auto pMap = GetMapByIndex(i);

		if ((pMap->GetName()[0] != '\0' && !V_strcasecmp(pMap->GetName(), pszMapName)) || (pMap->GetWorkshopId() != 0 && pMap->GetWorkshopId() == iWorkshopId))
			return {i, pMap};
	}

	return {-1, nullptr};
}

std::shared_ptr<CWorkshopDetailsQuery> CWorkshopDetailsQuery::Create(uint64 iWorkshopId, CCSPlayerController* pController, QueryCallback_t callbackSuccess)
{
	uint64 iWorkshopIDArray[1] = {iWorkshopId};
	UGCQueryHandle_t hQuery = g_steamAPI.SteamUGC()->CreateQueryUGCDetailsRequest(iWorkshopIDArray, 1);

	if (hQuery == k_UGCQueryHandleInvalid)
	{
		ClientPrint(pController, HUD_PRINTTALK, CHAT_PREFIX "Failed to query workshop map information for ID \x06%llu\x01.", iWorkshopId);
		return nullptr;
	}

	g_steamAPI.SteamUGC()->SetAllowCachedResponse(hQuery, 0);
	SteamAPICall_t hCall = g_steamAPI.SteamUGC()->SendQueryUGCRequest(hQuery);

	auto pQuery = std::make_shared<CWorkshopDetailsQuery>(hQuery, iWorkshopId, pController, callbackSuccess);
	g_pMapVoteSystem->AddWorkshopDetailsQuery(pQuery);
	pQuery->m_CallResult.Set(hCall, pQuery.get(), &CWorkshopDetailsQuery::OnQueryCompleted);

	return pQuery;
}

void CWorkshopDetailsQuery::OnQueryCompleted(SteamUGCQueryCompleted_t* pCompletedQuery, bool bFailed)
{
	CCSPlayerController* pController = m_hController.Get();
	SteamUGCDetails_t details;

	// Only allow null controller if controller was originally null (console)
	if (m_bConsole || pController)
	{
		if (bFailed || pCompletedQuery->m_eResult != k_EResultOK || pCompletedQuery->m_unNumResultsReturned < 1 || !g_steamAPI.SteamUGC()->GetQueryUGCResult(pCompletedQuery->m_handle, 0, &details) || details.m_eResult != k_EResultOK)
		{
			ClientPrint(pController, HUD_PRINTTALK, CHAT_PREFIX "Failed to query workshop map information for ID \x06%llu\x01.", m_iWorkshopId);
		}
		else if (details.m_nConsumerAppID != 730 || details.m_eFileType != k_EWorkshopFileTypeCommunity)
		{
			ClientPrint(pController, HUD_PRINTTALK, CHAT_PREFIX "The ID \x06%llu\x01 is not a valid CS2 workshop map.", m_iWorkshopId);
		}
		else
		{
			// Try to get a head start on downloading the map if needed
			g_steamAPI.SteamUGC()->DownloadItem(m_iWorkshopId, false);
			m_callbackSuccess(std::make_shared<CMap>(details.m_rgchTitle, m_iWorkshopId), pController);
		}
	}

	g_steamAPI.SteamUGC()->ReleaseQueryUGCRequest(m_hQuery);
	g_pMapVoteSystem->RemoveWorkshopDetailsQuery(shared_from_this());
}

// TODO: remove this once servers have been given at least a few months to update cs2fixes
bool CMapVoteSystem::ConvertCooldownsKVToJSON()
{
	Message("Attempting to convert KV1 cooldowns.txt to JSON format...\n");

	const char* pszPath = "addons/cs2fixes/data/cooldowns.txt";
	KeyValues* pKV = new KeyValues("cooldowns");
	KeyValues::AutoDelete autoDelete(pKV);

	if (!pKV->LoadFromFile(g_pFullFileSystem, pszPath))
	{
		Panic("Failed to load %s\n", pszPath);
		return false;
	}

	ordered_json jsonCooldowns;

	jsonCooldowns["Cooldowns"] = ordered_json(ordered_json::value_t::object);

	for (KeyValues* pKey = pKV->GetFirstSubKey(); pKey; pKey = pKey->GetNextKey())
		jsonCooldowns["Cooldowns"][pKey->GetName()] = pKey->GetUint64();

	const char* pszJsonPath = "addons/cs2fixes/data/cooldowns.jsonc";
	char szPath[MAX_PATH];
	V_snprintf(szPath, sizeof(szPath), "%s%s%s", Plat_GetGameDirectory(), "/csgo/", pszJsonPath);
	std::ofstream jsonFile(szPath);

	if (!jsonFile.is_open())
	{
		Panic("Failed to open %s\n", pszJsonPath);
		return false;
	}

	jsonFile << std::setfill('\t') << std::setw(1) << jsonCooldowns << std::endl;

	// remove old file
	V_snprintf(szPath, sizeof(szPath), "%s%s%s", Plat_GetGameDirectory(), "/csgo/", pszPath);
	std::remove(szPath);

	Message("Successfully converted KV1 cooldowns.txt to JSON format at %s\n", pszJsonPath);
	return true;
}