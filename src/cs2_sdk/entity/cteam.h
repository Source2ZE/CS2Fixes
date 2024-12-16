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
#include "cbaseplayercontroller.h"
#include "cbaseplayerpawn.h"

class CTeam : public CBaseEntity
{
public:
	DECLARE_SCHEMA_CLASS(CTeam);

	SCHEMA_FIELD_POINTER(CUtlVector<CHandle<CBasePlayerController>>, m_aPlayerControllers)
	SCHEMA_FIELD_POINTER(CUtlVector<CHandle<CBasePlayerPawn>>, m_aPlayers)

	SCHEMA_FIELD(int32_t, m_iScore)
};
