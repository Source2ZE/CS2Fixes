#pragma once

#include "cbaseentity.h"
#include"../schema.h"

#define SF_TRIG_PUSH_ONCE 0x80

class CEnvEntityMaker : public Z_CBaseEntity
{
public:
	DECLARE_SCHEMA_CLASS(CEnvEntityMaker);

	SCHEMA_FIELD(CUtlSymbolLarge, m_iszTemplate)
};
