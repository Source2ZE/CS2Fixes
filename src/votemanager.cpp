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

#include "votemanager.h"
#include "commands.h"
#include "ctimer.h"
#include "entity/cgamerules.h"
#include "icvar.h"
#include "playermanager.h"

#include "tier0/memdbgon.h"

extern CGameEntitySystem* g_pEntitySystem;
extern IVEngineServer2* g_pEngineServer2;
extern CGlobalVars* GetGlobals();
extern CCSGameRules* g_pGameRules;

CVoteManager* g_pVoteManager = nullptr;

bool g_bVoteManagerEnable = false;
int g_iMaxExtends = 1;
float g_flExtendSucceedRatio = 0.5f;
int g_iExtendTimeToAdd = 20;
float g_flRTVSucceedRatio = 0.6f;
bool g_bRTVEndRound = false;
int g_ExtendVoteMode = (int)EExtendVoteMode::EXTENDVOTE_ADMINONLY;
float g_flExtendVoteStartTime = 4.0f;
float g_flExtendVoteDuration = 30.0f;
float g_flExtendBeginRatio = 0.4f;
float g_flExtendVoteDelay = 300.0f;
float g_flRtvDelay = 300.0f;

FAKE_BOOL_CVAR(cs2f_votemanager_enable, "Whether to enable votemanager features such as RTV and extends", g_bVoteManagerEnable, false, false)
FAKE_FLOAT_CVAR(cs2f_extend_vote_delay, "If cs2f_extend_mode is 2, Time after map start until extend votes can be triggered", g_flExtendVoteDelay, 120.0f, false)
FAKE_INT_CVAR(cs2f_extend_mode, "How extend votes are handled. (0=off, 1=only admins can start, 2=players can start with !ve, 3=auto start at given timeleft)", g_ExtendVoteMode, (int)EExtendVoteMode::EXTENDVOTE_ADMINONLY, false)
FAKE_INT_CVAR(cs2f_extends, "Maximum extends per map", g_iMaxExtends, 1, false)
FAKE_FLOAT_CVAR(cs2f_extend_success_ratio, "Ratio needed to pass an extend vote", g_flExtendSucceedRatio, 0.5f, false)
FAKE_INT_CVAR(cs2f_extend_time, "Time to add per extend in minutes", g_iExtendTimeToAdd, 20, false)
FAKE_FLOAT_CVAR(cs2f_extend_vote_start_time, "If cs2f_extend_mode is 3, start an extend vote at this timeleft (minutes)", g_flExtendVoteStartTime, 4.0f, false)
FAKE_FLOAT_CVAR(cs2f_extend_vote_duration, "Time to leave the extend vote active for (seconds)", g_flExtendVoteDuration, 30.0f, false)
FAKE_FLOAT_CVAR(cs2f_extend_begin_ratio, "If cs2f_extend_mode is >= 2, Ratio needed to begin an extend vote", g_flExtendBeginRatio, 0.4f, false)

FAKE_FLOAT_CVAR(cs2f_rtv_vote_delay, "Time after map start until RTV votes can be cast", g_flRtvDelay, 120.0f, false)
FAKE_FLOAT_CVAR(cs2f_rtv_success_ratio, "Ratio needed to pass RTV", g_flRTVSucceedRatio, 0.6f, false)
FAKE_BOOL_CVAR(cs2f_rtv_endround, "Whether to immediately end the round when RTV succeeds", g_bRTVEndRound, false, false)

void CVoteManager::VoteManager_Init()
{
	// Disable RTV and Extend votes after map has just started
	m_RTVState = ERTVState::MAP_START;
	m_ExtendState = EExtendState::MAP_START;

	m_iExtends = 0;

	new CTimer(g_flExtendVoteDelay, false, true, [this]() {
		if (m_ExtendState < EExtendState::POST_EXTEND_NO_EXTENDS_LEFT)
			m_ExtendState = EExtendState::EXTEND_ALLOWED;
		return -1.0f;
	});

	new CTimer(g_flRtvDelay, false, true, [this]() {
		if (m_RTVState != ERTVState::BLOCKED_BY_ADMIN)
			m_RTVState = ERTVState::RTV_ALLOWED;
		return -1.0f;
	});

	new CTimer(m_flExtendVoteTickrate, false, true, std::bind(&CVoteManager::TimerCheckTimeleft, this));
}

float CVoteManager::TimerCheckTimeleft()
{
	if (!GetGlobals() || !g_pGameRules)
		return m_flExtendVoteTickrate;

	if (!g_bVoteManagerEnable)
		return m_flExtendVoteTickrate;

	// Auto votes disabled, dont stop the timer in case this changes mid-map
	if (g_ExtendVoteMode != EExtendVoteMode::EXTENDVOTE_AUTO)
		return m_flExtendVoteTickrate;

	// Vote already happening
	if (m_bVoteStarting || m_ExtendState == EExtendState::IN_PROGRESS)
		return m_flExtendVoteTickrate;

	// No more extends or map RTVd
	if ((g_iMaxExtends - m_iExtends) <= 0 || m_ExtendState >= EExtendState::POST_EXTEND_NO_EXTENDS_LEFT)
		return m_flExtendVoteTickrate;

	ConVar* cvar = g_pCVar->GetConVar(g_pCVar->FindConVar("mp_timelimit"));
	// CONVAR_TODO
	// HACK: values is actually the cvar value itself, hence this ugly cast.
	float flTimelimit = *(float*)&cvar->values;

	if (flTimelimit <= 0.0)
		return m_flExtendVoteTickrate;

	float flTimeleft = (g_pGameRules->m_flGameStartTime + flTimelimit * 60.0f) - GetGlobals()->curtime;

	// Not yet time to start a vote
	if (flTimeleft > (g_flExtendVoteStartTime * 60.0))
		return m_flExtendVoteTickrate;

	m_bVoteStarting = true;
	ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX "Extend vote starting in 10 seconds!");

	new CTimer(7.0f, false, true, [this]() {
		if (m_iVoteStartTicks == 0)
		{
			m_iVoteStartTicks = 3;
			g_pVoteManager->StartExtendVote(VOTE_CALLER_SERVER);
			m_bVoteStarting = false;
			return -1.0f;
		}

		ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX "Extend vote starting in %d....", m_iVoteStartTicks);
		m_iVoteStartTicks--;
		return 1.0f;
	});

	return m_flExtendVoteTickrate;
}

int CVoteManager::GetCurrentRTVCount()
{
	int iVoteCount = 0;

	if (!GetGlobals())
		return iVoteCount;

	for (int i = 0; i < GetGlobals()->maxClients; i++)
	{
		ZEPlayer* pPlayer = g_playerManager->GetPlayer(i);

		if (pPlayer && pPlayer->GetRTVVote() && !pPlayer->IsFakeClient())
			iVoteCount++;
	}

	return iVoteCount;
}

int CVoteManager::GetNeededRTVCount()
{
	return (int)(g_playerManager->GetOnlinePlayerCount(false) * g_flRTVSucceedRatio) + 1;
}

int CVoteManager::GetCurrentExtendCount()
{
	int iVoteCount = 0;

	if (!GetGlobals())
		return iVoteCount;

	for (int i = 0; i < GetGlobals()->maxClients; i++)
	{
		ZEPlayer* pPlayer = g_playerManager->GetPlayer(i);

		if (pPlayer && pPlayer->GetExtendVote() && !pPlayer->IsFakeClient())
			iVoteCount++;
	}

	return iVoteCount;
}

// TODO: wtf is going on here? function should be checked/tested, normally off our radar because not used in auto-extend mode
int CVoteManager::GetNeededExtendCount()
{
	int iOnlinePlayers = 0.0f;
	int iVoteCount = 0;

	if (!GetGlobals())
		return 0;

	for (int i = 0; i < GetGlobals()->maxClients; i++)
	{
		ZEPlayer* pPlayer = g_playerManager->GetPlayer(i);

		if (pPlayer && !pPlayer->IsFakeClient())
		{
			iOnlinePlayers++;
			if (pPlayer->GetExtendVote())
				iVoteCount++;
		}
	}

	return (int)(iOnlinePlayers * g_flExtendBeginRatio) + 1;
}

CON_COMMAND_CHAT(rtv, "- Vote to end the current map sooner")
{
	if (!g_bVoteManagerEnable || !GetGlobals())
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

	switch (g_pVoteManager->GetRTVState())
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

	if (pPlayer->GetRTVVote())
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "You have already rocked the vote (%i voted, %i needed).", g_pVoteManager->GetCurrentRTVCount(), g_pVoteManager->GetNeededRTVCount());
		return;
	}

	if (pPlayer->GetRTVVoteTime() + 60.0f > GetGlobals()->curtime)
	{
		int iRemainingTime = (int)(pPlayer->GetRTVVoteTime() + 60.0f - GetGlobals()->curtime);
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Wait %i seconds before you can RTV again.", iRemainingTime);
		return;
	}

	pPlayer->SetRTVVote(true);
	pPlayer->SetRTVVoteTime(GetGlobals()->curtime);

	if (!g_pVoteManager->CheckRTVStatus())
		ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX "%s wants to rock the vote (%i voted, %i needed).", player->GetPlayerName(), g_pVoteManager->GetCurrentRTVCount(), g_pVoteManager->GetNeededRTVCount());
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
	if (!g_bVoteManagerEnable || !GetGlobals() || !g_pGameRules)
		return;

	if (!player)
	{
		ClientPrint(player, HUD_PRINTCONSOLE, CHAT_PREFIX "You cannot use this command from the server console.");
		return;
	}

	switch (g_ExtendVoteMode)
	{
		case EExtendVoteMode::EXTENDVOTE_OFF:
			ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Extend votes are disabled.");
			return;
		case EExtendVoteMode::EXTENDVOTE_ADMINONLY:
			ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Extend votes are disabled.");
			return;
		case EExtendVoteMode::EXTENDVOTE_AUTO:
		{
			if (g_pVoteManager->GetExtendState() == EExtendState::EXTEND_ALLOWED)
			{
				ConVar* cvar = g_pCVar->GetConVar(g_pCVar->FindConVar("mp_timelimit"));

				// CONVAR_TODO
				// HACK: values is actually the cvar value itself, hence this ugly cast.
				float flTimelimit = *(float*)&cvar->values;
				if (flTimelimit <= 0.0)
					return;

				float flTimeleft = (g_pGameRules->m_flGameStartTime + flTimelimit * 60.0f) - GetGlobals()->curtime;
				int iTimeTillVote = (int)(flTimeleft - (g_flExtendVoteStartTime * 60.0));

				div_t div = std::div(iTimeTillVote, 60);
				int iMinutesLeft = div.quot;
				int iSecondsLeft = div.rem;

				if (iMinutesLeft > 0)
					ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "An extend vote will start in %im %is", iMinutesLeft, iSecondsLeft);
				else
					ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "An extend vote will start in %i seconds", iSecondsLeft);
				return;
			}
		}
	}

	int iPlayer = player->GetPlayerSlot();

	ZEPlayer* pPlayer = g_playerManager->GetPlayer(iPlayer);

	// Something has to really go wrong for this to happen
	if (!pPlayer)
	{
		Warning("%s Tried to access a null ZEPlayer!!\n", player->GetPlayerName());
		return;
	}

	switch (g_pVoteManager->GetExtendState())
	{
		case EExtendState::MAP_START:
			ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Extend vote is not open yet.");
			return;
		case EExtendState::IN_PROGRESS:
			ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "An extend vote is in progress right now!");
			return;
		case EExtendState::POST_EXTEND_COOLDOWN:
			ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Extend vote is not open yet.");
			return;
		case EExtendState::POST_EXTEND_NO_EXTENDS_LEFT:
			ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "There are no extends left for the current map.");
			return;
		case EExtendState::POST_EXTEND_FAILED:
			ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "A previous extend vote already failed.");
			return;
		case EExtendState::POST_LAST_ROUND_END:
			ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Extend vote is closed during next map selection.");
			return;
		case EExtendState::POST_RTV:
			ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Extend vote is closed because RTV vote has passed.");
			return;
		case EExtendState::NO_EXTENDS:
			ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Extend vote is not allowed for current map.");
			return;
	}

	int iCurrentExtendCount = g_pVoteManager->GetCurrentExtendCount();
	int iNeededExtendCount = g_pVoteManager->GetNeededExtendCount();

	if (pPlayer->GetExtendVote())
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "You have already voted to extend the map (%i voted, %i needed).", iCurrentExtendCount, iNeededExtendCount);
		return;
	}

	if (pPlayer->GetExtendVoteTime() + 60.0f > GetGlobals()->curtime)
	{
		int iRemainingTime = (int)(pPlayer->GetExtendVoteTime() + 60.0f - GetGlobals()->curtime);
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Wait %i seconds before you can vote extend again.", iRemainingTime);
		return;
	}

	if (iCurrentExtendCount + 1 >= iNeededExtendCount)
	{
		g_pVoteManager->StartExtendVote(VOTE_CALLER_SERVER);

		return;
	}

	pPlayer->SetExtendVote(true);
	pPlayer->SetExtendVoteTime(GetGlobals()->curtime);
	ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX "%s wants to extend the map (%i voted, %i needed).", player->GetPlayerName(), iCurrentExtendCount + 1, iNeededExtendCount);
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

	if (g_ExtendVoteMode != EExtendVoteMode::EXTENDVOTE_MANUAL)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Voting to start extend votes is disabled.");
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

CON_COMMAND_CHAT_FLAGS(adminve, "Start a vote extend immediately.", ADMFLAG_CHANGEMAP)
{
	if (!g_bVoteManagerEnable)
		return;

	if (g_ExtendVoteMode == EExtendVoteMode::EXTENDVOTE_OFF)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Extend votes are disabled.");
		return;
	}

	if (g_pVoteManager->GetExtendState() == EExtendState::IN_PROGRESS || g_pVoteManager->IsVoteStarting())
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "An extend vote is already in progress.");
		return;
	}

	int slot = VOTE_CALLER_SERVER;
	if (player)
		slot = player->GetPlayerSlot();

	g_pVoteManager->StartExtendVote(slot);
}

CON_COMMAND_CHAT_FLAGS(disablertv, "- Disable the ability for players to vote to end current map sooner", ADMFLAG_CHANGEMAP)
{
	if (!g_bVoteManagerEnable)
		return;

	if (g_pVoteManager->GetRTVState() == ERTVState::BLOCKED_BY_ADMIN)
	{
		if (player)
			ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "RTV is already disabled.");
		else
			ConMsg("RTV is already disabled.");
		return;
	}

	const char* pszCommandPlayerName = player ? player->GetPlayerName() : CONSOLE_NAME;

	g_pVoteManager->SetRTVState(ERTVState::BLOCKED_BY_ADMIN);

	ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX ADMIN_PREFIX "disabled vote for RTV.", pszCommandPlayerName);
}

CON_COMMAND_CHAT_FLAGS(enablertv, "- Restore the ability for players to vote to end current map sooner", ADMFLAG_CHANGEMAP)
{
	if (!g_bVoteManagerEnable)
		return;

	if (g_pVoteManager->GetRTVState() == ERTVState::RTV_ALLOWED)
	{
		if (player)
			ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "RTV is not disabled.");
		else
			ConMsg("RTV is not disabled.");
		return;
	}

	const char* pszCommandPlayerName = player ? player->GetPlayerName() : CONSOLE_NAME;

	g_pVoteManager->SetRTVState(ERTVState::RTV_ALLOWED);

	ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX ADMIN_PREFIX "enabled vote for RTV.", pszCommandPlayerName);
}

CON_COMMAND_CHAT(extendsleft, "- Display amount of extends left for the current map")
{
	if (!g_bVoteManagerEnable)
		return;

	if (g_iMaxExtends - g_pVoteManager->GetExtends() <= 0)
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "There are no extends left, the map was already extended %i/%i times.", g_pVoteManager->GetExtends(), g_iMaxExtends);
	else if (g_pVoteManager->GetExtendState() == EExtendState::POST_EXTEND_FAILED)
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "The map had %i/%i extends left, but the last extend vote failed.", g_iMaxExtends - g_pVoteManager->GetExtends(), g_iMaxExtends);
	else
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "The map has %i/%i extends left.", g_iMaxExtends - g_pVoteManager->GetExtends(), g_iMaxExtends);
}

CON_COMMAND_CHAT(timeleft, "- Display time left to end of current map.")
{
	if (!GetGlobals() || !g_pGameRules)
		return;

	if (!player)
	{
		ClientPrint(player, HUD_PRINTCONSOLE, CHAT_PREFIX "You cannot use this command from the server console.");
		return;
	}

	ConVar* cvar = g_pCVar->GetConVar(g_pCVar->FindConVar("mp_timelimit"));

	// CONVAR_TODO
	// HACK: values is actually the cvar value itself, hence this ugly cast.
	float flTimelimit = *(float*)&cvar->values;

	if (flTimelimit == 0.0f)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "No time limit");
		return;
	}

	int iTimeleft = (int)((g_pGameRules->m_flGameStartTime + flTimelimit * 60.0f) - GetGlobals()->curtime);

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

void CVoteManager::ExtendMap(int iMinutes, bool bAllowExtraTime)
{
	if (!GetGlobals() || !g_pGameRules)
		return;

	// CONVAR_TODO
	ConVar* cvar = g_pCVar->GetConVar(g_pCVar->FindConVar("mp_timelimit"));

	// CONVAR_TODO
	// HACK: values is actually the cvar value itself, hence this ugly cast.
	float flTimelimit = *(float*)&cvar->values;

	if (bAllowExtraTime && GetGlobals()->curtime - g_pGameRules->m_flGameStartTime > flTimelimit * 60)
		flTimelimit = (GetGlobals()->curtime - g_pGameRules->m_flGameStartTime) / 60.0f + iMinutes;
	else
		flTimelimit += iMinutes;

	if (flTimelimit <= 0)
		flTimelimit = 0.01f;

	char buf[32];
	V_snprintf(buf, sizeof(buf), "mp_timelimit %.6f", flTimelimit);

	// CONVAR_TODO
	g_pEngineServer2->ServerCommand(buf);
}

void CVoteManager::VoteExtendHandler(YesNoVoteAction action, int param1, int param2)
{
	switch (action)
	{
		case YesNoVoteAction::VoteAction_Start:
		{
			ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX "Extend vote started!");
			break;
		}
		case YesNoVoteAction::VoteAction_Vote: // param1 = client slot, param2 = choice (VOTE_OPTION1=yes, VOTE_OPTION2=no)
		{
			CCSPlayerController* pController = CCSPlayerController::FromSlot(param1);
			if (!pController || !pController->IsController() || !pController->IsConnected())
				break;
			ClientPrint(pController, HUD_PRINTTALK, CHAT_PREFIX "Thanks for voting! Type !revote to change your vote!");
			break;
		}
		case YesNoVoteAction::VoteAction_End:
		{
			if ((YesNoVoteEndReason)param1 == YesNoVoteEndReason::VoteEnd_Cancelled)
			{
				// Admin cancelled so stop further votes
				// It will reenable if an admin manually calls a vote
				if (g_ExtendVoteMode == EExtendVoteMode::EXTENDVOTE_AUTO)
					m_ExtendState = EExtendState::POST_EXTEND_FAILED;
			}

			break;
		}
	}
}

// return true to show vote pass, false to show fail
bool CVoteManager::VoteExtendEndCallback(YesNoVoteInfo info)
{
	// ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX "Vote end: numvotes:%d yes:%d no:%d numclients:%d", info.num_votes, info.yes_votes, info.no_votes, info.num_clients);

	float yes_percent = 0.0f;

	if (info.num_votes > 0)
		yes_percent = (float)info.yes_votes / (float)info.num_votes;

	ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX "Vote ended. Yes received %.2f%% of %d votes (Needed %.2f%%)", yes_percent * 100.0f, info.num_votes, g_flExtendSucceedRatio * 100.0f);

	if (yes_percent >= g_flExtendSucceedRatio)
	{
		ExtendMap(g_iExtendTimeToAdd);
		m_iExtends++;

		if (g_iMaxExtends - m_iExtends <= 0)
			// there are no extends left after a successfull extend vote
			m_ExtendState = EExtendState::POST_EXTEND_NO_EXTENDS_LEFT;
		else
		{
			// there's an extend left after a successfull extend vote
			if (g_ExtendVoteMode == EExtendVoteMode::EXTENDVOTE_AUTO)
			{
				// small delay to allow cvar change to go through
				new CTimer(0.1, false, true, [this]() {
					m_ExtendState = EExtendState::EXTEND_ALLOWED;
					return -1.0f;
				});
			}
			else
			{
				m_ExtendState = EExtendState::POST_EXTEND_COOLDOWN;

				// Allow another extend vote after added time lapses
				new CTimer(g_iExtendTimeToAdd * 60.0f, false, true, [this]() {
					if (m_ExtendState == EExtendState::POST_EXTEND_COOLDOWN)
						m_ExtendState = EExtendState::EXTEND_ALLOWED;
					return -1.0f;
				});
			}
		}

		for (int i = 0; i < MAXPLAYERS; i++)
		{
			ZEPlayer* pPlayer = g_playerManager->GetPlayer(i);
			if (pPlayer)
				pPlayer->SetExtendVote(false);
		}

		ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX "Extend vote succeeded! Current map has been extended by %i minutes.", g_iExtendTimeToAdd);

		return true;
	}

	// Vote failed so we don't allow any more votes
	m_ExtendState = EExtendState::POST_EXTEND_FAILED;

	ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX "Extend vote failed! Further extend votes disabled!", g_iExtendTimeToAdd);

	return false;
}

void CVoteManager::StartExtendVote(int iCaller)
{
	if (m_ExtendState == EExtendState::IN_PROGRESS)
		return;

	char sDetailStr[64];
	V_snprintf(sDetailStr, sizeof(sDetailStr), "Vote to extend the map for another %d minutes", g_iExtendTimeToAdd);

	m_ExtendState = EExtendState::IN_PROGRESS;

	g_pPanoramaVoteHandler->SendYesNoVoteToAll(g_flExtendVoteDuration, iCaller, "#SFUI_vote_passed_nextlevel_extend", sDetailStr,
											   std::bind(&CVoteManager::VoteExtendEndCallback, this, std::placeholders::_1), std::bind(&CVoteManager::VoteExtendHandler, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

	new CTimer(g_flExtendVoteDuration - 3.0f, false, true, [this]() {
		if (m_iVoteEndTicks == 0 || m_ExtendState != EExtendState::IN_PROGRESS)
		{
			m_iVoteEndTicks = 3;
			return -1.0f;
		}

		ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX "Extend vote ending in %d....", m_iVoteEndTicks);
		m_iVoteEndTicks--;
		return 1.0f;
	});
}

void CVoteManager::OnRoundEnd()
{
	if (!GetGlobals() || !g_pGameRules)
		return;

	ConVar* cvar = g_pCVar->GetConVar(g_pCVar->FindConVar("mp_timelimit"));

	// CONVAR_TODO
	// HACK: values is actually the cvar value itself, hence this ugly cast.
	float flTimelimit = *(float*)&cvar->values;

	int iTimeleft = (int)((g_pGameRules->m_flGameStartTime + flTimelimit * 60.0f) - GetGlobals()->curtime);

	// check for end of last round
	if (iTimeleft < 0)
	{
		m_RTVState = ERTVState::POST_LAST_ROUND_END;
		m_ExtendState = EExtendState::POST_LAST_ROUND_END;
	}
}

bool CVoteManager::CheckRTVStatus()
{
	if (!g_bVoteManagerEnable || m_RTVState != ERTVState::RTV_ALLOWED || !GetGlobals() || !g_pGameRules)
		return false;

	int iCurrentRTVCount = GetCurrentRTVCount();
	int iNeededRTVCount = GetNeededRTVCount();

	if (iCurrentRTVCount >= iNeededRTVCount)
	{
		m_RTVState = ERTVState::POST_RTV_SUCCESSFULL;
		m_ExtendState = EExtendState::POST_RTV;
		// CONVAR_TODO
		g_pEngineServer2->ServerCommand("mp_timelimit 0.01");

		if (g_bRTVEndRound)
		{
			ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX "RTV succeeded! Ending the map now...");

			new CTimer(3.0f, false, true, []() {
				g_pGameRules->TerminateRound(5.0f, CSRoundEndReason::Draw);

				return -1.0f;
			});
		}
		else
		{
			ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX "RTV succeeded! This is the last round of the map!");
		}

		for (int i = 0; i < GetGlobals()->maxClients; i++)
		{
			ZEPlayer* pPlayer = g_playerManager->GetPlayer(i);
			if (pPlayer)
				pPlayer->SetRTVVote(false);
		}

		return true;
	}

	return false;
}
