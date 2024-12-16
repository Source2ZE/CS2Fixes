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

#include "cbaseentity.h"
// #include "util_shared.h"

class CVoteController : public CBaseEntity
{
public:
	DECLARE_SCHEMA_CLASS(CVoteController);

	SCHEMA_FIELD(int, m_iActiveIssueIndex)
	SCHEMA_FIELD(int, m_iOnlyTeamToVote)
	SCHEMA_FIELD_POINTER(int, m_nVoteOptionCount)
	SCHEMA_FIELD(int, m_nPotentialVotes)
	SCHEMA_FIELD(bool, m_bIsYesNoVote)
	SCHEMA_FIELD_POINTER(int, m_nVotesCast)
	SCHEMA_FIELD(int, m_nHighestCountIndex)
	SCHEMA_FIELD_POINTER(CUtlVector<const char*>, m_VoteOptions)
};
