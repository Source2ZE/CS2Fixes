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

#include "common.h"
#include "icvar.h"

#include "tier0/memdbgon.h"

static uint64 g_iFlagsToRemove = (FCVAR_HIDDEN | FCVAR_DEVELOPMENTONLY);

static constexpr const char* pUnCheatCvars[] = {"bot_stop", "bot_freeze", "bot_zombie"};
static constexpr const char* pUnCheatCmds[] = {"report_entities", "endround"};

void UnlockConVars()
{
	if (!g_pCVar)
		return;

	int iUnhiddenConVars = 0;

	for (ConVarRefAbstract ref(ConVarRef((uint16)0)); ref.IsValidRef(); ref = ConVarRefAbstract(ConVarRef(ref.GetAccessIndex() + 1)))
	{
		for (int i = 0; i < sizeof(pUnCheatCvars) / sizeof(*pUnCheatCvars); i++)
			if (!V_strcmp(ref.GetName(), pUnCheatCvars[i]))
				ref.RemoveFlags(FCVAR_CHEAT);

		if (!ref.IsFlagSet(g_iFlagsToRemove))
			continue;

		ref.RemoveFlags(g_iFlagsToRemove);
		iUnhiddenConVars++;
	}

	Message("Removed hidden flags from %d convars\n", iUnhiddenConVars);
}

void UnlockConCommands()
{
	if (!g_pCVar)
		return;

	int iUnhiddenConCommands = 0;

	ConCommandData* data = g_pCVar->GetConCommandData(ConCommandRef());
	for (ConCommandRef ref = ConCommandRef((uint16)0); ref.GetRawData() != data; ref = ConCommandRef(ref.GetAccessIndex() + 1))
	{
		for (int i = 0; i < sizeof(pUnCheatCmds) / sizeof(*pUnCheatCmds); i++)
			if (!V_strcmp(ref.GetName(), pUnCheatCmds[i]))
				ref.RemoveFlags(FCVAR_CHEAT);

		if (!ref.IsFlagSet(g_iFlagsToRemove))
			continue;

		ref.RemoveFlags(g_iFlagsToRemove);
		iUnhiddenConCommands++;
	}

	Message("Removed hidden flags from %d commands\n", iUnhiddenConCommands);
}
