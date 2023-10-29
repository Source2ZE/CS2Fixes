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

#include "votemanager.h"
#include "commands.h"
#include "playermanager.h"
#include "ctimer.h"
#include "entity/cgamerules.h"

#include "tier0/memdbgon.h"

extern CEntitySystem *g_pEntitySystem;
extern IVEngineServer2 *g_pEngineServer2;
extern CGlobalVars *gpGlobals;
extern CCSGameRules *g_pGameRules;

ERTVState g_RTVState = ERTVState::MAP_START;
EExtendState g_ExtendState = EExtendState::MAP_START;
int g_ExtendsLeft = 1;

// CONVAR_TODO
float g_RTVSucceedRatio = 0.6f;
float g_ExtendSucceedRatio = 0.5f;
int g_ExtendTimeToAdd = 20;

int GetCurrentRTVCount()
{
	int iVoteCount = 0;

	for (int i = 0; i < gpGlobals->maxClients; i++)
	{
		ZEPlayer* pPlayer = g_playerManager->GetPlayer(i);

		if (pPlayer && pPlayer->GetRTVVote())
			iVoteCount++;
	}

	return iVoteCount;
}

int GetNeededRTVCount()
{
	int iOnlinePlayers = 0.0f;
	int iVoteCount = 0;

	for (int i = 0; i < gpGlobals->maxClients; i++)
	{
		ZEPlayer* pPlayer = g_playerManager->GetPlayer(i);

		if (pPlayer)
		{
			iOnlinePlayers++;
			if (pPlayer->GetRTVVote())
				iVoteCount++;
		}
	}

	return (int)(iOnlinePlayers * g_RTVSucceedRatio) + 1;
}

int GetCurrentExtendCount()
{
	int iVoteCount = 0;

	for (int i = 0; i < gpGlobals->maxClients; i++)
	{
		ZEPlayer* pPlayer = g_playerManager->GetPlayer(i);

		if (pPlayer && pPlayer->GetExtendVote())
			iVoteCount++;
	}

	return iVoteCount;
}

int GetNeededExtendCount()
{
	int iOnlinePlayers = 0.0f;
	int iVoteCount = 0;

	for (int i = 0; i < gpGlobals->maxClients; i++)
	{
		ZEPlayer* pPlayer = g_playerManager->GetPlayer(i);

		if (pPlayer)
		{
			iOnlinePlayers++;
			if (pPlayer->GetExtendVote())
				iVoteCount++;
		}
	}

	return (int)(iOnlinePlayers * g_ExtendSucceedRatio) + 1;
}

CON_COMMAND_CHAT(rtv, "Vote to end the current map sooner.")
{
	if (!player)
	{
		ClientPrint(player, HUD_PRINTCONSOLE, CHAT_PREFIX "You cannot use this command from the server console.");
		return;
	}

	int iPlayer = player->GetPlayerSlot();

	ZEPlayer* pPlayer = g_playerManager->GetPlayer(iPlayer);

	// Something has to really go wrong for this to happen
	if (!pPlayer)
	{
		Warning("%s Tried to access a null ZEPlayer!!\n", player->GetPlayerName());
		return;
	}

	switch (g_RTVState)
	{
	case ERTVState::MAP_START:
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "RTV is not open yet.");
		return;
	case ERTVState::POST_RTV_SUCCESSFULL:
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "RTV vote already succeeded.");
		return;
	case ERTVState::POST_LAST_ROUND_END:
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "RTV is closed during next map selection.");
		return;
	case ERTVState::BLOCKED_BY_ADMIN:
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "RTV has been blocked by an Admin.");
		return;
	}

	int iCurrentRTVCount = GetCurrentRTVCount();
	int iNeededRTVCount = GetNeededRTVCount();

	if (pPlayer->GetRTVVote())
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "You have already rocked the vote (%i voted, %i needed).", iCurrentRTVCount, iNeededRTVCount);
		return;
	}

	if (iCurrentRTVCount + 1 >= iNeededRTVCount)
	{
		g_RTVState = ERTVState::POST_RTV_SUCCESSFULL;
		ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX "RTV succeeded! This is the last round of the map!");
		// CONVAR_TODO
		g_pEngineServer2->ServerCommand("mp_timelimit 1");

		for (int i = 0; i < gpGlobals->maxClients; i++)
		{
			ZEPlayer* pPlayer2 = g_playerManager->GetPlayer(i);
			if (pPlayer2)
				pPlayer2->SetRTVVote(false);
		}

		return;
	}

	pPlayer->SetRTVVote(true);
	ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX "%s wants to rock the vote (%i voted, %i needed).", player->GetPlayerName(), iCurrentRTVCount+1, iNeededRTVCount);
}

CON_COMMAND_CHAT(unrtv, "Remove your vote to end the current map sooner.")
{
	if (!player)
	{
		ClientPrint(player, HUD_PRINTCONSOLE, CHAT_PREFIX "You cannot use this command from the server console.");
		return;
	}

	int iPlayer = player->GetPlayerSlot();

	ZEPlayer* pPlayer = g_playerManager->GetPlayer(iPlayer);

	// Something has to really go wrong for this to happen
	if (!pPlayer)
	{
		Warning("%s Tried to access a null ZEPlayer!!\n", player->GetPlayerName());
		return;
	}

	if (!pPlayer->GetRTVVote())
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "You have not voted to RTV current map.");
		return;
	}

	pPlayer->SetRTVVote(false);
	ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "You no longer want to RTV current map.");
}


CON_COMMAND_CHAT(ve, "Vote to extend the current map.")
{
	if (!player)
	{
		ClientPrint(player, HUD_PRINTCONSOLE, CHAT_PREFIX "You cannot use this command from the server console.");
		return;
	}

	int iPlayer = player->GetPlayerSlot();

	ZEPlayer* pPlayer = g_playerManager->GetPlayer(iPlayer);

	// Something has to really go wrong for this to happen
	if (!pPlayer)
	{
		Warning("%s Tried to access a null ZEPlayer!!\n", player->GetPlayerName());
		return;
	}

	switch (g_ExtendState)
	{
	case EExtendState::MAP_START:
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Extend vote is not open yet.");
		return;
	case EExtendState::POST_EXTEND_COOLDOWN:
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Extend vote is not open yet.");
		return;
	case EExtendState::POST_EXTEND_NO_EXTENDS_LEFT:
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "There are no extends left for the current map.");
		return;
	case EExtendState::POST_LAST_ROUND_END:
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Extend vote is closed during next map selection.");
		return;
	case EExtendState::NO_EXTENDS:
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Extend vote is not allowed for current map.");
		return;
	}

	int iCurrentExtendCount = GetCurrentExtendCount();
	int iNeededExtendCount = GetNeededExtendCount();

	if (pPlayer->GetExtendVote())
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "You have already voted to extend the map (%i voted, %i needed).", iCurrentExtendCount, iNeededExtendCount);
		return;
	}

	if (iCurrentExtendCount + 1 >= iNeededExtendCount)
	{
		// mimic behaviour of !extend
		// CONVAR_TODO
		ConVar* cvar = g_pCVar->GetConVar(g_pCVar->FindConVar("mp_timelimit"));

		float flTimelimit;
		// type punning
		memcpy(&flTimelimit, &cvar->values, sizeof(flTimelimit));

		if (gpGlobals->curtime - g_pGameRules->m_flGameStartTime > flTimelimit * 60)
			flTimelimit = (gpGlobals->curtime - g_pGameRules->m_flGameStartTime) / 60.0f + g_ExtendTimeToAdd;
		else
		{
			if (flTimelimit == 1)
				flTimelimit = 0;
			flTimelimit += g_ExtendTimeToAdd;
		}

		if (flTimelimit <= 0)
			flTimelimit = 1;
		
		char buf[32];
		V_snprintf(buf, sizeof(buf), "mp_timelimit %.6f", flTimelimit + g_ExtendTimeToAdd);

		// CONVAR_TODO
		g_pEngineServer2->ServerCommand(buf);

		if (g_ExtendsLeft == 1)
			// there are no extends left after a successfull extend vote
			g_ExtendState = EExtendState::POST_EXTEND_NO_EXTENDS_LEFT;
		else
		{
			// there's an extend left after a successfull extend vote
			g_ExtendState = EExtendState::POST_EXTEND_COOLDOWN;

			// Allow another extend vote after 2 minutes
			new CTimer(120.0f, false, []()
			{
				if (g_ExtendState == EExtendState::POST_EXTEND_COOLDOWN)
					g_ExtendState = EExtendState::EXTEND_ALLOWED;
				return -1.0f;
			});
		}

		for (int i = 0; i < gpGlobals->maxClients; i++)
		{
			ZEPlayer* pPlayer2 = g_playerManager->GetPlayer(i);
			if (pPlayer2)
				pPlayer2->SetExtendVote(false);
		}

		g_ExtendsLeft--;
		ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX "Extend vote succeeded! Current map has been extended by %i minutes.", g_ExtendTimeToAdd);

		return;
	}

	pPlayer->SetExtendVote(true);
	ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX "%s wants to extend the map (%i voted, %i needed).", player->GetPlayerName(), iCurrentExtendCount+1, iNeededExtendCount);
}

CON_COMMAND_CHAT(unve, "Remove your vote to extend current map.")
{
	if (!player)
	{
		ClientPrint(player, HUD_PRINTCONSOLE, CHAT_PREFIX "You cannot use this command from the server console.");
		return;
	}

	int iPlayer = player->GetPlayerSlot();

	ZEPlayer* pPlayer = g_playerManager->GetPlayer(iPlayer);

	// Something has to really go wrong for this to happen
	if (!pPlayer)
	{
		Warning("%s Tried to access a null ZEPlayer!!\n", player->GetPlayerName());
		return;
	}

	if (!pPlayer->GetExtendVote())
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "You have not voted to extend current map.");
		return;
	}

	pPlayer->SetExtendVote(false);
	ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "You no longer want to extend current map.");
}

CON_COMMAND_CHAT_FLAGS(blockrtv, "Block the ability for players to vote to end current map sooner.", ADMFLAG_CHANGEMAP)
{
	if (g_RTVState == ERTVState::BLOCKED_BY_ADMIN)
	{
		if (player)
			ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "RTV is already blocked.");
		else
			ConMsg("RTV is already blocked.");
		return;
	}

	const char* pszCommandPlayerName = player ? player->GetPlayerName() : "Console";

	g_RTVState = ERTVState::BLOCKED_BY_ADMIN;

	ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX ADMIN_PREFIX "blocked vote for RTV.", pszCommandPlayerName);
}

CON_COMMAND_CHAT_FLAGS(unblockrtv, "Restore the ability for players to vote to end current map sooner.", ADMFLAG_CHANGEMAP)
{
	if (g_RTVState == ERTVState::RTV_ALLOWED)
	{
		if (player)
			ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "RTV is not blocked.");
		else
			ConMsg("RTV is not blocked.");
		return;
	}

	const char* pszCommandPlayerName = player ? player->GetPlayerName() : "Console";

	g_RTVState = ERTVState::RTV_ALLOWED;

	ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX ADMIN_PREFIX "restored vote for RTV.", pszCommandPlayerName);
}

CON_COMMAND_CHAT_FLAGS(addextend, "Add another extend to the current map for players to vote.", ADMFLAG_CHANGEMAP)
{
	const char* pszCommandPlayerName = player ? player->GetPlayerName() : "Console";

	if (g_ExtendState == EExtendState::POST_EXTEND_NO_EXTENDS_LEFT || g_ExtendState == EExtendState::NO_EXTENDS)
		g_ExtendState = EExtendState::EXTEND_ALLOWED;
	
	g_ExtendsLeft += 1;

	ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX ADMIN_PREFIX "allowed for an additional extend.", pszCommandPlayerName);
}

CON_COMMAND_CHAT(extendsleft, "Display amount of extends left for the current map")
{
	char message[64];

	switch (g_ExtendsLeft)
	{
	case 0:
		strcpy(message, "There are no extends left.");
		break;
	case 1:
		strcpy(message, "There's 1 extend left");
		break;
	default:
		V_snprintf(message, sizeof(message), "There are %i extends left.", g_ExtendsLeft);
		break;
	}

	if (player)
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "%s", message);
	else
		ConMsg("%s", message);
}

CON_COMMAND_CHAT(timeleft, "Display time left to end of current map.")
{
	if (!player)
	{
		ClientPrint(player, HUD_PRINTCONSOLE, CHAT_PREFIX "You cannot use this command from the server console.");
		return;
	}

	// CONVAR_TODO
	ConVar* cvar = g_pCVar->GetConVar(g_pCVar->FindConVar("mp_timelimit"));

	float flTimelimit;
	memcpy(&flTimelimit, &cvar->values, sizeof(flTimelimit));

	if (flTimelimit == 0)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "No time limit");
		return;
	}

	int iTimeleft = (int) ((g_pGameRules->m_flGameStartTime + flTimelimit * 60.0f) - gpGlobals->curtime);

	if (iTimeleft < 0)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Last round!");
		return;
	}

	div_t div = std::div(iTimeleft, 60);
	int iMinutesLeft = div.quot;
	int iSecondsLeft = div.rem;
	
	if (iMinutesLeft > 0)
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Timeleft: %i minutes %i seconds", iMinutesLeft, iSecondsLeft);
	else
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Timeleft: %i seconds", iSecondsLeft);
}
