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

#include "cstrike15_usermessages.pb.h"

#include "commands.h"
#include "common.h"
#include "cs2fixes.h"
#include "ctimer.h"
#include "engine/igameeventsystem.h"
#include "entity/cvotecontroller.h"
#include "networksystem/inetworkmessages.h"
#include "panoramavote.h"
#include "recipientfilters.h"
#include "utils/entity.h"
#include "utlstring.h"

#include "tier0/memdbgon.h"

extern IGameEventManager2* g_gameEventManager;
extern IGameEventSystem* g_gameEventSystem;
extern INetworkMessages* g_pNetworkMessages;

CPanoramaVoteHandler* g_pPanoramaVoteHandler = nullptr;

void CPanoramaVoteHandler::Reset()
{
	m_bIsVoteInProgress = false;
	hVoteController = nullptr;
	m_VoteHandler = nullptr;
	m_VoteResult = nullptr;
	m_szCurrentVoteTitle[0] = '\0';
	m_szCurrentVoteDetailStr[0] = '\0';
}

void CPanoramaVoteHandler::Init()
{
	if (m_bIsVoteInProgress)
		return;

	CVoteController* pVoteController = nullptr;
	while (nullptr != (pVoteController = (CVoteController*)UTIL_FindEntityByClassname(pVoteController, "vote_controller")))
		hVoteController = pVoteController->GetHandle();
}

// Called by vote_cast event
void CPanoramaVoteHandler::VoteCast(IGameEvent* pEvent)
{
	if (!hVoteController.Get() || !m_bIsVoteInProgress)
		return;

	if (m_VoteHandler != nullptr)
	{
		CCSPlayerController* pVoter = (CCSPlayerController*)pEvent->GetPlayerController("userid");
		if (!pVoter) return;
		(m_VoteHandler)(YesNoVoteAction::VoteAction_Vote, pVoter->GetPlayerSlot(), pEvent->GetInt("vote_option"));
	}

	CheckForEarlyVoteClose();

	// ClientPrintAll(HUD_PRINTTALK, "VOTE CAST: slot:%d option:%d", pVoter->GetPlayerSlot(), pEvent->GetInt("vote_option"));
}

void CPanoramaVoteHandler::RemovePlayerFromVote(int iSlot)
{
	if (!m_bIsVoteInProgress)
		return;

	bool found = false;
	for (int i = 0; i < m_iVoterCount; i++)
		if (m_iVoters[i] == iSlot)
			found = true;
		else if (found)
			m_iVoters[i - 1] = m_iVoters[i];

	if (found)
	{
		m_iVoterCount--;
		m_iVoters[m_iVoterCount] = -1;
	}
}

bool CPanoramaVoteHandler::IsPlayerInVotePool(int iSlot)
{
	if (!m_bIsVoteInProgress)
		return false;

	if (iSlot < 0 || iSlot > MAXPLAYERS)
		return false;

	for (int i = 0; i < m_iVoterCount; i++)
		if (m_iVoters[i] == iSlot)
			return true;

	return false;
}

// Removes a client's vote and redraws the vote
bool CPanoramaVoteHandler::RedrawVoteToClient(int iSlot)
{
	if (!hVoteController.Get())
		return false;

	int myVote = hVoteController->m_nVotesCast[iSlot];
	if (myVote != VOTE_UNCAST)
	{
		hVoteController->m_nVotesCast[iSlot] = VOTE_UNCAST;
		hVoteController->m_nVoteOptionCount[myVote]--;

		UpdateVoteCounts();
	}

	CRecipientFilter pFilter;
	pFilter.AddRecipient(CPlayerSlot(iSlot));
	SendVoteStartUM(&pFilter);

	return true;
}

void CPanoramaVoteHandler::UpdateVoteCounts()
{
	IGameEvent* pEvent = g_gameEventManager->CreateEvent("vote_changed");

	if (pEvent)
	{
		pEvent->SetInt("vote_option1", hVoteController->m_nVoteOptionCount[0]);
		pEvent->SetInt("vote_option2", hVoteController->m_nVoteOptionCount[1]);
		pEvent->SetInt("vote_option3", hVoteController->m_nVoteOptionCount[2]);
		pEvent->SetInt("vote_option4", hVoteController->m_nVoteOptionCount[3]);
		pEvent->SetInt("vote_option5", hVoteController->m_nVoteOptionCount[4]);
		pEvent->SetInt("potentialVotes", m_iVoterCount);

		g_gameEventManager->FireEvent(pEvent, false);
	}
}

bool CPanoramaVoteHandler::IsVoteInProgress()
{
	return m_bIsVoteInProgress;
}

bool CPanoramaVoteHandler::SendYesNoVote(float flDuration, int iCaller, const char* sVoteTitle, const char* sDetailStr, IRecipientFilter* pFilter, YesNoVoteResult resultCallback, YesNoVoteHandler handler = nullptr)
{
	if (!hVoteController.Get() || m_bIsVoteInProgress)
		return false;

	if (pFilter->GetRecipientCount() <= 0)
		return false;

	if (resultCallback == nullptr)
		return false;

	Message("[Vote Start] Starting a new vote [id:%d]. Duration:%.1f Caller:%d NumClients:%d", m_iVoteCount, flDuration, iCaller, pFilter->GetRecipientCount());

	m_bIsVoteInProgress = true;

	InitVoters(pFilter);

	hVoteController->m_nPotentialVotes = m_iVoterCount;
	hVoteController->m_bIsYesNoVote = true;
	hVoteController->m_iActiveIssueIndex = 2;

	hVoteController->m_iOnlyTeamToVote = -1; // use the recipient filter param to handle who votes

	m_VoteResult = resultCallback;
	m_VoteHandler = handler;

	m_iCurrentVoteCaller = iCaller;
	V_strcpy(m_szCurrentVoteTitle, sVoteTitle);
	V_strcpy(m_szCurrentVoteDetailStr, sDetailStr);

	UpdateVoteCounts();

	SendVoteStartUM(pFilter);

	if (m_VoteHandler != nullptr)
		(m_VoteHandler)(YesNoVoteAction::VoteAction_Start, 0, 0);

	int voteNum = m_iVoteCount;
	new CTimer(flDuration, false, true, [voteNum]() {
		// Ensure we dont end the wrong vote
		if (voteNum == g_pPanoramaVoteHandler->m_iVoteCount)
			g_pPanoramaVoteHandler->EndVote(YesNoVoteEndReason::VoteEnd_TimeUp);
		return -1.0;
	});

	return true;
}

bool CPanoramaVoteHandler::SendYesNoVoteToAll(float flDuration, int iCaller, const char* sVoteTitle, const char* sDetailStr, YesNoVoteResult resultCallback, YesNoVoteHandler handler = nullptr)
{
	CRecipientFilter filter;
	filter.AddAllPlayers();

	return SendYesNoVote(flDuration, iCaller, sVoteTitle, sDetailStr, &filter, resultCallback, handler);
}

void CPanoramaVoteHandler::SendVoteStartUM(IRecipientFilter* pFilter)
{
	INetworkMessageInternal* pNetMsg = g_pNetworkMessages->FindNetworkMessagePartial("VoteStart");
	auto data = pNetMsg->AllocateMessage()->ToPB<CCSUsrMsg_VoteStart>();

	data->set_team(-1);
	data->set_player_slot(m_iCurrentVoteCaller);
	data->set_vote_type(-1);
	data->set_disp_str(m_szCurrentVoteTitle);
	data->set_details_str(m_szCurrentVoteDetailStr);
	data->set_is_yes_no_vote(true);

	g_gameEventSystem->PostEventAbstract(-1, false, pFilter, pNetMsg, data, 0);

	delete data;
}

void CPanoramaVoteHandler::InitVoters(IRecipientFilter* pFilter)
{
	// Clear any old info
	m_iVoterCount = 0;
	for (int i = 0; i < MAXPLAYERS; i++)
	{
		m_iVoters[i] = -1;
		hVoteController->m_nVotesCast[i] = VOTE_UNCAST;
	}

	for (int i = 0; i < VOTE_UNCAST; i++)
		hVoteController->m_nVoteOptionCount[i] = 0;

	m_iVoterCount = pFilter->GetRecipientCount();
	for (int i = 0, j = 0; i < m_iVoterCount; i++)
	{
		CPlayerSlot slot = pFilter->GetRecipientIndex(i);
		if (slot.Get() != -1)
		{
			m_iVoters[j] = slot.Get();
			j++;
		}
	}
}

void CPanoramaVoteHandler::CheckForEarlyVoteClose()
{
	int votes = 0;
	votes += hVoteController->m_nVoteOptionCount[VOTE_OPTION1];
	votes += hVoteController->m_nVoteOptionCount[VOTE_OPTION2];

	if (votes >= m_iVoterCount)
	{
		// Do this next frame to prevent a crash
		new CTimer(0.0, false, true, []() {
			g_pPanoramaVoteHandler->EndVote(YesNoVoteEndReason::VoteEnd_AllVotes);
			return -1.0;
		});
	}
}

void CPanoramaVoteHandler::EndVote(YesNoVoteEndReason reason)
{
	if (!m_bIsVoteInProgress)
		return;

	m_bIsVoteInProgress = false;

	switch (reason)
	{
		case VoteEnd_AllVotes:
			Message("[Vote Ending] [id:%d] All possible players voted.", m_iVoteCount);
			break;
		case VoteEnd_TimeUp:
			Message("[Vote Ending] [id:%d] Time ran out.", m_iVoteCount);
			break;
		case VoteEnd_Cancelled:
			Message("[Vote Ending] [id:%d] The vote has been cancelled.", m_iVoteCount);
			break;
	}

	// Cycle global vote counter
	if (m_iVoteCount == 99)
		m_iVoteCount = 0;
	else
		m_iVoteCount++;

	if (m_VoteHandler != nullptr)
		(m_VoteHandler)(YesNoVoteAction::VoteAction_End, reason, 0);

	if (!hVoteController.Get())
	{
		SendVoteFailed();
		return;
	}

	if (m_VoteResult == nullptr || reason == YesNoVoteEndReason::VoteEnd_Cancelled)
	{
		SendVoteFailed();
		hVoteController->m_iActiveIssueIndex = -1;
		return;
	}

	YesNoVoteInfo info{};
	info.num_clients = m_iVoterCount;
	info.yes_votes = hVoteController->m_nVoteOptionCount[VOTE_OPTION1];
	info.no_votes = hVoteController->m_nVoteOptionCount[VOTE_OPTION2];
	info.num_votes = info.yes_votes + info.no_votes;

	for (int i = 0; i < MAXPLAYERS; i++)
	{
		if (i < m_iVoterCount)
		{
			info.clientInfo[i][VOTEINFO_CLIENT_SLOT] = m_iVoters[i];
			info.clientInfo[i][VOTEINFO_CLIENT_ITEM] = hVoteController->m_nVotesCast[m_iVoters[i]];
		}
		else
		{
			info.clientInfo[i][VOTEINFO_CLIENT_SLOT] = -1;
			info.clientInfo[i][VOTEINFO_CLIENT_ITEM] = -1;
		}
	}

	bool passed = (m_VoteResult)(info);
	// TODO usermessage correctly
	if (passed)
		SendVotePassed();
	else
		SendVoteFailed();
}

void CPanoramaVoteHandler::SendVoteFailed()
{
	INetworkMessageInternal* pNetMsg = g_pNetworkMessages->FindNetworkMessagePartial("VoteFailed");

	auto data = pNetMsg->AllocateMessage()->ToPB<CCSUsrMsg_VoteFailed>();

	data->set_reason(0);
	data->set_team(-1);

	CRecipientFilter pFilter;
	pFilter.AddAllPlayers();
	g_gameEventSystem->PostEventAbstract(-1, false, &pFilter, pNetMsg, data, 0);

	delete data;
}

void CPanoramaVoteHandler::SendVotePassed()
{
	INetworkMessageInternal* pNetMsg = g_pNetworkMessages->FindNetworkMessagePartial("VotePass");

	auto data = pNetMsg->AllocateMessage()->ToPB<CCSUsrMsg_VotePass>();

	data->set_team(-1);
	data->set_vote_type(2);
	data->set_disp_str("#SFUI_Vote_None");
	data->set_details_str("");

	CRecipientFilter pFilter;
	pFilter.AddAllPlayers();
	g_gameEventSystem->PostEventAbstract(-1, false, &pFilter, pNetMsg, data, 0);

	delete data;
}

CON_COMMAND_CHAT_FLAGS(cancelvote, "Cancels the ongoing vote.", ADMFLAG_CHANGEMAP)
{
	if (!g_pPanoramaVoteHandler)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Votes cannot be started/stopped at the moment.");
		return;
	}

	if (!g_pPanoramaVoteHandler->IsVoteInProgress())
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "No vote in progress.");
		return;
	}

	g_pPanoramaVoteHandler->EndVote(YesNoVoteEndReason::VoteEnd_Cancelled);

	const char* pszCommandPlayerName = player ? player->GetPlayerName() : "Console";
	ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX "Admin %s has cancelled the vote.", pszCommandPlayerName);
}

CON_COMMAND_CHAT(revote, "Change your vote in the ongoing vote.")
{
	if (!g_pPanoramaVoteHandler->IsVoteInProgress())
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "No vote in progress.");
		return;
	}

	int slot = player->GetPlayerSlot();
	if (!g_pPanoramaVoteHandler->RedrawVoteToClient(slot))
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "No vote in progress.");
		return;
	}
}
