#pragma once
#include "platform.h"
#include "stdint.h"
#include "utils/module.h"

namespace modules
{
	inline CModule *engine;
	inline CModule *tier0;
	inline CModule *server;
	inline CModule *schemasystem;
	inline CModule *vscript;
	inline CModule *client;
	inline CModule *hammer;
}

typedef CUtlStringToken WorldGroupId_t;
class CEntityInstance;

namespace addresses
{
	void Initialize();

	inline void(FASTCALL *NetworkStateChanged)(int64 chainEntity, int64 offset, int64 a3);
	inline void(FASTCALL *StateChanged)(void *networkTransmitComponent, CEntityInstance *ent, int64 offset, int16 a4, int16 a5);
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
// Functions
	// look for string "\"Console<0>\" say \"%s\"\n"
	inline const byte *Host_Say = (byte *)"\x44\x89\x4C\x24\x2A\x44\x88\x44\x24\x2A\x55\x53\x56\x57\x41\x54\x41\x55";

	// both of these are called from Host_Say
	inline const byte *UTIL_SayTextFilter = (byte *)"\x48\x89\x5C\x24\x2A\x55\x56\x57\x48\x8D\x6C\x24\x2A\x48\x81\xEC\x2A\x2A\x2A\x2A\x49\x8B\xD8";
	inline const byte *UTIL_SayText2Filter = (byte *)"\x48\x89\x5C\x24\x2A\x55\x56\x57\x48\x8D\x6C\x24\x2A\x48\x81\xEC\x2A\x2A\x2A\x2A\x41\x0F\xB6\xF8";

	inline const byte *NetworkStateChanged = (byte *)"\x4C\x8B\xC9\x48\x8B\x09\x48\x85\xC9\x74\x2A\x48\x8B\x41\x10";
	inline const byte *StateChanged = (byte *)"\x48\x89\x54\x24\x10\x55\x53\x57\x41\x55";

	inline const byte *VoiceShouldHear = (byte *)"\x48\x89\x5C\x24\x10\x44\x88\x4C\x24\x20\x44\x88\x44\x24\x18";
	inline const byte *CSoundEmitterSystem_EmitSound = (byte *)"\x48\x8B\xC4\x4C\x89\x40\x18\x55\x57";

	// Has string "hammerUniqueId"
	inline const byte *CBaseEntity_Spawn = (byte *)"\x48\x89\x5C\x24\x10\x48\x89\x74\x24\x18\x57\x48\x83\xEC\x30\x48\x8B\xF2";

	// Is the 4th function in the CCSWeaponBase vtable
	inline const byte *CCSWeaponBase_Spawn = (byte *)"\x48\x89\x5C\x24\x08\x48\x89\x6C\x24\x18\x48\x89\x74\x24\x20\x57\x48\x83\xEC\x30\x48\x8B\xDA\x48\x8B\xE9\xE8\x2A\x2A\x2A\x2A";

// Patches
	// Check vauff's pin in #scripting
	inline const byte *MovementUnlock = (byte *)"\x76\x2A\xF2\x0F\x10\x57\x2A\xF3\x0F\x10\x2A\x2A\x0F\x28\xCA\xF3\x0F\x59\xC0";
	inline const byte *Patch_MovementUnlock = (byte *)"\xEB";

	// Check tilgep's pin in #scripting
	inline const byte *VScriptEnable = (byte *)"\xBE\x01\x2A\x2A\x2A\x2B\xD6\x74\x2A\x3B\xD6";
	inline const byte *Patch_VScriptEnable = (byte *)"\xBE\x02";

	// Find "Noise removal", there should be 3 customermachine checks
	inline const byte *HammerNoCustomerMachine = (byte *)"\xFF\x15\x2A\x2A\x2A\x2A\x84\xC0\x0F\x85\x2A\x2A\x2A\x2A\xB9";
	inline const byte *Patch_HammerNoCustomerMachine = (byte *)"\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90";
	inline const byte* IsHearingClient = (byte*)"\x40\x53\x48\x83\xEC\x20\x48\x8B\xD9\x3B\x91\xB8";
#else
	// Functions
	// look for string "\"Console<0>\" say \"%s\"\n"
	inline const byte *Host_Say = (byte *)"\x55\x48\x89\xE5\x41\x57\x49\x89\xFF\x41\x56\x41\x55\x41\x54\x4D\x89\xC4";

	// both of these are called from Host_Say
	inline const byte *UTIL_SayTextFilter = (byte *)"\x55\x48\x89\xE5\x41\x57\x49\x89\xD7\x31\xD2\x41\x56\x4C\x8D\x75\x98";
	inline const byte *UTIL_SayText2Filter = (byte *)"\x55\x48\x89\xE5\x41\x57\x4C\x8D\xBD\x2A\x2A\x2A\x2A\x41\x56\x4D\x89\xC6\x41\x55\x4D\x89\xCD";

	inline const byte *NetworkStateChanged = (byte *)"\x4C\x8B\x07\x4D\x85\xC0\x74\x2A\x49\x8B\x40\x10";
	inline const byte *StateChanged = (byte *)"\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x41\x54\x53\x89\xD3";

	inline const byte *VoiceShouldHear = (byte *)"\x55\x48\x89\xE5\x41\x57\x41\x89\xD7\x41\x56\x49\x89\xF6\x41\x55\x41\x54\x49\x89\xFC\x53\x48\x83\xEC\x28";
	inline const byte *CSoundEmitterSystem_EmitSound = (byte *)"\x48\xB8\x2A\x2A\x2A\x2A\xFF\xFF\xFF\xFF\x55\x48\x89\xE5\x41\x57\x41\x89\xF7";

	// Has string "hammerUniqueId"
	inline const byte *CBaseEntity_Spawn = (byte *)"\x55\x48\x89\xE5\x41\x56\x41\x55\x41\x54\x49\x89\xF4\x53\x48\x89\xFB\x48\x83\xEC\x10\xE8\x2A\x2A\x2A\x2A";

	// Is the 4th function in the CCSWeaponBase vtable
	inline const byte *CCSWeaponBase_Spawn = (byte *)"\x55\x48\x89\xE5\x41\x57\x41\x56\x4C\x8D\x75\xC0\x41\x55\x49\x89\xFD\x41\x54\x49\x89\xF4";

	// Patches
	// Check vauff's pin in #scripting
	inline const byte *MovementUnlock = (byte *)"\x0F\x87\x2A\x2A\x2A\x2A\x49\x8B\x7C\x24\x2A\xE8\x2A\x2A\x2A\x2A\x66\x0F\xEF\xED\x66\x0F\xD6\x85\x2A\x2A\x2A\x2A";
	inline const byte *Patch_MovementUnlock = (byte *)"\x90\x90\x90\x90\x90\x90";

	// Check tilgep's pin in #scripting
	inline const byte *VScriptEnable = (byte *)"\x83\xFE\x01\x0F\x84\x2A\x2A\x2A\x2A\x83";
	inline const byte *Patch_VScriptEnable = (byte *)"\x83\xFE\x02";

	inline const byte* IsHearingClient = (byte*)"\x55\x48\x89\xE5\x41\x55\x41\x54\x53\x48\x89\xFB\x48\x83\xEC\x08\x3B\xB7\xC8";
#endif
}