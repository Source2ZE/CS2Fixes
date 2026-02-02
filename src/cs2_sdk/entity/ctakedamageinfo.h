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
#include "ehandle.h"
#include <platform.h>

enum DamageTypes_t : uint32_t
{
	DMG_GENERIC = 0,			 // 0x0
	DMG_CRUSH = 1 << 0,			 // 0x1
	DMG_BULLET = 1 << 1,		 // 0x2
	DMG_SLASH = 1 << 2,			 // 0x4
	DMG_BURN = 1 << 3,			 // 0x8
	DMG_VEHICLE = 1 << 4,		 // 0x10
	DMG_FALL = 1 << 5,			 // 0x20
	DMG_BLAST = 1 << 6,			 // 0x40
	DMG_CLUB = 1 << 7,			 // 0x80
	DMG_SHOCK = 1 << 8,			 // 0x100
	DMG_SONIC = 1 << 9,			 // 0x200
	DMG_ENERGYBEAM = 1 << 10,	 // 0x400
	DMG_BUCKSHOT = 1 << 11,		 // 0x800
	DMG_DROWN = 1 << 14,		 // 0x4000
	DMG_POISON = 1 << 15,		 // 0x8000
	DMG_RADIATION = 1 << 16,	 // 0x10000
	DMG_DROWNRECOVER = 1 << 17,	 // 0x20000
	DMG_ACID = 1 << 18,			 // 0x40000
	DMG_PHYSGUN = 1 << 20,		 // 0x100000
	DMG_DISSOLVE = 1 << 21,		 // 0x200000
	DMG_BLAST_SURFACE = 1 << 22, // 0x400000
	DMG_HEADSHOT = 1 << 23,		 // 0x800000
};

enum TakeDamageFlags_t : uint64_t
{
	DFLAG_NONE = 0,								 // 0x0
	DFLAG_SUPPRESS_HEALTH_CHANGES = 1 << 0,		 // 0x1
	DFLAG_SUPPRESS_PHYSICS_FORCE = 1 << 1,		 // 0x2
	DFLAG_SUPPRESS_EFFECTS = 1 << 2,			 // 0x4
	DFLAG_PREVENT_DEATH = 1 << 3,				 // 0x8
	DFLAG_FORCE_DEATH = 1 << 4,					 // 0x10
	DFLAG_ALWAYS_GIB = 1 << 5,					 // 0x20
	DFLAG_NEVER_GIB = 1 << 6,					 // 0x40
	DFLAG_REMOVE_NO_RAGDOLL = 1 << 7,			 // 0x80
	DFLAG_SUPPRESS_DAMAGE_MODIFICATION = 1 << 8, // 0x100
	DFLAG_ALWAYS_FIRE_DAMAGE_EVENTS = 1 << 9,	 // 0x200
	DFLAG_RADIUS_DMG = 1 << 10,					 // 0x400
	DFLAG_FORCEREDUCEARMOR_DMG = 1 << 11,		 // 0x800
	DFLAG_SUPPRESS_INTERRUPT_FLINCH = 1 << 12,	 // 0x1000
	DFLAG_IGNORE_DESTRUCTIBLE_PARTS = 1 << 13,	 // 0x2000
	DFLAG_IGNORE_ARMOR = 1 << 14,				 // 0x4000
	DFLAG_SUPPRESS_UTILREMOVE = 1 << 15,		 // 0x8000
};

struct AttackerInfo_t
{
	bool m_bNeedInit;
	bool m_bIsPawn;
	bool m_bIsWorld;
	CHandle<CCSPlayerPawn> m_hAttackerPawn;
	int32_t m_nAttackerPlayerSlot;
	int m_iTeamChecked;
	int m_nTeam;
};
static_assert(sizeof(AttackerInfo_t) == 20);

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
	CGameTrace* m_pTrace;						// 0x68 | 104
	TakeDamageFlags_t m_nDamageFlags;			// 0x70 | 112
	HitGroup_t m_iHitGroupId;					// 0x78 | 120
	int32_t m_nNumObjectsPenetrated;			// 0x7c | 124
	float m_flFriendlyFireDamageReductionRatio; // 0x80 | 128
	bool m_bStoppedBullet;						// 0x84 | 132

private:
	uint8_t m_nUnknown2[0x58]; // 0x88 | 136

public:
	void* m_hScriptInstance;								// 0xe0 | 224
	AttackerInfo_t m_AttackerInfo;							// 0xe8 | 232
	CUtlVector<int> m_nDestructibleHitGroupsToForceDestroy; // 0x100 | 256
	bool m_bInTakeDamageFlow;								// 0x118 | 280

private:
	[[maybe_unused]] int32_t m_nUnknown4; // 0x11c | 284
};
static_assert(sizeof(CTakeDamageInfo) == 288);

struct CTakeDamageResult
{
	CTakeDamageInfo* m_pOriginatingInfo;
	int32_t m_nHealthLost;
	int32_t m_nHealthBefore;
	int32_t m_nDamageDealt;
	float m_flPreModifiedDamage;
	int32_t m_nTotalledHealthLost;
	int32_t m_nTotalledDamageDealt;
	float m_flTotalledPreModifiedDamage;
	bool m_bWasDamageSuppressed;
	bool m_bSuppressFlinch;
	HitGroup_t m_nOverrideFlinchHitGroup;

	void CopyFrom(CTakeDamageInfo* pInfo)
	{
		m_pOriginatingInfo = pInfo;
		m_nHealthLost = static_cast<int32_t>(pInfo->m_flDamage);
		m_nHealthBefore = 0;
		m_nDamageDealt = static_cast<int32_t>(pInfo->m_flDamage);
		m_flPreModifiedDamage = pInfo->m_flDamage;
		m_nTotalledHealthLost = static_cast<int32_t>(pInfo->m_flDamage);
		m_nTotalledDamageDealt = static_cast<int32_t>(pInfo->m_flDamage);
		m_bWasDamageSuppressed = false;
	}

	CTakeDamageResult() = delete;

	constexpr CTakeDamageResult(float damage) :
		m_pOriginatingInfo(nullptr),
		m_nHealthLost(static_cast<int32_t>(damage)),
		m_nHealthBefore(0),
		m_nDamageDealt(static_cast<int32_t>(damage)),
		m_flPreModifiedDamage(damage),
		m_nTotalledHealthLost(static_cast<int32_t>(damage)),
		m_nTotalledDamageDealt(static_cast<int32_t>(damage)),
		m_bWasDamageSuppressed(false)
	{
	}
};
static_assert(sizeof(CTakeDamageResult) == 48);