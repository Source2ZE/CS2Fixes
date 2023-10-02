#pragma once

#include "../schema.h"
#include "ccollisionproperty.h"
#include "mathlib/vector.h"
#include "baseentity.h"

CGlobalVars* GetGameGlobals();

class Z_CBaseEntity : public CBaseEntity
{
public:
	DECLARE_SCHEMA_CLASS(CBaseEntity)

	SCHEMA_FIELD(CBitVec<64>, m_isSteadyState)
	SCHEMA_FIELD(float, m_lastNetworkChange)
	PSCHEMA_FIELD(void*, m_NetworkTransmitComponent)
	SCHEMA_FIELD(int, m_iHealth)
	SCHEMA_FIELD(int, m_iTeamNum)
	SCHEMA_FIELD(Vector, m_vecBaseVelocity)
	SCHEMA_FIELD(CCollisionProperty*, m_pCollision)

	int entindex() { return m_pEntity->m_EHandle.GetEntryIndex(); }
};
