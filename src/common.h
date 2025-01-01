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

#pragma once

#include "platform.h"

#define VPROF_LEVEL 1

#define CS_TEAM_NONE 0
#define CS_TEAM_SPECTATOR 1
#define CS_TEAM_T 2
#define CS_TEAM_CT 3

#define HUD_PRINTNOTIFY 1
#define HUD_PRINTCONSOLE 2
#define HUD_PRINTTALK 3
#define HUD_PRINTCENTER 4

#define MAXPLAYERS 64

#ifdef _WIN32
	#define ROOTBIN "/bin/win64/"
	#define GAMEBIN "/csgo/bin/win64/"
#else
	#define ROOTBIN "/bin/linuxsteamrt64/"
	#define GAMEBIN "/csgo/bin/linuxsteamrt64/"
#endif

void UnlockConVars();
void UnlockConCommands();

void Message(const char*, ...);
void Panic(const char*, ...);

// CONVAR_TODO
// Need to replace with actual cvars once available in SDK
#define FAKE_CVAR(name, description, variable_name, variable_type, variable_type_format, variable_default, protect)     \
	CON_COMMAND_F(name, description, FCVAR_LINKED_CONCOMMAND | FCVAR_SPONLY | (protect ? FCVAR_PROTECTED : FCVAR_NONE)) \
	{                                                                                                                   \
		if (args.ArgC() < 2)                                                                                            \
			Msg("%s " #variable_type_format "\n", args[0], variable_name);                                              \
		else                                                                                                            \
			variable_name = V_StringTo##variable_type(args[1], variable_default);                                       \
	}

#define FAKE_INT_CVAR(name, description, variable_name, variable_default, protect) \
	FAKE_CVAR(name, description, variable_name, Int32, % i, variable_default, protect)

#define FAKE_BOOL_CVAR(name, description, variable_name, variable_default, protect) \
	FAKE_CVAR(name, description, variable_name, Bool, % i, variable_default, protect)

#define FAKE_FLOAT_CVAR(name, description, variable_name, variable_default, protect) \
	FAKE_CVAR(name, description, variable_name, Float32, % f, variable_default, protect)

// assumes std::string variable
#define FAKE_STRING_CVAR(name, description, variable_name, protect)                                                     \
	CON_COMMAND_F(name, description, FCVAR_LINKED_CONCOMMAND | FCVAR_SPONLY | (protect ? FCVAR_PROTECTED : FCVAR_NONE)) \
	{                                                                                                                   \
		if (args.ArgC() < 2)                                                                                            \
			Msg("%s %s\n", args[0], variable_name.c_str());                                                             \
		else                                                                                                            \
			variable_name = args[1];                                                                                    \
	}

#define FAKE_COLOR_CVAR(name, description, variable_name, protect)                                                      \
	CON_COMMAND_F(name, description, FCVAR_LINKED_CONCOMMAND | FCVAR_SPONLY | (protect ? FCVAR_PROTECTED : FCVAR_NONE)) \
	{                                                                                                                   \
		if (args.ArgC() < 2)                                                                                            \
			Msg("%s %i %i %i\n", args[0], variable_name[0], variable_name[1], variable_name[2]);                        \
		else if (args.ArgC() == 2)                                                                                      \
			V_StringToColor(args[1], variable_name);                                                                    \
		else                                                                                                            \
			V_StringToColor(args.ArgS(), variable_name);                                                                \
	}