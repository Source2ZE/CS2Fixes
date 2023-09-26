#pragma once

#include <platform.h>
#include "basehandle.h"

#define INVALID_EHANDLE_INDEX 0xFFFFFFFF

class CBaseEntity;

class CHandle : public CEntityHandle
{
	CBaseEntity *GetBaseEntity() const;

public:
	bool operator==(CHandle rhs) const { return m_Index == rhs.m_Index; }

	bool IsValid() const { return m_Index != INVALID_EHANDLE_INDEX; }

	int GetEntryIndex() const { return m_Parts.m_EntityIndex; }

	template <typename T = CBaseEntity>
	T *Get() const
	{
		return reinterpret_cast<T *>(GetBaseEntity());
	}
};