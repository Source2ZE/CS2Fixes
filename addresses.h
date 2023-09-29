#pragma once
#include "platform.h"
#include "stdint.h"
#include "module.h"

namespace modules
{
	inline CModule *engine;
	inline CModule *tier0;
	inline CModule *server;
	inline CModule *schemasystem;
}

namespace addresses
{
	void Initialize();

	inline void(FASTCALL *GiveNamedItem)(uintptr_t itemService, const char *pchName, int64 iSubType, int64 pScriptItem, int a5, int64 bForce);
	inline void(FASTCALL *NetworkStateChanged)(int64 chainEntity, int64 offset, int64 a3);
}