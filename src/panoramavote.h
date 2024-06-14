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

#pragma once

#include "common.h"
#include "eventlistener.h"
#include "irecipientfilter.h"
#include "utils/entity.h"
#include "entity/cvotecontroller.h"

#define VOTE_CALLER_SERVER 99

enum CastVote
{
	VOTE_NOTINCLUDED = -1,
	VOTE_OPTION1,  // Use this for Yes
	VOTE_OPTION2,  // Use this for No
	VOTE_OPTION3,
	VOTE_OPTION4,
	VOTE_OPTION5,
	VOTE_UNCAST = 5
};

enum YesNoVoteEndReason
{
	VoteEnd_AllVotes,	// All possible votes were cast
	VoteEnd_TimeUp,		// Time ran out
	VoteEnd_Cancelled,	// The vote got cancelled
};

enum YesNoVoteAction
{
	VoteAction_Start, // nothing passed
	VoteAction_Vote,  // param1 = client slot, param2 = choice (VOTE_OPTION1=yes, VOTE_OPTION2=no)
	VoteAction_End,   // param1 = YesNoVoteEndReason reason why the vote ended
};

#define VOTEINFO_CLIENT_SLOT 0
#define VOTEINFO_CLIENT_ITEM 1
struct YesNoVoteInfo
{
	int num_votes;					// Number of votes tallied in total
	int yes_votes;					// Number of votes for yes
	int no_votes;					// Number of votes for no
	int num_clients;				// Number of clients who could vote
	int clientInfo[MAXPLAYERS][2];	// Client voting info, user VOTEINFO_CLIENT_ defines
									//     Anything >= [num_clients] is VOTE_NOTINCLUDED, VOTE_UNCAST = client didn't vote
};

// Used to handle events relating to the vote menu
#define YesNoVoteHandler std::function<void(YesNoVoteAction, int, int)>

// Sends the vote results when the vote ends
// Return true for vote to pass, false to fail
#define YesNoVoteResult std::function<bool(YesNoVoteInfo)>

class CPanoramaVoteHandler
{
public:
	CPanoramaVoteHandler()
	{
		m_iVoteCount = 0;
		m_bIsVoteInProgress = false;
		m_VoteHandler = nullptr;
		m_VoteResult = nullptr;
	}
	int m_iVoteCount;

	void Reset();
	void Init();
	void VoteCast(IGameEvent* pEvent);

	void RemovePlayerFromVote(int iSlot);

	bool IsPlayerInVotePool(int iSlot);

	bool HasPlayerVoted(int iSlot);

	// Removes a client's vote and redraws the vote
	// True if client was in the vote, false if no ongoing vote
	bool RedrawVoteToClient(int iSlot);
	
	/* Returns true if a vote is in progress, false otherwise */
	bool IsVoteInProgress();
	
	// Ends the ongoing vote with the specified reason
	void EndVote(YesNoVoteEndReason reason);

	/* Start a new Yes/No vote
	*  
	*  @param flDuration		Maximum time to leave vote active for
	*  @param iCaller			Player slot of the vote caller. Use VOTE_CALLER_SERVER for 'Server'.
	*  @param sVoteTitle		Translation string to use as the vote message. (Only '#SFUI_vote' or '#Panorama_vote' strings)
	*  @param sDetailStr		Extra string used in some vote translation strings.
	*  @param pFilter			Recipient filter with all the clients who are allowed to participate in the vote.
	*  @param handler			Called when a menu action is completed.
	*  @param callback			Called when the vote has finished.
	*/
	bool SendYesNoVote(float flDuration, int iCaller, const char* sVoteTitle, const char* sDetailStr, IRecipientFilter *pFilter, YesNoVoteResult resultCallback, YesNoVoteHandler handler);

	/* Start a new Yes/No vote with all players included
	*
	*  @param flDuration		Maximum time to leave vote active for
	*  @param iCaller			Player slot of the vote caller. Use VOTE_CALLER_SERVER for 'Server'.
	*  @param sVoteTitle		Translation string to use as the vote message. (Only '#SFUI_vote' or '#Panorama_vote' strings)
	*  @param sDetailStr		Extra string used in some vote translation strings.
	*  @param handler			Called when a menu action is completed.
	*  @param callback			Called when the vote has finished.
	*/
	bool SendYesNoVoteToAll(float flDuration, int iCaller, const char* sVoteTitle, const char* sDetailStr, YesNoVoteResult resultCallback, YesNoVoteHandler handler);

	void SendVoteFailed();
	void SendVotePassed();

private:
	CHandle<CVoteController> hVoteController;
	bool m_bIsVoteInProgress;
	YesNoVoteHandler m_VoteHandler;
	YesNoVoteResult m_VoteResult;
	int m_iVoterCount;
	int m_iVoters[MAXPLAYERS];

	int m_iCurrentVoteCaller;
	char m_szCurrentVoteTitle[256];
	char m_szCurrentVoteDetailStr[256];

	void CheckForEarlyVoteClose();
	void InitVoters(IRecipientFilter* pFilter);
	void SendVoteStartUM(IRecipientFilter* pFilter);
	void UpdateVoteCounts();
};

extern CPanoramaVoteHandler *g_pPanoramaVoteHandler;
