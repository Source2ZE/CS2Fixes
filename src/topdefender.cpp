/**
 * =============================================================================
 * CS2Fixes
 * Copyright (C) 2023-2026 Source2ZE
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

#include "topdefender.h"
#include "commands.h"
#include "common.h"
#include "ctimer.h"
#include "detours.h"
#include "entity/ccsplayercontroller.h"
#include "idlemanager.h"
#include "playermanager.h"

std::vector<ZEPlayerHandle> sortedPlayers;
std::vector<std::string> failMessage = {
	"No top defenders? You all are bad.",
	"Start defending, less doorhugging.",
	"Hold left-click to shoot, FYI",
	"WHAT ARE YOU DOING?!?!?"};

std::weak_ptr<CTimer> m_pUpdateTimer;

CConVar<bool> g_cvarEnableTopDefender("cs2f_topdefender_enable", FCVAR_NONE, "Whether to use TopDefender", false);
CConVar<bool> g_cvarTopDefenderScoreboard("cs2f_topdefender_scoreboard", FCVAR_NONE, "Whether to display defender rank on scoreboard as MVP count", false);
CConVar<bool> g_cvarTopDefenderPrint("cs2f_topdefender_print", FCVAR_NONE, "Whether to print defender ranks to console at round end", true);
CConVar<float> g_cvarTopDefenderRate("cs2f_topdefender_rate", FCVAR_NONE, "How often TopDefender stats get updated", 1.0f, true, 0.1f, false, 0.0f);
CConVar<int> g_cvarTopDefenderThreshold("cs2f_topdefender_threshold", FCVAR_NONE, "Damage threshold for Top Defenders to be shown on round end", 1000, true, 0, false, 0);
CConVar<int> g_cvarTopDefenderScore("cs2f_topdefender_score", FCVAR_NONE, "Score given to the top defender", 5000, true, 0, false, 0);
CConVar<CUtlString> g_cvarTopDefenderClanTag("cs2f_topdefender_clantag", FCVAR_NONE, "Clan tag given to the top defender", "[Top Defender]");

// Array sorting function
bool SortTD(ZEPlayerHandle a, ZEPlayerHandle b)
{
	if (a.Get() && b.Get())
		return a.Get()->GetTotalDamage() > b.Get()->GetTotalDamage();
	else
		return false;
}

void UnfuckMVP(CCSPlayerController* pController)
{
	// stop this shit from spamming mvp music
	if (!pController->m_bMvpNoMusic())
		pController->m_bMvpNoMusic = true;

	if (pController->m_eMvpReason() != 0)
		pController->m_eMvpReason = 0;

	if (pController->m_iMusicKitID() != 0)
		pController->m_iMusicKitID = 0;

	if (pController->m_iMusicKitMVPs() != 0)
		pController->m_iMusicKitMVPs = 0;
}

void TD_OnPlayerHurt(IGameEvent* pEvent)
{
	CCSPlayerController* pAttacker = (CCSPlayerController*)pEvent->GetPlayerController("attacker");
	CCSPlayerController* pVictim = (CCSPlayerController*)pEvent->GetPlayerController("userid");

	// Ignore Ts/zombies and CTs hurting themselves
	if (!pAttacker || pAttacker->m_iTeamNum() != CS_TEAM_CT || pAttacker->m_iTeamNum() == pVictim->m_iTeamNum())
		return;

	ZEPlayer* pAttackerPlayer = pAttacker->GetZEPlayer();
	ZEPlayer* pVictimPlayer = pVictim->GetZEPlayer();

	if (!pAttackerPlayer || !pVictimPlayer)
		return;

	if (g_cvarIdleKickTime.Get() <= 0.0f || (std::time(0) - pVictimPlayer->GetLastInputTime()) < 15)
	{
		pAttackerPlayer->SetTotalDamage(pAttackerPlayer->GetTotalDamage() + pEvent->GetInt("dmg_health"));
		pAttackerPlayer->SetTotalHits(pAttackerPlayer->GetTotalHits() + 1);

		if (pEvent->GetInt("hitgroup") == 1)
			pAttackerPlayer->SetTotalHeadshots(pAttackerPlayer->GetTotalHeadshots() + 1);
	}
}

void TD_OnPlayerDeath(IGameEvent* pEvent)
{
	CCSPlayerController* pAttacker = (CCSPlayerController*)pEvent->GetPlayerController("attacker");
	CCSPlayerController* pVictim = (CCSPlayerController*)pEvent->GetPlayerController("userid");

	// Ignore Ts/zombie kills and ignore CT teamkilling or suicide
	if (!pAttacker || !pVictim || pAttacker->m_iTeamNum != CS_TEAM_CT || pAttacker->m_iTeamNum == pVictim->m_iTeamNum)
		return;

	ZEPlayer* pPlayer = pAttacker->GetZEPlayer();
	if (pPlayer)
		pPlayer->SetTotalKills(pPlayer->GetTotalKills() + 1);
}

void TD_OnRoundStart(IGameEvent* pEvent)
{
	if (!GetGlobals())
		return;

	// Reset player information
	for (int i = 0; i < GetGlobals()->maxClients; i++)
	{
		ZEPlayer* pPlayer = g_playerManager->GetPlayer(i);
		if (!pPlayer)
			continue;

		pPlayer->SetTotalDamage(0);
		pPlayer->SetTotalHits(0);
		pPlayer->SetTotalKills(0);
		pPlayer->SetTotalHeadshots(0);

		CCSPlayerController* pController = CCSPlayerController::FromSlot(pPlayer->GetPlayerSlot());
		if (pController)
		{
			if (g_cvarTopDefenderScoreboard.Get() && pController->m_iMVPs() != 0)
			{
				UnfuckMVP(pController);
				pController->m_iMVPs = 0;
			}

			// If player is top defender, we set their score and clan tag
			if (g_cvarTopDefenderScore.Get() > 0 && pPlayer->GetTopDefenderStatus())
			{
				pController->m_iScore() = pController->m_iScore() + g_cvarTopDefenderScore.Get();
				pController->SetClanTag(g_cvarTopDefenderClanTag.Get().String());
			}
		}
	}

	m_pUpdateTimer = CTimer::Create(g_cvarTopDefenderRate.Get(), TIMERFLAG_MAP | TIMERFLAG_ROUND, []() {
		if (!GetGlobals())
			return -1.0f;

		sortedPlayers.clear();

		for (int i = 0; i < GetGlobals()->maxClients; i++)
		{
			ZEPlayer* pPlayer = g_playerManager->GetPlayer(i);
			if (!pPlayer || pPlayer->GetTotalDamage() == 0)
				continue;

			sortedPlayers.push_back(pPlayer->GetHandle());
		}

		std::sort(sortedPlayers.begin(), sortedPlayers.end(), SortTD);

		if (g_cvarTopDefenderScoreboard.Get())
		{
			for (int i = 0; i < sortedPlayers.size(); i++)
			{
				ZEPlayer* pPlayer = sortedPlayers[i].Get();
				if (!pPlayer)
					continue;

				CCSPlayerController* pController = CCSPlayerController::FromSlot(pPlayer->GetPlayerSlot());
				if (!pController)
					continue;

				if (pController->m_iMVPs() != i + 1)
				{
					UnfuckMVP(pController);
					pController->m_iMVPs = i + 1;
				}
			}
		}

		return g_cvarTopDefenderRate.Get();
	});
}

void TD_OnRoundEnd(IGameEvent* pEvent)
{
	// When the round ends, stop the timer and do final recalculation
	if (!m_pUpdateTimer.expired())
		m_pUpdateTimer.lock()->Cancel();

	if (!GetGlobals())
		return;

	sortedPlayers.clear();

	for (int i = 0; i < GetGlobals()->maxClients; i++)
	{
		ZEPlayer* pPlayer = g_playerManager->GetPlayer(i);
		if (!pPlayer)
			continue;

		// Only add players over the threshold to the array
		if (pPlayer->GetTotalDamage() >= g_cvarTopDefenderThreshold.Get())
			sortedPlayers.push_back(pPlayer->GetHandle());

		// Reset the current top defender
		if (pPlayer->GetTopDefenderStatus())
		{
			CCSPlayerController* pController = CCSPlayerController::FromSlot(pPlayer->GetPlayerSlot());
			if (pController)
			{
				pPlayer->SetTopDefenderStatus(false);
				pController->m_iScore() = pController->m_iScore() - g_cvarTopDefenderScore.Get();
				pController->SetClanTag("");
			}
		}
	}

	ClientPrintAll(HUD_PRINTTALK, " \x09*** TOP DEFENDERS ***");

	// Check if players damaged more than threshold
	if (sortedPlayers.size() == 0)
	{
		ClientPrintAll(HUD_PRINTTALK, " \x02%s", failMessage[rand() % failMessage.size()].c_str());
		return;
	}

	std::sort(sortedPlayers.begin(), sortedPlayers.end(), SortTD);

	char colorMap[] = {'\x10', '\x08', '\x09', '\x0B'};

	for (int i = 0; i < sortedPlayers.size(); i++)
	{
		ZEPlayer* pPlayer = sortedPlayers[i].Get();
		if (!pPlayer)
			continue;

		CCSPlayerController* pController = CCSPlayerController::FromSlot(pPlayer->GetPlayerSlot());
		if (!pController)
			continue;

		if (i < 5)
			ClientPrintAll(HUD_PRINTTALK, " %c%i. %s \x01- \x07%i DMG \x05(%i HITS | %.0f%% HS | %i KILL%s)", colorMap[MIN(i, 3)], i + 1, pController->GetPlayerName(), pPlayer->GetTotalDamage(), pPlayer->GetTotalHits(), ((double)pPlayer->GetTotalHeadshots() / (double)pPlayer->GetTotalHits()) * 100.0f, pPlayer->GetTotalKills(), pPlayer->GetTotalKills() == 1 ? "" : "S");
		else
			ClientPrint(pController, HUD_PRINTTALK, " \x0C%i. %s \x01- \x07%i DMG \x05(%i HITS | %.0f%% HS | %i KILL%s)", i + 1, pController->GetPlayerName(), pPlayer->GetTotalDamage(), pPlayer->GetTotalHits(), ((double)pPlayer->GetTotalHeadshots() / (double)pPlayer->GetTotalHits()) * 100.0f, pPlayer->GetTotalKills(), pPlayer->GetTotalKills() == 1 ? "" : "S");

		if (i == 0)
			pPlayer->SetTopDefenderStatus(true);
	}

	// Because there are other round end stats to be displayed, delay printing it by a second to mitigate conflicts
	if (g_cvarTopDefenderPrint.Get())
	{
		CTimer::Create(1.0f, TIMERFLAG_MAP | TIMERFLAG_ROUND, []() {
			ClientPrintAll(HUD_PRINTCONSOLE, "--------------------------------- [Top Defender] ---------------------------------");
			for (int i = 0; i < sortedPlayers.size(); i++)
			{
				ZEPlayer* pPlayer = sortedPlayers[i].Get();
				if (!pPlayer)
					continue;

				CCSPlayerController* pController = CCSPlayerController::FromSlot(pPlayer->GetPlayerSlot());
				if (!pController)
					continue;

				ClientPrintAll(HUD_PRINTCONSOLE, "%i. %s - %i DMG (%i HITS | %.0f%% HS | %i KILL%s)", i + 1, pController->GetPlayerName(), pPlayer->GetTotalDamage(), pPlayer->GetTotalHits(), ((double)pPlayer->GetTotalHeadshots() / (double)pPlayer->GetTotalHits()) * 100.0f, pPlayer->GetTotalKills(), pPlayer->GetTotalKills() == 1 ? "" : "S");
			}
			ClientPrintAll(HUD_PRINTCONSOLE, "----------------------------------------------------------------------------------");
			return -1.0f;
		});
	}
}

CON_COMMAND_CHAT(tdrank, "[player/rank] - Displays your defender stats or a specified player's stats.")
{
	TopDefenderSearch(player, args);
}

CON_COMMAND_CHAT(tdfind, "[player/rank] - Displays your defender stats or a specified player's stats.")
{
	TopDefenderSearch(player, args);
}

void TopDefenderSearch(CCSPlayerController* player, const CCommand& args)
{
	if (!player)
	{
		ClientPrint(player, HUD_PRINTCONSOLE, TD_PREFIX "You cannot use this command from the server console.");
		return;
	}

	if (sortedPlayers.size() == 0)
	{
		ClientPrint(player, HUD_PRINTTALK, TD_PREFIX "There are no top defenders at this time.");
		return;
	}

	// First check if no argument is passed
	if (args.ArgC() < 2)
	{
		ZEPlayer* pPlayer = player->GetZEPlayer();
		if (!pPlayer)
			return;

		// Search for the player in the sorted array
		for (int i = 0; i < sortedPlayers.size(); i++)
		{
			if (sortedPlayers[i] != pPlayer)
				continue;

			ClientPrint(player, HUD_PRINTTALK, TD_PREFIX "RANK \4%d \1- \4%d \1DMG (\4%d \1HITS | \4%.0f%% \1HS | \4%d \1KILL%s)", i + 1, pPlayer->GetTotalDamage(), pPlayer->GetTotalHits(), ((double)pPlayer->GetTotalHeadshots() / (double)pPlayer->GetTotalHits()) * 100.0f, pPlayer->GetTotalKills(), pPlayer->GetTotalKills() == 1 ? "" : "S");
			return;
		}

		ClientPrint(player, HUD_PRINTTALK, TD_PREFIX "You do not have any stats to display at this time.");
	}
	else
	{
		// Check if the argument passed is a valid rank
		// If invalid, we then assume it's a player's name and search accordingly
		int iRank = Q_atoi(args[1]);
		if (iRank <= 0 || iRank > sortedPlayers.size())
		{
			int iNumClients = 0;
			int pSlots[MAXPLAYERS];

			if (!g_playerManager->CanTargetPlayers(player, args[1], iNumClients, pSlots, NO_MULTIPLE))
				return;

			for (int i = 0; i < sortedPlayers.size(); i++)
			{
				CCSPlayerController* pController = CCSPlayerController::FromSlot(pSlots[0]);
				if (!pController)
					continue;

				ZEPlayer* pTarget = pController->GetZEPlayer();
				if (!pTarget || sortedPlayers[i] != pTarget)
					continue;

				ClientPrint(player, HUD_PRINTTALK, TD_PREFIX "RANK \4%d\1: \4%s \1 - \4%d \1DMG (\4%d \1HITS | \4%.0f%% \1HS | \4%d \1KILL%s)", i + 1, pController->GetPlayerName(), pTarget->GetTotalDamage(), pTarget->GetTotalHits(), ((double)pTarget->GetTotalHeadshots() / (double)pTarget->GetTotalHits()) * 100.0f, pTarget->GetTotalKills(), pTarget->GetTotalKills() == 1 ? "" : "S");
				return;
			}

			ClientPrint(player, HUD_PRINTTALK, TD_PREFIX "%s has no stats to display at this time.", CCSPlayerController::FromSlot(pSlots[0])->GetPlayerName());
		}
		else
		{
			ZEPlayer* pPlayer = sortedPlayers[iRank - 1].Get();
			if (!pPlayer)
				return;

			CCSPlayerController* pController = CCSPlayerController::FromSlot(pPlayer->GetPlayerSlot());
			if (!pController)
				return;

			ClientPrint(player, HUD_PRINTTALK, TD_PREFIX "RANK \4%d\1: \4%s \1- \4%d \1DMG (\4%d \1HITS | \4%.0f%% \1HS | \4%d \1KILL%s)", iRank, pController->GetPlayerName(), pPlayer->GetTotalDamage(), pPlayer->GetTotalHits(), ((double)pPlayer->GetTotalHeadshots() / (double)pPlayer->GetTotalHits()) * 100.0f, pPlayer->GetTotalKills(), pPlayer->GetTotalKills() == 1 ? "" : "S");
		}
	}
}