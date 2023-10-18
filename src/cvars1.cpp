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
#include "icvar.h"

#include "tier0/memdbgon.h"

static uint64 g_iFlagsToRemove = (FCVAR_HIDDEN | FCVAR_DEVELOPMENTONLY | FCVAR_MISSING0 | FCVAR_MISSING1 | FCVAR_MISSING2 | FCVAR_MISSING3);

void UnlockConVars()
{
	if (!g_pCVar)
		return;

	int iUnhiddenConVars = 0;

	ConVar *pCvar = nullptr;
	ConVarHandle hCvarHandle;
	hCvarHandle.Set(0);

	// Can't use FindFirst/Next here as it would skip cvars with certain flags, so just loop through the handles
	do
	{
		pCvar = g_pCVar->GetConVar(hCvarHandle);

		hCvarHandle.Set(hCvarHandle.Get() + 1);

		if (!pCvar || !(pCvar->flags & g_iFlagsToRemove))
			continue;

		pCvar->flags &= ~g_iFlagsToRemove;
		iUnhiddenConVars++;
	} while (pCvar);

	Message("Removed hidden flags from %d convars\n", iUnhiddenConVars);
}

void UnlockConCommands()
{
	if (!g_pCVar)
		return;

	ConCommand *say = g_pCVar->GetCommand(g_pCVar->FindCommand("say"));

	int iUnhiddenConCommands = 0;

	ConCommand *pConCommand = nullptr;
	ConCommand *pInvalidCommand = g_pCVar->GetCommand(ConCommandHandle());
	ConCommandHandle hConCommandHandle;
	hConCommandHandle.Set(0);

	do
	{
		pConCommand = g_pCVar->GetCommand(hConCommandHandle);

		hConCommandHandle.Set(hConCommandHandle.Get() + 1);

		if (!pConCommand || pConCommand == pInvalidCommand || !(pConCommand->GetFlags() & g_iFlagsToRemove))
			continue;

		pConCommand->RemoveFlags(g_iFlagsToRemove);
		iUnhiddenConCommands++;
	} while (pConCommand && pConCommand != pInvalidCommand);

	Message("Removed hidden flags from %d commands\n", iUnhiddenConCommands);
}
