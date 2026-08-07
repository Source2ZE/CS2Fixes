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

template <typename Ret, class Class, typename... Args>
class CScriptFunction
{
public:
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
#ifdef PLATFORM_LINUX
		if (m_bVirtual)
		{
			auto pFunction = vmt::GetVMethod<Ret(FASTCALL*)(Class*, Args...)>(m_iOffset, pThis);

			return pFunction(pThis, args...);
		}
#endif

		return m_pFunction(pThis, args...);
	}

	inline bool IsVirtual() const
	{
#ifdef PLATFORM_LINUX
		return m_bVirtual;
#endif

		return false;
	}

	inline void* GetPtr() const
	{
#ifdef PLATFORM_LINUX
		if (IsVirtual())
			return nullptr;
#endif

		return m_pFunction;
	}

	inline int GetOffset() const
	{
#ifdef PLATFORM_LINUX
		if (IsVirtual())
			return m_iOffset;
#endif

		return -1;
	}

private:
	union
	{
		Ret(FASTCALL* m_pFunction)(Class*, Args...);
#ifdef PLATFORM_LINUX
		int m_iOffset;
#endif
	};

#ifdef PLATFORM_LINUX
	bool m_bVirtual = false;
#endif
};
