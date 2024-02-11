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

#include "schema.h"
#include "ccollisionproperty.h"
#include "globaltypes.h"
#include "ctakedamageinfo.h"
#include "mathlib/vector.h"
#include "ehandle.h"
#include "entitykeyvalues.h"
#include "../../gameconfig.h"

extern CGameConfig *g_GameConfig;

class CGameSceneNode
{
public:
	DECLARE_SCHEMA_CLASS(CGameSceneNode)

	SCHEMA_FIELD(CEntityInstance *, m_pOwner)
	SCHEMA_FIELD(CGameSceneNode *, m_pParent)
	SCHEMA_FIELD(CGameSceneNode *, m_pChild)
	SCHEMA_FIELD(CNetworkOriginCellCoordQuantizedVector, m_vecOrigin)
	SCHEMA_FIELD(QAngle, m_angRotation)
	SCHEMA_FIELD(float, m_flScale)
	SCHEMA_FIELD(float, m_flAbsScale)
	SCHEMA_FIELD(Vector, m_vecAbsOrigin)
	SCHEMA_FIELD(QAngle, m_angAbsRotation)
	SCHEMA_FIELD(Vector, m_vRenderOrigin)

	matrix3x4_t EntityToWorldTransform()
	{
		matrix3x4_t mat;

		// issues with this and im tired so hardcoded it
		//AngleMatrix(this->m_angAbsRotation(), this->m_vecAbsOrigin(), mat);

		QAngle angles = this->m_angAbsRotation();
		float sr, sp, sy, cr, cp, cy;
		SinCos(DEG2RAD(angles[YAW]), &sy, &cy);
		SinCos(DEG2RAD(angles[PITCH]), &sp, &cp);
		SinCos(DEG2RAD(angles[ROLL]), &sr, &cr);
		mat[0][0] = cp * cy;
		mat[1][0] = cp * sy;
		mat[2][0] = -sp;

		float crcy = cr * cy;
		float crsy = cr * sy;
		float srcy = sr * cy;
		float srsy = sr * sy;
		mat[0][1] = sp * srcy - crsy;
		mat[1][1] = sp * srsy + crcy;
		mat[2][1] = sr * cp;

		mat[0][2] = (sp * crcy + srsy);
		mat[1][2] = (sp * crsy - srcy);
		mat[2][2] = cr * cp;

		Vector pos = this->m_vecAbsOrigin();
		mat[0][3] = pos.x;
		mat[1][3] = pos.y;
		mat[2][3] = pos.z;

		return mat;
	}
};

class CBodyComponent
{
public:
	DECLARE_SCHEMA_CLASS(CBodyComponent)

	SCHEMA_FIELD(CGameSceneNode *, m_pSceneNode)
};

class CEntitySubclassVDataBase
{
public:
	DECLARE_SCHEMA_CLASS(CEntitySubclassVDataBase)
};

class Z_CBaseEntity : public CBaseEntity
{
public:
	// This is a unique case as CBaseEntity is already defined in the sdk
	typedef Z_CBaseEntity ThisClass;
	static constexpr const char *ThisClassName = "CBaseEntity";
	static constexpr bool IsStruct = false;

	SCHEMA_FIELD(CBodyComponent *, m_CBodyComponent)
	SCHEMA_FIELD(CBitVec<64>, m_isSteadyState)
	SCHEMA_FIELD(float, m_lastNetworkChange)
	SCHEMA_FIELD_POINTER(CNetworkTransmitComponent, m_NetworkTransmitComponent)
	SCHEMA_FIELD(int, m_iHealth)
	SCHEMA_FIELD(int, m_iMaxHealth)
	SCHEMA_FIELD(int, m_iTeamNum)
	SCHEMA_FIELD(bool, m_bLagCompensate)
	SCHEMA_FIELD(Vector, m_vecAbsVelocity)
	SCHEMA_FIELD(Vector, m_vecBaseVelocity)
	SCHEMA_FIELD(CCollisionProperty*, m_pCollision)
	SCHEMA_FIELD(MoveType_t, m_MoveType)
	SCHEMA_FIELD(MoveType_t, m_nActualMoveType)
	SCHEMA_FIELD(uint32, m_spawnflags)
	SCHEMA_FIELD(uint32, m_fFlags)
	SCHEMA_FIELD(LifeState_t, m_lifeState)
	SCHEMA_FIELD_POINTER(CUtlStringToken, m_nSubclassID)
	SCHEMA_FIELD(float, m_flGravityScale)
	SCHEMA_FIELD(float, m_flSpeed)
	SCHEMA_FIELD(CUtlString, m_sUniqueHammerID);

	int entindex() { return m_pEntity->m_EHandle.GetEntryIndex(); }

	Vector GetAbsOrigin() { return m_CBodyComponent->m_pSceneNode->m_vecAbsOrigin; }
	QAngle GetAbsRotation() { return m_CBodyComponent->m_pSceneNode->m_angAbsRotation; }
	void SetAbsOrigin(Vector vecOrigin) { m_CBodyComponent->m_pSceneNode->m_vecAbsOrigin = vecOrigin; }
	void SetAbsRotation(QAngle angAbsRotation) { m_CBodyComponent->m_pSceneNode->m_angAbsRotation = angAbsRotation; }

	void SetAbsVelocity(Vector vecVelocity) { m_vecAbsVelocity = vecVelocity; }
	void SetBaseVelocity(Vector vecVelocity) { m_vecBaseVelocity = vecVelocity; }

	void TakeDamage(int iDamage)
	{
		m_iHealth = m_iHealth() - iDamage;
	}

	void Teleport(Vector *position, QAngle *angles, Vector *velocity)
	{
		static int offset = g_GameConfig->GetOffset("Teleport");
		CALL_VIRTUAL(void, offset, this, position, angles, velocity);
	}

	void CollisionRulesChanged()
	{
		static int offset = g_GameConfig->GetOffset("CollisionRulesChanged");
		CALL_VIRTUAL(void, offset, this);
	}

	bool IsPawn()
	{
		static int offset = g_GameConfig->GetOffset("IsEntityPawn");
		return CALL_VIRTUAL(bool, offset, this);
	}

	bool IsController()
	{
		static int offset = g_GameConfig->GetOffset("IsEntityController");
		return CALL_VIRTUAL(bool, offset, this);
	}

	void AcceptInput(const char *pInputName, variant_t value = variant_t(""), CEntityInstance *pActivator = nullptr, CEntityInstance *pCaller = nullptr)
	{
		addresses::CEntityInstance_AcceptInput(this, pInputName, pActivator, pCaller, &value, 0);
	}

	bool IsAlive() { return m_lifeState == LifeState_t::LIFE_ALIVE; }

	CHandle<CBaseEntity> GetHandle() { return m_pEntity->m_EHandle; }

	// A double pointer to entity VData is available 4 bytes past m_nSubclassID, if applicable
	CEntitySubclassVDataBase* GetVData() { return *(CEntitySubclassVDataBase**)((uint8*)(m_nSubclassID()) + 4); }

	void DispatchSpawn(CEntityKeyValues *pEntityKeyValues = nullptr)
	{
		addresses::DispatchSpawn(this, pEntityKeyValues);
	}

	// Emit a sound event
	void EmitSound(const char *pszSound, int nPitch = 100, float flVolume = 1.0, float flDelay = 0.0)
	{
		addresses::CBaseEntity_EmitSoundParams(this, pszSound, nPitch, flVolume, flDelay);
	}

	// This was needed so we can parent to nameless entities using pointers
	void SetParent(Z_CBaseEntity *pNewParent)
	{
		addresses::CBaseEntity_SetParent(this, pNewParent, 0, nullptr);
	}

	void Remove()
	{
		addresses::UTIL_Remove(this);
	}

	void SetMoveType(MoveType_t nMoveType)
	{
		m_MoveType = nMoveType; // necessary to maintain client prediction
		m_nActualMoveType = nMoveType;
	}
};

class SpawnPoint : public Z_CBaseEntity
{
public:
	DECLARE_SCHEMA_CLASS(SpawnPoint);

	SCHEMA_FIELD(bool, m_bEnabled);
};