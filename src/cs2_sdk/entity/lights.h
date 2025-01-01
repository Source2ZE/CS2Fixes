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

#include "cbasemodelentity.h"
#include "globaltypes.h"

class CLightComponent
{
public:
	DECLARE_SCHEMA_CLASS(CLightComponent)
};

class CLightEntity : public CBaseModelEntity
{
public:
	DECLARE_SCHEMA_CLASS(CLightEntity)

	SCHEMA_FIELD(CLightComponent*, m_CLightComponent)
};

class CBarnLight : public CBaseModelEntity
{
public:
	DECLARE_SCHEMA_CLASS(CBarnLight)

	SCHEMA_FIELD(bool, m_bEnabled)
	SCHEMA_FIELD(int, m_nColorMode) // 0 = color, 1 = color temperature
	SCHEMA_FIELD_POINTER(Color, m_Color)
	SCHEMA_FIELD(float, m_flColorTemperature) // default 6500
	SCHEMA_FIELD(float, m_flBrightness)
	SCHEMA_FIELD(float, m_flBrightnessScale)
	SCHEMA_FIELD(int, m_nDirectLight)	// Always set to 2 for dynamic
	SCHEMA_FIELD(int, m_nCastShadows)	// 0 = no, 1 = dynamic (and baked but pointless here)
	SCHEMA_FIELD(int, m_nShadowMapSize) // Shadowmap size in pixels (512 is a good starting value)
	SCHEMA_FIELD(int, m_nShadowPriority)
	SCHEMA_FIELD(bool, m_bContactShadow)
	SCHEMA_FIELD(float, m_flRange)
	SCHEMA_FIELD(float, m_flSkirt)	   // Falloff over the range
	SCHEMA_FIELD(float, m_flSkirtNear) // Falloff from the source
	SCHEMA_FIELD(float, m_flSoftX)
	SCHEMA_FIELD(float, m_flSoftY)
	SCHEMA_FIELD_POINTER(Vector, m_vSizeParams)

	// Artificially softens direct specular (0.0 to 1.0)
	SCHEMA_FIELD(float, m_flMinRoughness)
};

class COmniLight : public CBarnLight
{
public:
	DECLARE_SCHEMA_CLASS(COmniLight)

	SCHEMA_FIELD(float, m_flInnerAngle)
	SCHEMA_FIELD(float, m_flOuterAngle)
	SCHEMA_FIELD(bool, m_bShowLight)
};