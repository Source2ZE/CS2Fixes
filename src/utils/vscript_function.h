/**
 * =============================================================================
 * CS2Fixes
 * Copyright (C) 2023-2026 Source2ZE
 * =============================================================================
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License, version 3.0, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "platform.h"
#include "virtual.h"

struct ScriptClassDesc_t;
class CBaseEntity;
class Vector;
class QAngle;

void* GetVScriptFunction(ScriptClassDesc_t* pScriptDesc, const char* pszFuncName);

template <typename Ret, class Class, typename... Args>
class CVScriptFunction
{
public:
	CVScriptFunction() :
		m_pFunction(nullptr), m_bVirtual(false) {}

	inline bool Initialize(void* pFunction)
	{
		if (!pFunction)
			return false;

#ifdef PLATFORM_LINUX
		// Itanium ABI: a pointer may encode the virtual index
		uintptr_t ptr = reinterpret_cast<uintptr_t>(pFunction);
		if (ptr & 1)
		{
			m_bVirtual = true;
			m_iOffset = static_cast<int>((ptr - 1) >> 3);

			return true;
		}
#endif

		m_pFunction = reinterpret_cast<decltype(m_pFunction)>(pFunction);

		return true;
	}

	inline Ret operator()(Class* pThis, Args... args)
	{
		if (m_bVirtual)
		{
			auto pFunction = vmt::GetVMethod<Ret(FASTCALL*)(Class*, Args...)>(m_iOffset, pThis);

			return pFunction(pThis, args...);
		}

		return m_pFunction(pThis, args...);
	}

	inline bool IsVirtual() const
	{
		return m_bVirtual;
	}

	inline void* GetPtr() const
	{
		if (IsVirtual())
			return nullptr;

		return reinterpret_cast<void*>(m_pFunction);
	}

	inline int GetOffset() const
	{
		if (IsVirtual())
			return m_iOffset;

		return -1;
	}

protected:
	union
	{
		Ret(FASTCALL* m_pFunction)(Class*, Args...);
		int m_iOffset;
	};

	bool m_bVirtual;
};
