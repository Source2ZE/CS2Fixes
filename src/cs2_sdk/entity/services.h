/**
 * =============================================================================
 * CS2Fixes
 * Copyright (C) 2023-2024 Source2ZE
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
#include "globaltypes.h"
#include <entity/ccsweaponbase.h>
#include <entity/ccsplayerpawn.h>
#include "gametrace.h"

class CBaseEntity;

struct CSPerRoundStats_t
{
public:
	DECLARE_SCHEMA_CLASS_INLINE(CSPerRoundStats_t)

	SCHEMA_FIELD(int, m_iKills)
	SCHEMA_FIELD(int, m_iDeaths)
	SCHEMA_FIELD(int, m_iAssists)
	SCHEMA_FIELD(int, m_iDamage)
};

struct CSMatchStats_t : public CSPerRoundStats_t
{
public:
	DECLARE_SCHEMA_CLASS_INLINE(CSMatchStats_t)

	SCHEMA_FIELD(int32_t, m_iEntryWins);
};

class CCSPlayerController_ActionTrackingServices
{
public:
	DECLARE_SCHEMA_CLASS(CCSPlayerController_ActionTrackingServices)

	SCHEMA_FIELD(CSMatchStats_t, m_matchStats)
};

class CPlayerPawnComponent
{
public:
	DECLARE_SCHEMA_CLASS(CPlayerPawnComponent);

	SCHEMA_FIELD(CCSPlayerPawn*, __m_pChainEntity)

	CCSPlayerPawn *GetPawn() { return __m_pChainEntity; }
};

class CPlayer_MovementServices : public CPlayerPawnComponent
{
public:
	DECLARE_SCHEMA_CLASS(CPlayer_MovementServices);

	SCHEMA_FIELD(CInButtonState, m_nButtons)
	SCHEMA_FIELD(uint64_t, m_nQueuedButtonDownMask)
	SCHEMA_FIELD(uint64_t, m_nQueuedButtonChangeMask)
	SCHEMA_FIELD(uint64_t, m_nButtonDoublePressed)

	// m_pButtonPressedCmdNumber[64]
	SCHEMA_FIELD_POINTER(uint32_t, m_pButtonPressedCmdNumber)
	SCHEMA_FIELD(uint32_t, m_nLastCommandNumberProcessed)
	SCHEMA_FIELD(uint64_t, m_nToggleButtonDownMask)
	SCHEMA_FIELD(float, m_flMaxspeed)
};

class CPlayer_MovementServices_Humanoid : public CPlayer_MovementServices
{
public:
	DECLARE_SCHEMA_CLASS(CPlayer_MovementServices_Humanoid);

	SCHEMA_FIELD(float, m_flFallVelocity)
	SCHEMA_FIELD(float, m_bInCrouch)
	SCHEMA_FIELD(uint32_t, m_nCrouchState)
	SCHEMA_FIELD(bool, m_bInDuckJump)
	SCHEMA_FIELD(float, m_flSurfaceFriction)
	SCHEMA_FIELD(bool, m_bDucked)
};

class CCSPlayer_MovementServices : public CPlayer_MovementServices_Humanoid
{
public:
	DECLARE_SCHEMA_CLASS(CCSPlayer_MovementServices);

	SCHEMA_FIELD(float, m_flMaxFallVelocity)
	SCHEMA_FIELD(float, m_flJumpVel)
	SCHEMA_FIELD(float, m_flStamina)
	SCHEMA_FIELD(float, m_flDuckSpeed)
	SCHEMA_FIELD(bool, m_bDuckOverride)
};

class CPlayer_WeaponServices : public CPlayerPawnComponent
{
public:
	DECLARE_SCHEMA_CLASS(CPlayer_WeaponServices);

	SCHEMA_FIELD_POINTER(CUtlVector<CHandle<CBasePlayerWeapon>>, m_hMyWeapons)
	SCHEMA_FIELD(CHandle<CBasePlayerWeapon>, m_hActiveWeapon)
};

class CCSPlayer_WeaponServices : public CPlayer_WeaponServices
{
public:
	DECLARE_SCHEMA_CLASS(CCSPlayer_WeaponServices);

	SCHEMA_FIELD(GameTime_t, m_flNextAttack)
	SCHEMA_FIELD(bool, m_bIsLookingAtWeapon)
	SCHEMA_FIELD(bool, m_bIsHoldingLookAtWeapon)

	SCHEMA_FIELD(CHandle<CBasePlayerWeapon>, m_hSavedWeapon)
	SCHEMA_FIELD(int32_t, m_nTimeToMelee)
	SCHEMA_FIELD(int32_t, m_nTimeToSecondary)
	SCHEMA_FIELD(int32_t, m_nTimeToPrimary)
	SCHEMA_FIELD(int32_t, m_nTimeToSniperRifle)
	SCHEMA_FIELD(bool, m_bIsBeingGivenItem)
	SCHEMA_FIELD(bool, m_bIsPickingUpItemWithUse)
	SCHEMA_FIELD(bool, m_bPickedUpWeapon)
};

class CCSPlayerController_InGameMoneyServices
{
public:
	DECLARE_SCHEMA_CLASS(CCSPlayerController_InGameMoneyServices);

    SCHEMA_FIELD(int, m_iAccount)
};

class CCSPlayer_ItemServices
{
public:
	DECLARE_SCHEMA_CLASS(CCSPlayer_ItemServices);
	
	virtual ~CCSPlayer_ItemServices() = 0;
private:
	virtual void unk_01() = 0;
	virtual void unk_02() = 0;
	virtual void unk_03() = 0;
	virtual void unk_04() = 0;
	virtual void unk_05() = 0;
	virtual void unk_06() = 0;
	virtual void unk_07() = 0;
	virtual void unk_08() = 0;
	virtual void unk_09() = 0;
	virtual void unk_10() = 0;
	virtual void unk_11() = 0;
	virtual void unk_12() = 0;
	virtual void unk_13() = 0;
	virtual void unk_14() = 0;
	virtual CBaseEntity* _GiveNamedItem(const char* pchName) = 0;
public:
    virtual bool         GiveNamedItemBool(const char* pchName)      = 0;
    virtual CBaseEntity* GiveNamedItem(const char* pchName)          = 0;
    virtual void         DropPlayerWeapon(CBasePlayerWeapon* weapon) = 0;
    virtual void         StripPlayerWeapons(bool removeSuit = false) = 0;
};

// We need an exactly sized class to be able to iterate the vector, our schema system implementation can't do this
class WeaponPurchaseCount_t
{
private:
	virtual void unk01() {};
	uint64_t unk1 = 0; // 0x8
	uint64_t unk2 = 0; // 0x10
	uint64_t unk3 = 0; // 0x18
	uint64_t unk4 = 0; // 0x20
	uint64_t unk5 = -1; // 0x28
public:
	uint16_t m_nItemDefIndex; // 0x30	
	uint16_t m_nCount; // 0x32
private:
	uint32_t unk6 = 0;
};

struct WeaponPurchaseTracker_t
{
public:
	DECLARE_SCHEMA_CLASS_INLINE(WeaponPurchaseTracker_t)

	SCHEMA_FIELD_POINTER(CUtlVector<WeaponPurchaseCount_t>, m_weaponPurchases)
};

class CCSPlayer_ActionTrackingServices
{
public:
	DECLARE_SCHEMA_CLASS(CCSPlayer_ActionTrackingServices)

	SCHEMA_FIELD(WeaponPurchaseTracker_t, m_weaponPurchasesThisRound)
};

class CPlayer_ObserverServices
{
public:
	DECLARE_SCHEMA_CLASS(CPlayer_ObserverServices)

	SCHEMA_FIELD(ObserverMode_t, m_iObserverMode)
	SCHEMA_FIELD(CHandle<Z_CBaseEntity>, m_hObserverTarget)
	SCHEMA_FIELD(ObserverMode_t, m_iObserverLastMode)
	SCHEMA_FIELD(bool, m_bForcedObserverMode)
};

struct touchlist_t {
	Vector deltavelocity;
	trace_t trace;
};

class CTraceFilterPlayerMovementCS : public CTraceFilter
{
};

class CTraceFilterHitAllTriggers: public CTraceFilter
{
public:
	CTraceFilterHitAllTriggers()
	{
		m_nInteractsAs = 0;
		m_nInteractsExclude = 0;
		m_nInteractsWith = 4;
		m_nEntityIdsToIgnore[0] = -1;
		m_nEntityIdsToIgnore[1] = -1;
		m_nOwnerIdsToIgnore[0] = -1;
		m_nOwnerIdsToIgnore[1] = -1;
		m_nObjectSetMask = RNQUERY_OBJECTS_ALL;
		m_nHierarchyIds[0] = 0;
		m_nHierarchyIds[1] = 0;
		m_bIterateEntities = true;
		m_nCollisionGroup = COLLISION_GROUP_DEBRIS;
		m_bHitTrigger = true;
	}
	CUtlVector<CEntityHandle> hitTriggerHandles;
	virtual ~CTraceFilterHitAllTriggers() { hitTriggerHandles.Purge(); }
	virtual bool ShouldHitEntity(CEntityInstance *other) override
	{
		hitTriggerHandles.AddToTail(other->GetRefEHandle());
		return false;
	}
};

struct MoveDataUnkSubtickStruct
{
	uint8_t unknown[24];
};

struct SubtickMove
{
	float when;
	uint64 button;
	bool pressed;
};

// Size: 0xE8
class CMoveData
{
public:
	CMoveData() = default;

	CMoveData(const CMoveData &source)
		// clang-format off
		: moveDataFlags {source.moveDataFlags}, 
		m_nPlayerHandle {source.m_nPlayerHandle},
		m_vecAbsViewAngles {source.m_vecAbsViewAngles},
		m_vecViewAngles {source.m_vecViewAngles},
		m_vecLastMovementImpulses {source.m_vecLastMovementImpulses},
		m_flForwardMove {source.m_flForwardMove}, 
		m_flSideMove {source.m_flSideMove}, 
		m_flUpMove {source.m_flUpMove},
		m_flSubtickFraction {source.m_flSubtickFraction}, 
		m_vecVelocity {source.m_vecVelocity}, 
		m_vecAngles {source.m_vecAngles},
		m_bHasSubtickInputs {source.m_bHasSubtickInputs},
		m_collisionNormal {source.m_collisionNormal},
		m_groundNormal {source.m_groundNormal}, 
		m_vecAbsOrigin {source.m_vecAbsOrigin},
		m_nGameModeMovedPlayer {source.m_nGameModeMovedPlayer},
		m_outWishVel {source.m_outWishVel},
		m_vecOldAngles {source.m_vecOldAngles}, 
		m_flMaxSpeed {source.m_flMaxSpeed}, 
		m_flClientMaxSpeed {source.m_flClientMaxSpeed},
		m_flSubtickAccelSpeed {source.m_flSubtickAccelSpeed}, 
		m_bJumpedThisTick {source.m_bJumpedThisTick},
		m_bOnGround {source.m_bOnGround},
		m_bShouldApplyGravity {source.m_bShouldApplyGravity}, 
		m_bGameCodeMovedPlayer {source.m_bGameCodeMovedPlayer}
	// clang-format on
	{
		for (int i = 0; i < source.m_AttackSubtickMoves.Count(); i++)
		{
			this->m_AttackSubtickMoves.AddToTail(source.m_AttackSubtickMoves[i]);
		}
		for (int i = 0; i < source.m_SubtickMoves.Count(); i++)
		{
			this->m_SubtickMoves.AddToTail(source.m_SubtickMoves[i]);
		}
		for (int i = 0; i < source.m_TouchList.Count(); i++)
		{
			auto touch = this->m_TouchList.AddToTailGetPtr();
			touch->deltavelocity = m_TouchList[i].deltavelocity;
			touch->trace.m_pSurfaceProperties = m_TouchList[i].trace.m_pSurfaceProperties;
			touch->trace.m_pEnt = m_TouchList[i].trace.m_pEnt;
			touch->trace.m_pHitbox = m_TouchList[i].trace.m_pHitbox;
			touch->trace.m_hBody = m_TouchList[i].trace.m_hBody;
			touch->trace.m_hShape = m_TouchList[i].trace.m_hShape;
			touch->trace.m_nContents = m_TouchList[i].trace.m_nContents;
			touch->trace.m_BodyTransform = m_TouchList[i].trace.m_BodyTransform;
			touch->trace.m_vHitNormal = m_TouchList[i].trace.m_vHitNormal;
			touch->trace.m_vHitPoint = m_TouchList[i].trace.m_vHitPoint;
			touch->trace.m_flHitOffset = m_TouchList[i].trace.m_flHitOffset;
			touch->trace.m_flFraction = m_TouchList[i].trace.m_flFraction;
			touch->trace.m_nTriangle = m_TouchList[i].trace.m_nTriangle;
			touch->trace.m_nHitboxBoneIndex = m_TouchList[i].trace.m_nHitboxBoneIndex;
			touch->trace.m_eRayType = m_TouchList[i].trace.m_eRayType;
			touch->trace.m_bStartInSolid = m_TouchList[i].trace.m_bStartInSolid;
			touch->trace.m_bExactHitPoint = m_TouchList[i].trace.m_bExactHitPoint;
		}
	}

public:
	uint8_t moveDataFlags;
	CHandle<CCSPlayerPawn> m_nPlayerHandle;
	QAngle m_vecAbsViewAngles;
	QAngle m_vecViewAngles;
	Vector m_vecLastMovementImpulses;
	float m_flForwardMove;
	float m_flSideMove; // Warning! Flipped compared to CS:GO, moving right gives negative value
	float m_flUpMove;
	float m_flSubtickFraction;
	Vector m_vecVelocity;
	Vector m_vecAngles;
	CUtlVector<SubtickMove> m_SubtickMoves;
	CUtlVector<SubtickMove> m_AttackSubtickMoves;
	bool m_bHasSubtickInputs;
	CUtlVector<touchlist_t> m_TouchList;
	Vector m_collisionNormal;
	Vector m_groundNormal; // unsure
	Vector m_vecAbsOrigin;
	bool m_nGameModeMovedPlayer;
	Vector m_outWishVel;
	Vector m_vecOldAngles;
	float m_flMaxSpeed;
	float m_flClientMaxSpeed;
	float m_flSubtickAccelSpeed; // Related to ground acceleration subtick stuff with sv_stopspeed and friction
	bool m_bJumpedThisTick;      // something to do with basevelocity and the tick the player jumps
	bool m_bOnGround;
	bool m_bShouldApplyGravity;
	bool m_bGameCodeMovedPlayer; // true if usercmd cmd number == (m_nGameCodeHasMovedPlayerAfterCommand + 1)
};

static_assert(offsetof(CMoveData, m_vecViewAngles) == 0x14);
static_assert(offsetof(CMoveData, m_vecVelocity) == 0x3c);
static_assert(offsetof(CMoveData, m_vecAngles) == 0x48);
static_assert(offsetof(CMoveData, m_vecAbsOrigin) == 0xc0);
static_assert(offsetof(CMoveData, m_outWishVel) == 0xd0);
static_assert(offsetof(CMoveData, m_flMaxSpeed) == 0xe8);
static_assert(offsetof(CMoveData, m_flClientMaxSpeed) == 0xec);
static_assert(offsetof(CMoveData, m_flSubtickAccelSpeed) == 0xf0);
static_assert(offsetof(CMoveData, m_bJumpedThisTick) == 0xf4);
static_assert(offsetof(CMoveData, m_bOnGround) == 0xf5);
static_assert(sizeof(CMoveData) == 248, "Class didn't match expected size");
