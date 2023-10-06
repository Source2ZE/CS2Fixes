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
#include "mempatch.h"
#include "icvar.h"
#include "irecipientfilter.h"
#include "interfaces/cs2_interfaces.h"
#include "entity/ccsplayercontroller.h"
#include "entity/ccsplayerpawn.h"
#include "entity/cbasemodelentity.h"
#include "addresses.h"

#include "tier0/memdbgon.h"

CMemPatch g_CommonPatches[] =
{
	CMemPatch(&modules::server, sigs::MovementUnlock, sigs::Patch_MovementUnlock, "ServerMovementUnlock"),
	CMemPatch(&modules::vscript, sigs::VScriptEnable, sigs::Patch_VScriptEnable, "VScriptEnable"),
};

CMemPatch g_ClientPatches[] =
{
	CMemPatch(&modules::client, sigs::MovementUnlock, sigs::Patch_MovementUnlock, "ClientMovementUnlock"),
};

#ifdef _WIN32
CMemPatch g_ToolsPatches[] =
{
	// Remove some -nocustomermachine checks without needing -nocustomermachine itself as it can break stuff, mainly to enable device selection in compiles
	CMemPatch(&modules::hammer, sigs::HammerNoCustomerMachine, sigs::Patch_HammerNoCustomerMachine, "HammerNoCustomerMachine", 4),
};
#endif

void InitPatches()
{
	for (int i = 0; i < sizeof(g_CommonPatches) / sizeof(*g_CommonPatches); i++)
		g_CommonPatches[i].PerformPatch();

	// Dedicated servers don't load client
	if (!CommandLine()->HasParm("-dedicated"))
	{
		for (int i = 0; i < sizeof(g_ClientPatches) / sizeof(*g_ClientPatches); i++)
			g_ClientPatches[i].PerformPatch();
	}

#ifdef _WIN32
	// None of the tools are loaded without, well, -tools
	if (CommandLine()->HasParm("-tools"))
	{
		for (int i = 0; i < sizeof(g_ToolsPatches) / sizeof(*g_ToolsPatches); i++)
			g_ToolsPatches[i].PerformPatch();
	}
#endif
}

void UndoPatches()
{
	for (int i = 0; i < sizeof(g_CommonPatches) / sizeof(*g_CommonPatches); i++)
		g_CommonPatches[i].UndoPatch();

	if (!CommandLine()->HasParm("-dedicated"))
	{
		for (int i = 0; i < sizeof(g_ClientPatches) / sizeof(*g_ClientPatches); i++)
			g_ClientPatches[i].UndoPatch();
	}

#ifdef _WIN32
	if (CommandLine()->HasParm("-tools"))
	{
		for (int i = 0; i < sizeof(g_ToolsPatches) / sizeof(*g_ToolsPatches); i++)
			g_ToolsPatches[i].UndoPatch();
	}
#endif
}
