
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

extern ERTVState g_RTVState;
extern EExtendState g_ExtendState;
extern bool g_bVoteManagerEnable;

void VoteManager_Init();
void SetExtendsLeft();
void ExtendMap(int iMinutes);

float TimerCheckTimeleft();
void StartExtendVote(int iCaller);