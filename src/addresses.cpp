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

#include "addresses.h"
#include "gameconfig.h"
#include "utils/module.h"

#include "entityinstance.h"
#include "tier1/strtools.h"
#include "vscript/ivscript.h"

#include "tier0/memdbgon.h"

#define RESOLVE_SIG(name, variable)										 \
	variable = (decltype(variable))g_GameConfig->ResolveSignature(name); \
	if (!variable)														 \
		return false;													 \
	Message("Found %s at 0x%p\n", name, variable);

#define RESOLVE_SF(scriptDesc, funcName, variable)                                                                \
	if (!variable.Initialize(GetVScriptFunction(scriptDesc, funcName)))                                           \
		return false;                                                                                             \
	if (variable.IsVirtual())                                                                                     \
		Message("Found %s::%s at vtable index %i\n", scriptDesc->m_pszClassname, funcName, variable.GetOffset()); \
	else                                                                                                          \
		Message("Found %s::%s at 0x%p\n", scriptDesc->m_pszClassname, funcName, variable.GetPtr());

bool addresses::Initialize()
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

	RESOLVE_SIG("SetGroundEntity", addresses::SetGroundEntity);
	RESOLVE_SIG("CCSPlayerController_SwitchTeam", addresses::CCSPlayerController_SwitchTeam);
	RESOLVE_SIG("CBasePlayerController_SetPawn", addresses::CBasePlayerController_SetPawn);
	RESOLVE_SIG("CBaseModelEntity_SetModel", addresses::CBaseModelEntity_SetModel);
	RESOLVE_SIG("CEntitySystem_AddEntityIOEvent", addresses::CEntitySystem_AddEntityIOEvent);
	RESOLVE_SIG("CEntityInstance_AcceptInput", addresses::CEntityInstance_AcceptInput);
	RESOLVE_SIG("CGameEntitySystem_FindEntityByClassName", addresses::CGameEntitySystem_FindEntityByClassName);
	RESOLVE_SIG("CGameEntitySystem_FindEntityByName", addresses::CGameEntitySystem_FindEntityByName);
	RESOLVE_SIG("CGameRules_TerminateRound", addresses::CGameRules_TerminateRound);
	RESOLVE_SIG("CreateEntityByName", addresses::CreateEntityByName);
	RESOLVE_SIG("DispatchSpawn", addresses::DispatchSpawn);
	RESOLVE_SIG("DispatchParticleEffect", addresses::DispatchParticleEffect);
	RESOLVE_SIG("CBaseEntity_EmitSoundFilter", addresses::CBaseEntity_EmitSoundFilter);
	RESOLVE_SIG("CBaseEntity_SetMoveType", addresses::CBaseEntity_SetMoveType);
	RESOLVE_SIG("CCSPlayer_WeaponServices_EquipWeapon", addresses::CCSPlayer_WeaponServices_EquipWeapon);
	RESOLVE_SIG("GetSpawnGroups", addresses::GetSpawnGroups);
	RESOLVE_SIG("CBasePlayerPawn_SnapViewAngles", addresses::CBasePlayerPawn_SnapViewAngles);
	RESOLVE_SIG("CBaseEntity_TakeDamageOld", addresses::CBaseEntity_TakeDamageOld);

	return InitializeBanMap();
}

bool addresses::InitializeBanMap()
{
	// This signature directly points to the instruction referencing sm_mapGcBanInformation
	uintptr_t pAddr = (uintptr_t)g_GameConfig->ResolveSignature("CCSGameRules__sm_mapGcBanInformation");

	if (!pAddr)
		return false;

	// the opcode is 3 bytes so we skip those
	pAddr += 3;

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

bool addresses::InitializeVScriptFunctions()
{
	void* pCBaseEntityVTable = modules::server->FindVirtualTable("CBaseEntity");
	if (!pCBaseEntityVTable)
	{
		Message("Failed to find CBaseEntity vtable\n");
		return false;
	}

	// GetScriptDesc ignores this, so the vtable pointer is sufficient here.
	ScriptClassDesc_t* pCBaseEntityScriptDesc = reinterpret_cast<ScriptClassDesc_t*>(reinterpret_cast<CEntityInstance*>(&pCBaseEntityVTable)->GetScriptDesc());

	RESOLVE_SF(pCBaseEntityScriptDesc, "SetGravity", SetGravityScale);
	RESOLVE_SF(pCBaseEntityScriptDesc, "SetEntityName", ScriptSetEntityName);
	RESOLVE_SF(pCBaseEntityScriptDesc, "EmitSoundParams", ScriptEmitSoundParams);
	RESOLVE_SF(pCBaseEntityScriptDesc, "SetTeam", ChangeTeam);
	RESOLVE_SF(pCBaseEntityScriptDesc, "IsPlayerPawn", IsPlayerPawn);
	RESOLVE_SF(pCBaseEntityScriptDesc, "IsPlayerController", IsPlayerController);

	return true;
}
