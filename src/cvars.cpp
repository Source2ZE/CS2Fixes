/**
 * =============================================================================
 * CS2Fixes
 * Copyright (C) 2023-2024 Source2ZE
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

static constexpr const char* pUnCheatCvars[] = {"bot_stop", "bot_freeze", "bot_zombie"};
static constexpr const char* pUnCheatCmds[] = {"report_entities", "endround"};

void UnlockConVars()
{
	if (!g_pCVar)
		return;

	int iUnhiddenConVars = 0;

	ConVar* pCvar = nullptr;
	ConVarHandle hCvarHandle;
	hCvarHandle.Set(0);

	// Can't use FindFirst/Next here as it would skip cvars with certain flags, so just loop through the handles
	do
	{
		pCvar = g_pCVar->GetConVar(hCvarHandle);

		hCvarHandle.Set(hCvarHandle.Get() + 1);

		if (!pCvar)
			continue;

		for (int i = 0; i < sizeof(pUnCheatCvars) / sizeof(*pUnCheatCvars); i++)
			if (!V_strcmp(pCvar->m_pszName, pUnCheatCvars[i]))
				pCvar->flags &= ~FCVAR_CHEAT;

		if (!(pCvar->flags & g_iFlagsToRemove))
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

	int iUnhiddenConCommands = 0;

	ConCommand* pConCommand = nullptr;
	ConCommand* pInvalidCommand = g_pCVar->GetCommand(ConCommandHandle());
	ConCommandHandle hConCommandHandle;
	hConCommandHandle.Set(0);

	do
	{
		pConCommand = g_pCVar->GetCommand(hConCommandHandle);

		hConCommandHandle.Set(hConCommandHandle.Get() + 1);

		if (!pConCommand || pConCommand == pInvalidCommand)
			continue;

		for (int i = 0; i < sizeof(pUnCheatCmds) / sizeof(*pUnCheatCmds); i++)
			if (!V_strcmp(pConCommand->GetName(), pUnCheatCmds[i]))
				pConCommand->RemoveFlags(FCVAR_CHEAT);

		if (pConCommand->GetFlags() & g_iFlagsToRemove)
		{
			pConCommand->RemoveFlags(g_iFlagsToRemove);
			iUnhiddenConCommands++;
		}
	} while (pConCommand && pConCommand != pInvalidCommand);

	Message("Removed hidden flags from %d commands\n", iUnhiddenConCommands);
}

CON_COMMAND_F(c_dump_cvars, "dump all cvars", FCVAR_SPONLY | FCVAR_LINKED_CONCOMMAND)
{
	ConVar* pCvar = nullptr;
	ConVarHandle hCvarHandle;
	hCvarHandle.Set(0);

	do
	{
		pCvar = g_pCVar->GetConVar(hCvarHandle);

		hCvarHandle.Set(hCvarHandle.Get() + 1);

		if (pCvar)
			switch (pCvar->m_eVarType)
			{
				case EConVarType_Bool:
					Message("%s : bool : %s\n", pCvar->m_pszName, (bool)pCvar->values ? "true" : "false");
					break;
				case EConVarType_Int16:
					Message("%s : int16 : %i\n", pCvar->m_pszName, *(int16*)&pCvar->values);
					break;
				case EConVarType_Int32:
					Message("%s : int32 : %i\n", pCvar->m_pszName, *(int32*)&pCvar->values);
					break;
				case EConVarType_Int64:
					Message("%s : int64 : %lli\n", pCvar->m_pszName, (int64)pCvar->values);
					break;
				case EConVarType_UInt16:
					Message("%s : uint16 : %i\n", pCvar->m_pszName, *(uint16*)&pCvar->values);
					break;
				case EConVarType_UInt32:
					Message("%s : uint32 : %i\n", pCvar->m_pszName, *(uint32*)&pCvar->values);
					break;
				case EConVarType_UInt64:
					Message("%s : uint64 : %lli\n", pCvar->m_pszName, (uint64)pCvar->values);
					break;
				case EConVarType_Float32:
					Message("%s : float32 : %.2f\n", pCvar->m_pszName, *(float32*)&pCvar->values);
					break;
				case EConVarType_Float64:
					Message("%s : float64 : %.2f\n", pCvar->m_pszName, *(float64*)&pCvar->values);
					break;
				case EConVarType_String:
					Message("%s : string : %s\n", pCvar->m_pszName, (char*)pCvar->values);
					break;

				case EConVarType_Color:
					int color[4];
					V_memcpy(&color, &pCvar->values, sizeof(color));
					Message("%s : color : %.2f %.2f %.2f %.2f\n", pCvar->m_pszName, color[0], color[1], color[2], color[3]);
					break;

				case EConVarType_Vector2:
					float vec2[2];
					V_memcpy(&vec2, &pCvar->values, sizeof(vec2));
					Message("%s : vector2 : %.2f %.2f\n", pCvar->m_pszName, vec2[0], vec2[1]);
					break;

				case EConVarType_Vector3:
					float vec3[3];
					V_memcpy(&vec3, &pCvar->values, sizeof(vec3));
					Message("%s : vector3 : %.2f %.2f %.2f\n", pCvar->m_pszName, vec3[0], vec3[1], vec3[2]);
					break;

				case EConVarType_Vector4:
					float vec4[4];
					V_memcpy(&vec4, &pCvar->values, sizeof(vec4));
					Message("%s : vector4 : %.2f %.2f %.2f %.2f\n", pCvar->m_pszName, vec4[0], vec4[1], vec4[2], vec4[3]);
					break;

				case EConVarType_Qangle:
					float angle[3];
					V_memcpy(&vec3, &pCvar->values, sizeof(angle));
					Message("%s : qangle : %.2f %.2f %.2f\n", pCvar->m_pszName, angle[0], angle[1], angle[2]);
					break;

				default:
					Message("%s : unknown type : %p\n", pCvar->m_pszName, (void*)pCvar->values);
					break;
			};

	} while (pCvar);
}
