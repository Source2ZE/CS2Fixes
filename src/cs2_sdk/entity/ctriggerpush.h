#pragma once

#include "cbasemodelentity.h"
#include"../schema.h"

#define SF_TRIG_PUSH_ONCE 0x80

class CTriggerPush : public CBaseModelEntity
{
public:
	DECLARE_SCHEMA_CLASS(CTriggerPush);

	SCHEMA_FIELD(Vector, m_vecPushDirEntitySpace)
	SCHEMA_FIELD(bool, m_bTriggerOnStartTouch)

	bool PassesTriggerFilters(Z_CBaseEntity *pOther)
	{
		static int offset = g_GameConfig->GetOffset("PassesTriggerFilters");
		return CALL_VIRTUAL(bool, offset, this, pOther);
	}
};
