
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

#pragma once

#include "panoramavote.h"

enum class ERTVState
{
	MAP_START,
	RTV_ALLOWED,
	POST_RTV_SUCCESSFULL,
	POST_LAST_ROUND_END,
	BLOCKED_BY_ADMIN,
};

enum class EExtendState
{
	MAP_START,
	EXTEND_ALLOWED,
	IN_PROGRESS,
	POST_EXTEND_COOLDOWN,
	POST_EXTEND_NO_EXTENDS_LEFT,
	POST_EXTEND_FAILED,
	POST_LAST_ROUND_END,
	POST_RTV,
	NO_EXTENDS,
};

enum EExtendVoteMode
{
	EXTENDVOTE_OFF,		  // No extend votes can be called
	EXTENDVOTE_ADMINONLY, // Only admins can initiate extend votes, all further options also include this
	EXTENDVOTE_MANUAL,	  // Extend votes are triggered by players typing !ve
	EXTENDVOTE_AUTO,	  // Extend votes can be triggered by !ve or when map timelimit reaches a given value
};

extern bool g_bVoteManagerEnable;

class CVoteManager
{
public:
	void VoteManager_Init();
	float TimerCheckTimeleft();
	int GetCurrentRTVCount();
	int GetNeededRTVCount();
	int GetCurrentExtendCount();
	int GetNeededExtendCount();
	void ExtendMap(int iMinutes, bool bAllowExtraTime = true);
	void VoteExtendHandler(YesNoVoteAction action, int param1, int param2);
	bool VoteExtendEndCallback(YesNoVoteInfo info);
	void StartExtendVote(int iCaller);
	void OnRoundEnd();
	bool CheckRTVStatus();

	ERTVState GetRTVState() { return m_RTVState; }
	EExtendState GetExtendState() { return m_ExtendState; }
	int GetExtends() { return m_iExtends; }
	bool IsVoteStarting() { return m_bVoteStarting; }
	void SetRTVState(ERTVState RTVState) { m_RTVState = RTVState; }
	void SetExtendState(EExtendState ExtendState) { m_ExtendState = ExtendState; }

private:
	ERTVState m_RTVState = ERTVState::MAP_START;
	EExtendState m_ExtendState = EExtendState::MAP_START;
	int m_iExtends = 0;
	int m_iVoteEndTicks = 3;
	int m_iVoteStartTicks = 3;
	bool m_bVoteStarting = false;
	const float m_flExtendVoteTickrate = 1.0f;
};

extern CVoteManager* g_pVoteManager;