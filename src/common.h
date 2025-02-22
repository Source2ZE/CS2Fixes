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