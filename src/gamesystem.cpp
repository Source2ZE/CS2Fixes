/**
 * =============================================================================
 * CS2Fixes
 * Copyright (C) 2023 Source2ZE
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

#include "common.h"
#include "gameconfig.h"
#include "addresses.h"
#include "gamesystem.h"

extern CGameConfig *g_GameConfig;

CBaseGameSystemFactory **CBaseGameSystemFactory::sm_pFirst = nullptr;

CResourcePrecacheSystem g_ResourcePrecacheSystem;
IGameSystemFactory *CResourcePrecacheSystem::sm_Factory = nullptr;

// This mess is needed to get the pointer to sm_pFirst so we can insert game systems
bool InitGameSystems()
{
	// This signature directly points to the instruction referencing sm_pFirst, and the opcode is 3 bytes so we skip those
	uint8 *ptr = (uint8*)g_GameConfig->ResolveSignature("IGameSystem_InitAllSystems_pFirst") + 3;

	if (!ptr)
	{
		Panic("Failed to InitGameSystems, see warnings above.\n");
		return false;
	}

	// Grab the offset as 4 bytes
	uint32 offset = *(uint32*)ptr;

	// Go to the next instruction, which is the starting point of the relative jump
	ptr += 4;

	// Now grab our pointer
	CBaseGameSystemFactory::sm_pFirst = (CBaseGameSystemFactory **)(ptr + offset);

	// And insert the game system(s)
	CResourcePrecacheSystem::sm_Factory = new CGameSystemStaticFactory<CResourcePrecacheSystem>("ResourcePrecacheSystem", &g_ResourcePrecacheSystem);

	return true;
}

GS_EVENT_MEMBER(CResourcePrecacheSystem, BuildGameSessionManifest)
{
	Message("CResourcePrecacheSystem::BuildGameSessionManifest\n");

	IEntityResourceManifest *pResourceManifest = msg->m_pResourceManifest;

	// This takes any resource type, model or not
	// Any resource adding MUST be done here, the resource manifest is not long-lived
	// pResourceManifest->AddResource("characters/models/my_character_model.vmdl");
}
