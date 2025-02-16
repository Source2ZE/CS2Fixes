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

#include "../schema.h"
#include "cbaseentity.h"

class CMathCounter : public CBaseEntity
{
public:
	DECLARE_SCHEMA_CLASS(CMathCounter)

	SCHEMA_FIELD(float, m_flMin)
	SCHEMA_FIELD(float, m_flMax)
	SCHEMA_FIELD(bool, m_bHitMin)
	SCHEMA_FIELD(bool, m_bHitMax)
	SCHEMA_FIELD(bool, m_bDisabled)

	SCHEMA_FIELD_POINTER(char, m_OutValue) // TODO: CEntityOutputTemplate< float32 > m_OutValue

	float GetCounterValue()
	{
		float flCounterValue = *(float*)(m_OutValue() + 24);
		return flCounterValue;
	}
};