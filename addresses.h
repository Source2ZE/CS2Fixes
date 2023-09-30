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

namespace offsets
{
#ifdef _WIN32
	inline constexpr int GameEntitySystem = 0x58;
#else
	inline constexpr int GameEntitySystem = 0x50;
#endif
}

namespace sigs
{
#ifdef _WIN32
	inline const byte* Host_Say = (byte*)"\x44\x89\x4C\x24\x2A\x44\x88\x44\x24\x2A\x55\x53\x56\x57\x41\x54\x41\x55";
	inline const byte* UTIL_SayTextFilter = (byte*)"\x48\x89\x5C\x24\x2A\x55\x56\x57\x48\x8D\x6C\x24\x2A\x48\x81\xEC\x2A\x2A\x2A\x2A\x49\x8B\xD8";
	inline const byte* UTIL_SayText2Filter = (byte*)"\x48\x89\x5C\x24\x2A\x55\x56\x57\x48\x8D\x6C\x24\x2A\x48\x81\xEC\x2A\x2A\x2A\x2A\x41\x0F\xB6\xF8";
#else
	inline const byte* Host_Say = (byte*)"\x55\x48\x89\xE5\x41\x57\x49\x89\xFF\x41\x56\x41\x55\x41\x54\x4D\x89\xC4";
	inline const byte* UTIL_SayTextFilter = (byte*)"\x55\x48\x89\xE5\x41\x57\x49\x89\xD7\x31\xD2\x41\x56\x4C\x8D\x75\x98";
	inline const byte* UTIL_SayText2Filter = (byte*)"\x55\x48\x89\xE5\x41\x57\x4C\x8D\xBD\x2A\x2A\x2A\x2A\x41\x56\x4D\x89\xC6\x41\x55\x4D\x89\xCD";
#endif
}