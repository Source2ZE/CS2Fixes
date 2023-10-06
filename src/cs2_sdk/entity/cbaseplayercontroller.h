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

#pragma once

#include "ehandle.h"
#include "cbaseentity.h"
#include "ccsplayerpawn.h"

class CBasePlayerController : public Z_CBaseEntity
{
public:
	DECLARE_SCHEMA_CLASS(CBasePlayerController);

	SCHEMA_FIELD(uint64, m_steamID)
	SCHEMA_FIELD(CHandle<CBasePlayerPawn>, m_hPawn)
	SCHEMA_FIELD(char, m_iszPlayerName)

	CBasePlayerPawn *GetPawn() { return m_hPawn().Get(); }
};