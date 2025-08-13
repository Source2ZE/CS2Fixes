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

#include "cbaseentity.h"

enum PointOrientGoalDirectionType_t : uint32_t
{
	eAbsOrigin = 0,
	eCenter = 1,
	eHead = 2,
	eForward = 3,
	eEyesForward = 4,
};

enum PointOrientConstraint_t : uint32_t
{
	eNone = 0,
	ePreserveUpAxis = 1,
};

class CPointOrient : public CBaseEntity
{
public:
	DECLARE_SCHEMA_CLASS(CPointOrient)

	SCHEMA_FIELD(CUtlSymbolLarge, m_iszSpawnTargetName)
	SCHEMA_FIELD(CHandle<CBaseEntity>, m_hTarget)
	SCHEMA_FIELD(bool, m_bActive)
	SCHEMA_FIELD(PointOrientGoalDirectionType_t, m_nGoalDirection)
	SCHEMA_FIELD(PointOrientConstraint_t, m_nConstraint)
	SCHEMA_FIELD(float32, m_flMaxTurnRate)
	SCHEMA_FIELD(GameTime_t, m_flLastGameTime)
};