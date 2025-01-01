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
#include "schema.h"
#include "soundflags.h"
#include <platform.h>

enum InputBitMask_t : uint64_t
{
	// MEnumeratorIsNotAFlag
	IN_NONE = 0x0,
	// MEnumeratorIsNotAFlag
	IN_ALL = 0xffffffffffffffff,
	IN_ATTACK = 0x1,
	IN_JUMP = 0x2,
	IN_DUCK = 0x4,
	IN_FORWARD = 0x8,
	IN_BACK = 0x10,
	IN_USE = 0x20,
	IN_TURNLEFT = 0x80,
	IN_TURNRIGHT = 0x100,
	IN_MOVELEFT = 0x200,
	IN_MOVERIGHT = 0x400,
	IN_ATTACK2 = 0x800,
	IN_RELOAD = 0x2000,
	IN_SPEED = 0x10000,
	IN_JOYAUTOSPRINT = 0x20000,
	// MEnumeratorIsNotAFlag
	IN_FIRST_MOD_SPECIFIC_BIT = 0x100000000,
	IN_USEORRELOAD = 0x100000000,
	IN_SCORE = 0x200000000,
	IN_ZOOM = 0x400000000,
	IN_LOOK_AT_WEAPON = 0x800000000,
};

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

enum ObserverMode_t : uint8_t
{
	OBS_MODE_NONE = 0x0,
	OBS_MODE_FIXED = 0x1,
	OBS_MODE_IN_EYE = 0x2,
	OBS_MODE_CHASE = 0x3,
	OBS_MODE_ROAMING = 0x4,
	OBS_MODE_DIRECTED = 0x5,
	NUM_OBSERVER_MODES = 0x6,
};

typedef uint32 SoundEventGuid_t;
struct SndOpEventGuid_t
{
	SoundEventGuid_t m_nGuid;
	uint64 m_hStackHash;
};

// used with EmitSound_t
enum gender_t : uint8
{
	GENDER_NONE = 0x0,
	GENDER_MALE = 0x1,
	GENDER_FEMALE = 0x2,
	GENDER_NAMVET = 0x3,
	GENDER_TEENGIRL = 0x4,
	GENDER_BIKER = 0x5,
	GENDER_MANAGER = 0x6,
	GENDER_GAMBLER = 0x7,
	GENDER_PRODUCER = 0x8,
	GENDER_COACH = 0x9,
	GENDER_MECHANIC = 0xA,
	GENDER_CEDA = 0xB,
	GENDER_CRAWLER = 0xC,
	GENDER_UNDISTRACTABLE = 0xD,
	GENDER_FALLEN = 0xE,
	GENDER_RIOT_CONTROL = 0xF,
	GENDER_CLOWN = 0x10,
	GENDER_JIMMY = 0x11,
	GENDER_HOSPITAL_PATIENT = 0x12,
	GENDER_BRIDE = 0x13,
	GENDER_LAST = 0x14,
};

struct EmitSound_t
{
	EmitSound_t() :
		m_nChannel(0),
		m_pSoundName(0),
		m_flVolume(VOL_NORM),
		m_SoundLevel(SNDLVL_NONE),
		m_nFlags(0),
		m_nPitch(PITCH_NORM),
		m_pOrigin(0),
		m_flSoundTime(0.0f),
		m_pflSoundDuration(0),
		m_bEmitCloseCaption(true),
		m_bWarnOnMissingCloseCaption(false),
		m_bWarnOnDirectWaveReference(false),
		m_nSpeakerEntity(-1),
		m_UtlVecSoundOrigin(),
		m_nForceGuid(0),
		m_SpeakerGender(GENDER_NONE)
	{
	}
	int m_nChannel;
	const char* m_pSoundName;
	float m_flVolume;
	soundlevel_t m_SoundLevel;
	int m_nFlags;
	int m_nPitch;
	const Vector* m_pOrigin;
	float m_flSoundTime;
	float* m_pflSoundDuration;
	bool m_bEmitCloseCaption;
	bool m_bWarnOnMissingCloseCaption;
	bool m_bWarnOnDirectWaveReference;
	CEntityIndex m_nSpeakerEntity;
	CUtlVector<Vector, CUtlMemory<Vector, int> > m_UtlVecSoundOrigin;
	SoundEventGuid_t m_nForceGuid;
	gender_t m_SpeakerGender;
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

	// m_pButtonStates[0] is the mask of currently pressed buttons
	// m_pButtonStates[1] is the mask of buttons that changed in the current frame
	SCHEMA_FIELD_POINTER(uint64, m_pButtonStates)
};

class CGlowProperty
{
public:
	DECLARE_SCHEMA_CLASS_INLINE(CGlowProperty)

	SCHEMA_FIELD(Vector, m_fGlowColor)
	SCHEMA_FIELD(int, m_iGlowType)
	SCHEMA_FIELD(int, m_iGlowTeam)
	SCHEMA_FIELD(int, m_nGlowRange)
	SCHEMA_FIELD(int, m_nGlowRangeMin)
	SCHEMA_FIELD(Color, m_glowColorOverride)
	SCHEMA_FIELD(bool, m_bFlashing)
	SCHEMA_FIELD(bool, m_bGlowing)
};