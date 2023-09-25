#pragma once
#include <platform.h>

namespace addresses
{
	void Initialize();

	inline void(__fastcall* GiveNamedItem)(uintptr_t itemService, const char* pchName, int64 iSubType, int64 pScriptItem, int a5, int64 bForce);
	inline void(__fastcall* NetworkStateChanged)(int64 chainEntity, int64 offset, int64 a3);
}