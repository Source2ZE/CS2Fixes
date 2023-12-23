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
#include "igamesystemfactory.h"

extern CGameConfig *g_GameConfig;

CBaseGameSystemFactory **CBaseGameSystemFactory::sm_pFirst = nullptr;

CResourcePrecacheSystem g_ResourcePrecacheSystem;

// This mess is needed to get the pointer to sm_pFirst so we can insert game systems
bool InitGameSystems()
{
	// Here we go to the 12th byte in IGameSystem::InitAllSystems, which is the start of a 32-bit relative address to sm_pFirst
	uint8 *ptr = (uint8*)g_GameConfig->ResolveSignature("IGameSystem_InitAllSystems") + 11;

	if (!ptr)
	{
		Panic("Failed to init ResourcePrecacheSystem\n");
		return false;
	}

	// Grab the offset
	uint32 offset = *(uint32 *)ptr;

	// Go to the next instruction, which is the starting point of the relative jump
	ptr += 4;

	// Now grab our pointer
	CBaseGameSystemFactory::sm_pFirst = (CBaseGameSystemFactory **)(ptr + offset);

	// And insert the game system(s)
	new CGameSystemStaticFactory<CResourcePrecacheSystem>("ResourcePrecacheSystem", &g_ResourcePrecacheSystem);

	return true;
}

void CResourcePrecacheSystem::BuildGameSessionManifest(const EventBuildGameSessionManifest_t *const ppManifest)
{
	Message("CResourcePrecacheSystem::BuildGameSessionManifest\n");

	IEntityResourceManifest *pResourceManifest = *(IEntityResourceManifest**)ppManifest;

	// This takes any resource type, model or not
	// addresses::PrecacheResource("characters/models/my_character_model.vmdl", pResourceManifest);
}
