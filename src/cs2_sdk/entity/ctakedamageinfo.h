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
#include "ehandle.h"
#include <platform.h>

enum DamageTypes_t : uint32_t
{
	DMG_GENERIC = 0x0,
	DMG_CRUSH = 0x1,
	DMG_BULLET = 0x2,
	DMG_SLASH = 0x4,
	DMG_BURN = 0x8,
	DMG_VEHICLE = 0x10,
	DMG_FALL = 0x20,
	DMG_BLAST = 0x40,
	DMG_CLUB = 0x80,
	DMG_SHOCK = 0x100,
	DMG_SONIC = 0x200,
	DMG_ENERGYBEAM = 0x400,
	DMG_DROWN = 0x4000,
	DMG_POISON = 0x8000,
	DMG_RADIATION = 0x10000,
	DMG_DROWNRECOVER = 0x20000,
	DMG_ACID = 0x40000,
	DMG_PHYSGUN = 0x100000,
	DMG_DISSOLVE = 0x200000,
	DMG_BLAST_SURFACE = 0x400000,
	DMG_BUCKSHOT = 0x1000000,
	DMG_LASTGENERICFLAG = 0x1000000,
	DMG_HEADSHOT = 0x2000000,
	DMG_DANGERZONE = 0x4000000,
};

enum TakeDamageFlags_t : uint32_t
{
	DFLAG_NONE = 0x0,
	DFLAG_SUPPRESS_HEALTH_CHANGES = 0x1,
	DFLAG_SUPPRESS_PHYSICS_FORCE = 0x2,
	DFLAG_SUPPRESS_EFFECTS = 0x4,
	DFLAG_PREVENT_DEATH = 0x8,
	DFLAG_FORCE_DEATH = 0x10,
	DFLAG_ALWAYS_GIB = 0x20,
	DFLAG_NEVER_GIB = 0x40,
	DFLAG_REMOVE_NO_RAGDOLL = 0x80,
	DFLAG_SUPPRESS_DAMAGE_MODIFICATION = 0x100,
	DFLAG_ALWAYS_FIRE_DAMAGE_EVENTS = 0x200,
	DFLAG_RADIUS_DMG = 0x400,
	DMG_LASTDFLAG = 0x400,
	DFLAG_IGNORE_ARMOR = 0x800,
};

struct AttackerInfo_t
{
	bool m_bNeedInit;
	bool m_bIsPawn;
	bool m_bIsWorld;
	CHandle<CCSPlayerPawn> m_hAttackerPawn;
	uint16_t m_nAttackerPlayerSlot;
	int m_iTeamChecked;
	int m_nTeam;
};
static_assert(sizeof(AttackerInfo_t) == 20);

// No idea what this is meant to have, but OnTakeDamage_Alive expects this and we only care about pInfo
struct CTakeDamageInfoContainer
{
	CTakeDamageInfo* pInfo;
};
class CGameTrace;

class CTakeDamageInfo
{
private:
	[[maybe_unused]] uint8_t __pad0000[0x8];

public:
	CTakeDamageInfo()
	{
		addresses::CTakeDamageInfo_Constructor(this, nullptr, nullptr, nullptr, &vec3_origin, &vec3_origin, 0.f, 0, 0, nullptr);
	}

	CTakeDamageInfo(CBaseEntity* pInflictor, CBaseEntity* pAttacker, CBaseEntity* pAbility, float flDamage, DamageTypes_t bitsDamageType)
	{
		addresses::CTakeDamageInfo_Constructor(this, pInflictor, pAttacker, pAbility, &vec3_origin, &vec3_origin, flDamage, bitsDamageType, 0, nullptr);
	}

	Vector m_vecDamageForce;	  // 0x8  |  8
	Vector m_vecDamagePosition;	  // 0x14 | 20
	Vector m_vecReportedPosition; // 0x20 | 32
	Vector m_vecDamageDirection;  // 0x2c | 44
	CBaseHandle m_hInflictor;	  // 0x38 | 56
	CBaseHandle m_hAttacker;	  // 0x3c | 60
	CBaseHandle m_hAbility;		  // 0x40 | 64
	float m_flDamage;			  // 0x44 | 68
	float m_flTotalledDamage;	  // 0x48 | 72
	int32_t m_bitsDamageType;	  // 0x4c | 76
	int32_t m_iDamageCustom;	  // 0x50 | 80
	int8_t m_iAmmoType;			  // 0x54 | 84

private:
	[[maybe_unused]] uint8_t m_nUnknown0[0xb]; // 0x55 | 85

public:
	float m_flOriginalDamage; // 0x60 | 96
	bool m_bShouldBleed;	  // 0x64 | 100
	bool m_bShouldSpark;	  // 0x65 | 101

private:
	[[maybe_unused]] uint8_t m_nUnknown1[0x2]; // 0x66

public:
	float m_flDamageAbsorbed;		  // 0x68 | 104
	CGameTrace* m_pTrace;			  // 0x70 | 112
	TakeDamageFlags_t m_nDamageFlags; // 0x78 | 120

private:
	[[maybe_unused]] uint8_t m_sDamageSourceName[0x8]; // 0x80 | 128

public:
	HitGroup_t m_iHitGroupId;					// 0x88 | 136
	int32_t m_nNumObjectsPenetrated;			// 0x8c | 140
	float m_flFriendlyFireDamageReductionRatio; // 0x90 | 144
private:
	uint8_t m_nUnknown2[0x5C]; // 0x94 | 148
public:
	void* m_hScriptInstance;	   // 0xf0 | 240
	AttackerInfo_t m_AttackerInfo; // 0xf8 | 248
	bool m_bInTakeDamageFlow;	   // 0x104 | 268

private:
	[[maybe_unused]] int32_t m_nUnknown3; // 0x108 | 272
};
static_assert(sizeof(CTakeDamageInfo) == 280);
