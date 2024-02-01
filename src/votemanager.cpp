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
#include "icvar.h"
#include "entity/cgamerules.h"

#include "tier0/memdbgon.h"

extern CGameEntitySystem *g_pEntitySystem;
extern IVEngineServer2 *g_pEngineServer2;
extern CGlobalVars *gpGlobals;
extern CCSGameRules *g_pGameRules;

ERTVState g_RTVState = ERTVState::MAP_START;
EExtendState g_ExtendState = EExtendState::MAP_START;

// CONVAR_TODO
bool g_bVoteManagerEnable = false;
int g_iExtendsLeft = 1;
float g_flExtendSucceedRatio = 0.5f;
int g_iExtendTimeToAdd = 20;
float g_flRTVSucceedRatio = 0.6f;
bool g_bRTVEndRound = false;

CON_COMMAND_F(cs2f_votemanager_enable, "Whether to enable votemanager features such as RTV and extends", FCVAR_SPONLY | FCVAR_LINKED_CONCOMMAND)
{
	if (args.ArgC() < 2)
		Msg("%s %i\n", args[0], g_bVoteManagerEnable);
	else
		g_bVoteManagerEnable = V_StringToBool(args[1], false);
}
CON_COMMAND_F(cs2f_extends, "Maximum extends per map", FCVAR_SPONLY | FCVAR_LINKED_CONCOMMAND)
{
	if (args.ArgC() < 2)
		Msg("%s %i\n", args[0], g_iExtendsLeft);
	else
		g_iExtendsLeft = V_StringToInt32(args[1], 1);
}
CON_COMMAND_F(cs2f_extend_success_ratio, "Ratio needed to pass an extend", FCVAR_SPONLY | FCVAR_LINKED_CONCOMMAND)
{
	if (args.ArgC() < 2)
		Msg("%s %.2f\n", args[0], g_flExtendSucceedRatio);
	else
		g_flExtendSucceedRatio = V_StringToFloat32(args[1], 0.5f);
}
CON_COMMAND_F(cs2f_extend_time, "Time to add per extend", FCVAR_SPONLY | FCVAR_LINKED_CONCOMMAND)
{
	if (args.ArgC() < 2)
		Msg("%s %i\n", args[0], g_iExtendTimeToAdd);
	else
		g_iExtendTimeToAdd = V_StringToInt32(args[1], 20);
}
CON_COMMAND_F(cs2f_rtv_success_ratio, "Ratio needed to pass RTV", FCVAR_SPONLY | FCVAR_LINKED_CONCOMMAND)
{
	if (args.ArgC() < 2)
		Msg("%s %.2f\n", args[0], g_flRTVSucceedRatio);
	else
		g_flRTVSucceedRatio = V_StringToFloat32(args[1], 0.6f);
}
CON_COMMAND_F(cs2f_rtv_endround, "Whether to immediately end the round when RTV succeeds", FCVAR_LINKED_CONCOMMAND | FCVAR_SPONLY)
{
	if (args.ArgC() < 2)
		Msg("%s %i\n", args[0], g_bRTVEndRound);
	else
		g_bRTVEndRound = V_StringToBool(args[1], false);
}

int GetCurrentRTVCount()
{
	int iVoteCount = 0;

	for (int i = 0; i < gpGlobals->maxClients; i++)
	{
		ZEPlayer* pPlayer = g_playerManager->GetPlayer(i);

		if (pPlayer && pPlayer->GetRTVVote() && !pPlayer->IsFakeClient())
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

		if (pPlayer && !pPlayer->IsFakeClient())
		{
			iOnlinePlayers++;
			if (pPlayer->GetRTVVote())
				iVoteCount++;
		}
	}

	return (int)(iOnlinePlayers * g_flRTVSucceedRatio) + 1;
}

int GetCurrentExtendCount()
{
	int iVoteCount = 0;

	for (int i = 0; i < gpGlobals->maxClients; i++)
	{
		ZEPlayer* pPlayer = g_playerManager->GetPlayer(i);

		if (pPlayer && pPlayer->GetExtendVote() && !pPlayer->IsFakeClient())
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

		if (pPlayer && !pPlayer->IsFakeClient())
		{
			iOnlinePlayers++;
			if (pPlayer->GetExtendVote())
				iVoteCount++;
		}
	}

	return (int)(iOnlinePlayers * g_flExtendSucceedRatio) + 1;
}

CON_COMMAND_CHAT(rtv, "- Vote to end the current map sooner")
{
	if (!g_bVoteManagerEnable)
		return;

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

	if (pPlayer->GetRTVVoteTime() + 60.0f > gpGlobals->curtime)
	{
		int iRemainingTime = (int)(pPlayer->GetRTVVoteTime() + 60.0f - gpGlobals->curtime);
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Wait %i seconds before you can RTV again.", iRemainingTime);
		return;
	}

	if (iCurrentRTVCount + 1 >= iNeededRTVCount)
	{
		g_RTVState = ERTVState::POST_RTV_SUCCESSFULL;
		// CONVAR_TODO
		g_pEngineServer2->ServerCommand("mp_timelimit 1");

		if (g_bRTVEndRound)
		{
			ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX "RTV succeeded! Ending the map now...");

			new CTimer(3.0f, false, []()
			{
				g_pGameRules->TerminateRound(5.0f, CSRoundEndReason::Draw);

				return -1.0f;
			});
		}
		else
		{
			ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX "RTV succeeded! This is the last round of the map!");
		}

		for (int i = 0; i < gpGlobals->maxClients; i++)
		{
			ZEPlayer* pPlayer2 = g_playerManager->GetPlayer(i);
			if (pPlayer2)
				pPlayer2->SetRTVVote(false);
		}

		return;
	}

	pPlayer->SetRTVVote(true);
	pPlayer->SetRTVVoteTime(gpGlobals->curtime);
	ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX "%s wants to rock the vote (%i voted, %i needed).", player->GetPlayerName(), iCurrentRTVCount+1, iNeededRTVCount);
}

CON_COMMAND_CHAT(unrtv, "- Remove your vote to end the current map sooner")
{
	if (!g_bVoteManagerEnable)
		return;

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

CON_COMMAND_CHAT(ve, "- Vote to extend current map")
{
	if (!g_bVoteManagerEnable)
		return;

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

	if (pPlayer->GetExtendVoteTime() + 60.0f > gpGlobals->curtime)
	{
		int iRemainingTime = (int)(pPlayer->GetExtendVoteTime() + 60.0f - gpGlobals->curtime);
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Wait %i seconds before you can vote extend again.", iRemainingTime);
		return;
	}

	if (iCurrentExtendCount + 1 >= iNeededExtendCount)
	{
		// mimic behaviour of !extend
		// CONVAR_TODO
		ConVar* cvar = g_pCVar->GetConVar(g_pCVar->FindConVar("mp_timelimit"));

		// CONVAR_TODO
		// HACK: values is actually the cvar value itself, hence this ugly cast.
		float flTimelimit = *(float *)&cvar->values;

		if (gpGlobals->curtime - g_pGameRules->m_flGameStartTime > flTimelimit * 60)
			flTimelimit = (gpGlobals->curtime - g_pGameRules->m_flGameStartTime) / 60.0f + g_iExtendTimeToAdd;
		else
		{
			if (flTimelimit == 1)
				flTimelimit = 0;
			flTimelimit += g_iExtendTimeToAdd;
		}

		if (flTimelimit <= 0)
			flTimelimit = 1;
		
		char buf[32];
		V_snprintf(buf, sizeof(buf), "mp_timelimit %.6f", flTimelimit);

		// CONVAR_TODO
		g_pEngineServer2->ServerCommand(buf);

		if (g_iExtendsLeft == 1)
			// there are no extends left after a successfull extend vote
			g_ExtendState = EExtendState::POST_EXTEND_NO_EXTENDS_LEFT;
		else
		{
			// there's an extend left after a successfull extend vote
			g_ExtendState = EExtendState::POST_EXTEND_COOLDOWN;

			// Allow another extend vote after added time lapses
			new CTimer(g_iExtendTimeToAdd * 60.0f, false, []()
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

		g_iExtendsLeft--;
		ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX "Extend vote succeeded! Current map has been extended by %i minutes.", g_iExtendTimeToAdd);

		return;
	}

	pPlayer->SetExtendVote(true);
	pPlayer->SetExtendVoteTime(gpGlobals->curtime);
	ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX "%s wants to extend the map (%i voted, %i needed).", player->GetPlayerName(), iCurrentExtendCount+1, iNeededExtendCount);
}

CON_COMMAND_CHAT(unve, "- Remove your vote to extend current map")
{
	if (!g_bVoteManagerEnable)
		return;

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

CON_COMMAND_CHAT_FLAGS(disablertv, "- Disable the ability for players to vote to end current map sooner", ADMFLAG_CHANGEMAP)
{
	if (!g_bVoteManagerEnable)
		return;

	if (g_RTVState == ERTVState::BLOCKED_BY_ADMIN)
	{
		if (player)
			ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "RTV is already disabled.");
		else
			ConMsg("RTV is already disabled.");
		return;
	}

	const char* pszCommandPlayerName = player ? player->GetPlayerName() : "Console";

	g_RTVState = ERTVState::BLOCKED_BY_ADMIN;

	ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX ADMIN_PREFIX "disabled vote for RTV.", pszCommandPlayerName);
}

CON_COMMAND_CHAT_FLAGS(enablertv, "- Restore the ability for players to vote to end current map sooner", ADMFLAG_CHANGEMAP)
{
	if (!g_bVoteManagerEnable)
		return;

	if (g_RTVState == ERTVState::RTV_ALLOWED)
	{
		if (player)
			ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "RTV is not disabled.");
		else
			ConMsg("RTV is not disabled.");
		return;
	}

	const char* pszCommandPlayerName = player ? player->GetPlayerName() : "Console";

	g_RTVState = ERTVState::RTV_ALLOWED;

	ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX ADMIN_PREFIX "enabled vote for RTV.", pszCommandPlayerName);
}

CON_COMMAND_CHAT_FLAGS(addextend, "- Add another extend to the current map for players to vote", ADMFLAG_CHANGEMAP)
{
	if (!g_bVoteManagerEnable)
		return;

	const char* pszCommandPlayerName = player ? player->GetPlayerName() : "Console";

	if (g_ExtendState == EExtendState::POST_EXTEND_NO_EXTENDS_LEFT || g_ExtendState == EExtendState::NO_EXTENDS)
		g_ExtendState = EExtendState::EXTEND_ALLOWED;
	
	g_iExtendsLeft += 1;

	ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX ADMIN_PREFIX "allowed for an additional extend.", pszCommandPlayerName);
}

CON_COMMAND_CHAT(extendsleft, "- Display amount of extends left for the current map")
{
	if (!g_bVoteManagerEnable)
		return;

	char message[64];

	switch (g_iExtendsLeft)
	{
	case 0:
		strcpy(message, "There are no extends left.");
		break;
	case 1:
		strcpy(message, "There's 1 extend left");
		break;
	default:
		V_snprintf(message, sizeof(message), "There are %i extends left.", g_iExtendsLeft);
		break;
	}

	if (player)
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "%s", message);
	else
		ConMsg("%s", message);
}

CON_COMMAND_CHAT(timeleft, "- Display time left to end of current map.")
{
	if (!player)
	{
		ClientPrint(player, HUD_PRINTCONSOLE, CHAT_PREFIX "You cannot use this command from the server console.");
		return;
	}

	ConVar* cvar = g_pCVar->GetConVar(g_pCVar->FindConVar("mp_timelimit"));

	// CONVAR_TODO
	// HACK: values is actually the cvar value itself, hence this ugly cast.
	float flTimelimit = *(float *)&cvar->values;

	if (flTimelimit == 0.0f)
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
