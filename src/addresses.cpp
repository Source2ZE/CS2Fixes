/**
 * =============================================================================
 * CS2Fixes
 * Copyright (C) 2023-2025 Source2ZE
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

#include "addresses.h"
#include "gameconfig.h"
#include "utils/module.h"

#include "tier0/memdbgon.h"

extern CGameConfig* g_GameConfig;

#define RESOLVE_SIG(gameConfig, name, variable)                        \
	variable = (decltype(variable))gameConfig->ResolveSignature(name); \
	if (!variable)                                                     \
		return false;                                                  \
	Message("Found %s at 0x%p\n", name, variable);

bool addresses::Initialize(CGameConfig* g_GameConfig)
{
	modules::engine = new CModule(ROOTBIN, "engine2");
	modules::tier0 = new CModule(ROOTBIN, "tier0");
	modules::server = new CModule(GAMEBIN, "server");
	modules::schemasystem = new CModule(ROOTBIN, "schemasystem");
	modules::vscript = new CModule(ROOTBIN, "vscript");
	modules::networksystem = new CModule(ROOTBIN, "networksystem");
	modules::vphysics2 = new CModule(ROOTBIN, "vphysics2");
	modules::matchmaking = new CModule(GAMEBIN, "matchmaking");
	modules::client = nullptr;

	if (!CommandLine()->HasParm("-dedicated"))
		modules::client = new CModule(GAMEBIN, "client");

#ifdef _WIN32
	modules::hammer = nullptr;
	if (CommandLine()->HasParm("-tools"))
		modules::hammer = new CModule(ROOTBIN, "tools/hammer");
#endif

	RESOLVE_SIG(g_GameConfig, "SetGroundEntity", addresses::SetGroundEntity);
	RESOLVE_SIG(g_GameConfig, "CCSPlayerController_SwitchTeam", addresses::CCSPlayerController_SwitchTeam);
	RESOLVE_SIG(g_GameConfig, "CBasePlayerController_SetPawn", addresses::CBasePlayerController_SetPawn);
	RESOLVE_SIG(g_GameConfig, "CBaseModelEntity_SetModel", addresses::CBaseModelEntity_SetModel);
	RESOLVE_SIG(g_GameConfig, "UTIL_Remove", addresses::UTIL_Remove);
	RESOLVE_SIG(g_GameConfig, "CEntitySystem_AddEntityIOEvent", addresses::CEntitySystem_AddEntityIOEvent);
	RESOLVE_SIG(g_GameConfig, "CEntityInstance_AcceptInput", addresses::CEntityInstance_AcceptInput);
	RESOLVE_SIG(g_GameConfig, "CGameEntitySystem_FindEntityByClassName", addresses::CGameEntitySystem_FindEntityByClassName);
	RESOLVE_SIG(g_GameConfig, "CGameEntitySystem_FindEntityByName", addresses::CGameEntitySystem_FindEntityByName);
	RESOLVE_SIG(g_GameConfig, "CGameRules_TerminateRound", addresses::CGameRules_TerminateRound);
	RESOLVE_SIG(g_GameConfig, "CreateEntityByName", addresses::CreateEntityByName);
	RESOLVE_SIG(g_GameConfig, "DispatchSpawn", addresses::DispatchSpawn);
	RESOLVE_SIG(g_GameConfig, "CEntityIdentity_SetEntityName", addresses::CEntityIdentity_SetEntityName);
	RESOLVE_SIG(g_GameConfig, "CBaseEntity_EmitSoundParams", addresses::CBaseEntity_EmitSoundParams);
	RESOLVE_SIG(g_GameConfig, "CBaseEntity_SetParent", addresses::CBaseEntity_SetParent);
	RESOLVE_SIG(g_GameConfig, "DispatchParticleEffect", addresses::DispatchParticleEffect);
	RESOLVE_SIG(g_GameConfig, "CBaseEntity_EmitSoundFilter", addresses::CBaseEntity_EmitSoundFilter);
	RESOLVE_SIG(g_GameConfig, "CBaseEntity_SetMoveType", addresses::CBaseEntity_SetMoveType);
	RESOLVE_SIG(g_GameConfig, "CTakeDamageInfo", addresses::CTakeDamageInfo_Constructor);
	RESOLVE_SIG(g_GameConfig, "CNetworkStringTable_DeleteAllStrings", addresses::CNetworkStringTable_DeleteAllStrings);
	RESOLVE_SIG(g_GameConfig, "CCSPlayer_WeaponServices_EquipWeapon", addresses::CCSPlayer_WeaponServices_EquipWeapon);

	return InitializeBanMap(g_GameConfig);
}

bool addresses::InitializeBanMap(CGameConfig* g_GameConfig)
{
	// This signature directly points to the instruction referencing sm_mapGcBanInformation, and the opcode is 3 bytes so we skip those
	uint8* pAddr = (uint8*)g_GameConfig->ResolveSignature("CCSGameRules__sm_mapGcBanInformation") + 3;

	if (!pAddr)
		return false;

	// Grab the offset as 4 bytes
	uint32 offset = *(uint32*)pAddr;

	// Go to the next instruction, which is what the relative address is based off
	pAddr += 4;

	// Get the real address
	addresses::sm_mapGcBanInformation = (decltype(addresses::sm_mapGcBanInformation))(pAddr + offset);

	if (!addresses::sm_mapGcBanInformation)
		return false;

	Message("Found %s at 0x%p\n", "CCSGameRules__sm_mapGcBanInformation", addresses::sm_mapGcBanInformation);
	return true;
}
