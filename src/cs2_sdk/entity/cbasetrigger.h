#pragma once

#include "cbasemodelentity.h"
#include"../schema.h"

#define SF_TRIG_PUSH_ONCE 0x80

class CBaseTrigger : public CBaseModelEntity
{
public:
	DECLARE_SCHEMA_CLASS(CBaseTrigger);

	SCHEMA_FIELD(CUtlSymbolLarge, m_iFilterName)
	SCHEMA_FIELD(CEntityHandle, m_hFilter)

	bool PassesTriggerFilters(Z_CBaseEntity *pOther)
	{
		static int offset = g_GameConfig->GetOffset("PassesTriggerFilters");
		return CALL_VIRTUAL(bool, offset, this, pOther);
	}
};
