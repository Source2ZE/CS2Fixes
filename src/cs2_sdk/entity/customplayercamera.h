/**
 * =============================================================================
 * CS2Fixes
 * Copyright (C) 2023-2026 Source2ZE
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

#include "entity/ccsplayerpawn.h"

enum CustomCameraMode_t : uint8_t
{
	CUSTOM_CAMERA_MODE_DISABLED = 0,
	CUSTOM_CAMERA_MODE_CONTROLLED = 1,
	CUSTOM_CAMERA_MODE_CONTROLLED_POSITION = 2,
	CUSTOM_CAMERA_MODE_FOLLOW_POSITION = 3,
};

class CCSCustomPlayerCamera : public CBaseEntity
{
public:
	DECLARE_SCHEMA_CLASS(CCSCustomPlayerCamera)

	SCHEMA_FIELD(CHandle<CCSPlayerPawnBase>, m_hPawn)
	SCHEMA_FIELD(CustomCameraMode_t, m_nCameraMode)
	SCHEMA_FIELD(CHandle<CBaseEntity>, m_hFollowEntity)
	SCHEMA_FIELD(bool, m_bFollowEyes)
	SCHEMA_FIELD(Vector, m_vecFollowOffset)
	SCHEMA_FIELD(Vector, m_vecCameraOffset)
	SCHEMA_FIELD(bool, m_bClipCameraOffset)
	SCHEMA_FIELD(float32, m_flCameraOffsetReturnStrength)
};