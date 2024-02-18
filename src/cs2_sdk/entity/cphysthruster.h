#pragma once

#include "../schema.h"
#include "cbaseentity.h"

class CPhysForce : public Z_CBaseEntity
{
	DECLARE_SCHEMA_CLASS(CPhysForce)
public:
	SCHEMA_FIELD(float, m_force)
};

class CPhysThruster : public CPhysForce
{
};