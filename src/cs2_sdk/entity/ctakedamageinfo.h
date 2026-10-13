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
#include "gametrace.h"
#include "shareddefs.h"
#include <platform.h>

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
	DECLARE_SCHEMA_CLASS(CTakeDamageInfo)

	CTakeDamageInfo()
	{
		Construct();
	}

	CTakeDamageInfo(CBaseEntity* pInflictor, CBaseEntity* pAttacker, CBaseEntity* pAbility, float flDamage, DamageTypes_t bitsDamageType)
	{
		Construct();
		m_hInflictor = pInflictor;
		m_hAttacker = pAttacker;
		m_hAbility = pAbility;
		m_flDamage = flDamage;
		m_bitsDamageType = bitsDamageType;
	}

	Vector m_vecDamageForce;		   // 0x8  |  8
	Vector m_vecDamagePosition;		   // 0x14 | 20
	Vector m_vecReportedPosition;	   // 0x20 | 32
	Vector m_vecDamageDirection;	   // 0x2c | 44
	CHandle<CBaseEntity> m_hInflictor; // 0x38 | 56
	CHandle<CBaseEntity> m_hAttacker;  // 0x3c | 60
	CHandle<CBaseEntity> m_hAbility;   // 0x40 | 64
	float m_flDamage;				   // 0x44 | 68
	float m_flTotalledDamage;		   // 0x48 | 72
	int32_t m_bitsDamageType;		   // 0x4c | 76
	int32_t m_iDamageCustom;		   // 0x50 | 80
	int8_t m_iAmmoType;				   // 0x54 | 84

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
	void* m_hScriptInstance;							// 0xe0 | 224
	AttackerInfo_t m_AttackerInfo;						// 0xe8 | 232
	CUtlLeanVector<int> m_DestructibleHitGroupRequests; // 0x100 | 256   CUtlLeanVector<DestructiblePartDamageRequest_t>
	bool m_bInTakeDamageFlow;							// 0x110 | 272

private:
	[[maybe_unused]] int32_t m_nUnknown4; // 0x11c | 284
};
static_assert(sizeof(CTakeDamageInfo) == 280);

struct CTakeDamageResult
{
public:
	CTakeDamageInfo* m_pOriginatingInfo;
	CUtlLeanVector<int> m_DestructibleHitGroupRequests; // CUtlLeanVector<DestructiblePartDamageRequest_t>
	int32_t m_nHealthLost;
	int32_t m_nHealthBefore;
	float m_flDamageDealt;
	float m_flPreModifiedDamage;
	VectorWS m_vDamagePosition;
	int32_t m_nTotalledHealthLost;
	float m_flTotalledDamageDealt;
	float m_flTotalledPreModifiedDamage;
	float m_flNewDamageAccumulatorValue;
	TakeDamageFlags_t m_nDamageFlags;
	bool m_bWasDamageSuppressed;
	bool m_bSuppressFlinch;
	HitGroup_t m_nOverrideFlinchHitGroup;

private:
	[[maybe_unused]] uint8_t m_nUnknown0[0x8];

public:
	void CopyFrom(CTakeDamageInfo* pInfo)
	{
		m_pOriginatingInfo = pInfo;
		m_nHealthLost = static_cast<int32_t>(pInfo->m_flDamage);
		m_nHealthBefore = 0;
		m_flDamageDealt = pInfo->m_flDamage;
		m_flPreModifiedDamage = pInfo->m_flDamage;
		m_nTotalledHealthLost = static_cast<int32_t>(pInfo->m_flDamage);
		m_flTotalledDamageDealt = pInfo->m_flDamage;
		m_bWasDamageSuppressed = false;
	}

	CTakeDamageResult() = delete;

	CTakeDamageResult(float damage) :
		m_pOriginatingInfo(nullptr),
		m_nHealthLost(static_cast<int32_t>(damage)),
		m_nHealthBefore(0),
		m_flDamageDealt(damage),
		m_flPreModifiedDamage(damage),
		m_nTotalledHealthLost(static_cast<int32_t>(damage)),
		m_flTotalledDamageDealt(damage),
		m_bWasDamageSuppressed(false)
	{
	}
};
static_assert(sizeof(CTakeDamageResult) == 96);