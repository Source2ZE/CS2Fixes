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

#include "patches.h"
#include "addresses.h"
#include "common.h"
#include "entity/cbasemodelentity.h"
#include "entity/ccsplayercontroller.h"
#include "entity/ccsplayerpawn.h"
#include "icvar.h"
#include "irecipientfilter.h"
#include "mempatch.h"

#include "tier0/memdbgon.h"

extern CGameConfig* g_GameConfig;

CMemPatch g_CommonPatches[] =
	{
		CMemPatch("ServerMovementUnlock", "ServerMovementUnlock"),
		CMemPatch("CheckJumpButtonWater", "FixWaterFloorJump"),
		CMemPatch("WaterLevelGravity", "WaterLevelGravity"),
		CMemPatch("CPhysBox_Use", "CPhysBox_Use"),
		CMemPatch("BotNavIgnore", "BotNavIgnore"),
#ifndef _WIN32
		// Linux checks for the nav mesh in each bot_add command, so we patch 3 times
		CMemPatch("BotNavIgnore", "BotNavIgnore"),
		CMemPatch("BotNavIgnore", "BotNavIgnore"),
#endif
};

CConVar<bool> cs2f_movement_unlocker_enable("cs2f_movement_unlocker_enable", FCVAR_NONE, "Whether to enable movement unlocker", false,
											[](CConVar<bool>* cvar, CSplitScreenSlot slot, const bool* new_val, const bool* old_val) {
												// Movement unlocker is always the first patch
												if (*new_val)
													g_CommonPatches[0].PerformPatch(g_GameConfig);
												else
													g_CommonPatches[0].UndoPatch();
											});

bool InitPatches(CGameConfig* g_GameConfig)
{
	bool success = true;

	// Skip first patch (movement unlocker), it gets patched in convar callback
	for (int i = 1; i < sizeof(g_CommonPatches) / sizeof(*g_CommonPatches); i++)
		if (!g_CommonPatches[i].PerformPatch(g_GameConfig))
			success = false;

	return success;
}

void UndoPatches()
{
	for (int i = 0; i < sizeof(g_CommonPatches) / sizeof(*g_CommonPatches); i++)
		g_CommonPatches[i].UndoPatch();
}
