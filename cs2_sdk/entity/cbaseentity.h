#pragma once

#include "../schema.h"
#include "ccollisionproperty.h"
#include "mathlib/vector.h"

class CBaseEntity
{
public:
	DECLARE_SCHEMA_CLASS(CBaseEntity)

	SCHEMA_FIELD(int, m_iHealth)
	SCHEMA_FIELD(int, m_iTeamNum)
	SCHEMA_FIELD(Vector, m_vecBaseVelocity)
	SCHEMA_FIELD(CCollisionProperty*, m_pCollision)
};