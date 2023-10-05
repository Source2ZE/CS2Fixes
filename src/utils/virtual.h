#pragma once
#include "platform.h"

#define CALL_VIRTUAL(retType, idx, ...) \
	vmt::CallVirtual<retType>(idx, __VA_ARGS__)

namespace vmt
{
	template <typename T = void *>
	inline T GetVMethod(uint32 uIndex, void *pClass)
	{
		if (!pClass)
		{
			Warning("Tried getting virtual function from a null class.\n");
			return T();
		}

		void **pVTable = *static_cast<void ***>(pClass);
		if (!pVTable)
		{
			Warning("Tried getting virtual function from a null vtable.\n");
			return T();
		}

		return reinterpret_cast<T>(pVTable[uIndex]);
	}

	template <typename T, typename... Args>
	inline T CallVirtual(uint32 uIndex, void *pClass, Args... args)
	{
#ifdef _WIN32
		auto pFunc = GetVMethod<T(__thiscall *)(void *, Args...)>(uIndex, pClass);
#else
		auto pFunc = GetVMethod<T(__cdecl*)(void*, Args...)>(uIndex, pClass);
#endif
		if (!pFunc)
		{
			Warning("Tried calling a null virtual function.\n");
			return T();
		}

		return pFunc(pClass, args...);
	}
}