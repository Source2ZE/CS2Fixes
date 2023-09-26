#pragma once

#include <platform.h>

#define INVALID_EHANDLE_INDEX 0xFFFFFFFF
#define ENT_ENTRY_MASK		  0x7FFF

class CBaseEntity;

class CHandle
{
	CBaseEntity *GetBaseEntity() const;

public:
	bool operator==(CHandle rhs) const { return m_Index == rhs.m_Index; }

	bool IsValid() const { return m_Index != INVALID_EHANDLE_INDEX; }

	int GetEntryIndex() const { return m_Index & ENT_ENTRY_MASK; }

	template <typename T = CBaseEntity>
	T *Get() const
	{
		return reinterpret_cast<T *>(GetBaseEntity());
	}

	uint32 m_Index;
};