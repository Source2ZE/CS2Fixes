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
#include "networktransmitcomponent.h"
#include "schema.h"
#include "shareddefs.h"
#include "soundflags.h"
#include <platform.h>

enum EInButtonState : uint32_t
{
	IN_BUTTON_UP = 0x0,
	IN_BUTTON_DOWN = 0x1,
	IN_BUTTON_DOWN_UP = 0x2,
	IN_BUTTON_UP_DOWN = 0x3,
	IN_BUTTON_UP_DOWN_UP = 0x4,
	IN_BUTTON_DOWN_UP_DOWN = 0x5,
	IN_BUTTON_DOWN_UP_DOWN_UP = 0x6,
	IN_BUTTON_UP_DOWN_UP_DOWN = 0x7,
	IN_BUTTON_STATE_COUNT = 0x8,
};

enum ParticleAttachment_t : uint32_t
{
	PATTACH_INVALID = 0xffffffff,
	PATTACH_ABSORIGIN = 0x0,		// Spawn at entity origin
	PATTACH_ABSORIGIN_FOLLOW = 0x1, // Spawn at and follow entity origin
	PATTACH_CUSTOMORIGIN = 0x2,
	PATTACH_CUSTOMORIGIN_FOLLOW = 0x3,
	PATTACH_POINT = 0x4,		// Spawn at attachment point
	PATTACH_POINT_FOLLOW = 0x5, // Spawn at and follow attachment point
	PATTACH_EYES_FOLLOW = 0x6,
	PATTACH_OVERHEAD_FOLLOW = 0x7,
	PATTACH_WORLDORIGIN = 0x8,
	PATTACH_ROOTBONE_FOLLOW = 0x9,
	PATTACH_RENDERORIGIN_FOLLOW = 0xa,
	PATTACH_MAIN_VIEW = 0xb,
	PATTACH_WATERWAKE = 0xc,
	PATTACH_CENTER_FOLLOW = 0xd,
	PATTACH_CUSTOM_GAME_STATE_1 = 0xe,
	PATTACH_HEALTHBAR = 0xf,
	MAX_PATTACH_TYPES = 0x10,
};

typedef uint32 SoundEventGuid_t;
struct SndOpEventGuid_t
{
	SoundEventGuid_t m_nGuid;
	uint32 m_hStackHash;
};

#pragma pack(push, 1)
struct StartSoundEventInfo
{
	SndOpEventGuid_t m_nSndOpEventGuid;
	int32 m_nFlags;
	uint64 m_nRecipients;
};
#pragma pack(pop)
static_assert(sizeof(StartSoundEventInfo) == 20); // Ensure alignment change works

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

	// m_pButtonStates[0] is the mask of currently pressed buttons
	// m_pButtonStates[1] is the mask of buttons that changed in the current frame
	SCHEMA_FIELD_POINTER(uint64, m_pButtonStates)
};

class CGlowProperty
{
public:
	DECLARE_SCHEMA_CLASS_BASE(CGlowProperty, 4)

	SCHEMA_FIELD(Vector, m_fGlowColor)
	SCHEMA_FIELD(int, m_iGlowType)
	SCHEMA_FIELD(int, m_iGlowTeam)
	SCHEMA_FIELD(int, m_nGlowRange)
	SCHEMA_FIELD(int, m_nGlowRangeMin)
	SCHEMA_FIELD(Color, m_glowColorOverride)
	SCHEMA_FIELD(bool, m_bFlashing)
	SCHEMA_FIELD(float, m_flGlowTime)
	SCHEMA_FIELD(float, m_flGlowStartTime)
	SCHEMA_FIELD(bool, m_bEligibleForScreenHighlight)
	SCHEMA_FIELD(bool, m_bGlowing)
};