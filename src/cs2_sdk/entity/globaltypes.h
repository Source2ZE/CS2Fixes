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
#include <platform.h>
#include "schema.h"

struct GameTime_t
{
public:
	DECLARE_SCHEMA_CLASS_INLINE(GameTime_t)

	SCHEMA_FIELD(float, m_Value)
};

class CNetworkTransmitComponent
{
public:
	DECLARE_SCHEMA_CLASS_INLINE(CNetworkTransmitComponent)
};

class CNetworkVelocityVector
{
public:
	DECLARE_SCHEMA_CLASS_INLINE(CNetworkVelocityVector)

	SCHEMA_FIELD(float, m_vecX)
	SCHEMA_FIELD(float, m_vecY)
	SCHEMA_FIELD(float, m_vecZ)
};

class CNetworkOriginCellCoordQuantizedVector
{
public:
	DECLARE_SCHEMA_CLASS_INLINE(CNetworkOriginCellCoordQuantizedVector)

	SCHEMA_FIELD(uint16, m_cellX)
	SCHEMA_FIELD(uint16, m_cellY)
	SCHEMA_FIELD(uint16, m_cellZ)
	SCHEMA_FIELD(uint16, m_nOutsideWorld)

	// These are actually CNetworkedQuantizedFloat but we don't have the definition for it...
	SCHEMA_FIELD(float, m_vecX)
	SCHEMA_FIELD(float, m_vecY)
	SCHEMA_FIELD(float, m_vecZ)
};

class CInButtonState
{
public:
	DECLARE_SCHEMA_CLASS_INLINE(CInButtonState)

	// m_pButtonStates[3]
	SCHEMA_FIELD_POINTER(uint64_t, m_pButtonStates)
};

class CGlowProperty
{
public:
	DECLARE_SCHEMA_CLASS_INLINE(CGlowProperty)

	SCHEMA_FIELD(Vector, m_fGlowColor)
	SCHEMA_FIELD(int, m_iGlowType)
	SCHEMA_FIELD(int, m_nGlowRange)
	SCHEMA_FIELD(Color, m_glowColorOverride)
	SCHEMA_FIELD(bool, m_bFlashing)
	SCHEMA_FIELD(bool, m_bGlowing)
};