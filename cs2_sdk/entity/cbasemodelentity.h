#pragma once

#include "cbaseentity.h"
#include "cglowproperty.h"

class CBaseModelEntity : public Z_CBaseEntity
{
public:
	DECLARE_SCHEMA_CLASS(CBaseModelEntity);

	SCHEMA_FIELD(CCollisionProperty , m_Collision)
	SCHEMA_FIELD(CGlowProperty, m_Glow)
};