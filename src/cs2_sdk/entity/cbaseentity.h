#pragma once

#include "../schema.h"
#include "ccollisionproperty.h"
#include "mathlib/vector.h"
#include "baseentity.h"

CGlobalVars* GetGameGlobals();

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
	SCHEMA_FIELD(QAngle, m_vecAbsRotation)
	SCHEMA_FIELD(Vector, m_vRenderOrigin)
};

class CBodyComponent
{
public:
	DECLARE_SCHEMA_CLASS(CBodyComponent)

	SCHEMA_FIELD(CGameSceneNode *, m_pSceneNode)
};

class Z_CBaseEntity : public CBaseEntity
{
public:
	DECLARE_SCHEMA_CLASS(CBaseEntity)

	SCHEMA_FIELD(CBodyComponent *, m_CBodyComponent)
	SCHEMA_FIELD(CBitVec<64>, m_isSteadyState)
	SCHEMA_FIELD(float, m_lastNetworkChange)
	PSCHEMA_FIELD(void*, m_NetworkTransmitComponent)
	SCHEMA_FIELD(int, m_iHealth)
	SCHEMA_FIELD(int, m_iTeamNum)
	SCHEMA_FIELD(Vector, m_vecBaseVelocity)
	SCHEMA_FIELD(CCollisionProperty*, m_pCollision)

	int entindex() { return m_pEntity->m_EHandle.GetEntryIndex(); }

	Vector GetAbsOrigin() { return m_CBodyComponent()->m_pSceneNode()->m_vecAbsOrigin(); }

	void SetAbsOrigin(Vector vecOrigin) { m_CBodyComponent()->m_pSceneNode()->m_vecAbsOrigin(vecOrigin); }
};
