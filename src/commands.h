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

#pragma once
#include "entity/ccsplayercontroller.h"
#include "convar.h"

#define COMMAND_PREFIX "c_"
#define CHAT_PREFIX	" \7[CS2Fixes]\1 "

typedef void (*FnChatCommandCallback_t)(const CCommand &args, CCSPlayerController *player);

extern CUtlMap<uint32, FnChatCommandCallback_t> g_CommandList;

void ClientPrintAll(int destination, const char *msg, ...);
void ClientPrint(CBasePlayerController *player, int destination, const char *msg, ...);

// Just a wrapper class so we're able to insert the callback
class CChatCommand
{
public:
	CChatCommand(const char *cmd, FnChatCommandCallback_t callback)
	{
		g_CommandList.Insert(hash_32_fnv1a_const(cmd), callback);
	}
};

struct WeaponMapEntry_t
{
	const char *command;
	const char *szWeaponName;
	int iPrice;
	uint16 iItemDefIndex;
	int maxAmount = 0;
};

void ParseChatCommand(const char *, CCSPlayerController *);

#define CON_COMMAND_CHAT(name, description)																												\
	void name##_callback(const CCommand &args, CCSPlayerController *player);																			\
	static void name##_con_callback(const CCommandContext &context, const CCommand &args)																\
	{																																					\
		CCSPlayerController *pController = nullptr;																										\
		if (context.GetPlayerSlot().Get() != -1)																										\
			pController = (CCSPlayerController *)g_pEntitySystem->GetBaseEntity((CEntityIndex)(context.GetPlayerSlot().Get() + 1));						\
																																						\
		/*Only allow connected players to run chat commands*/																							\
		if (pController && !pController->IsConnected())																									\
			return;																																		\
																																						\
		name##_callback(args, pController);																												\
	}																																					\
	static CChatCommand name##_chat_command(#name, name##_callback);																					\
	static ConCommandRefAbstract name##_ref;																											\
	static ConCommand name##_command(&name##_ref, COMMAND_PREFIX #name, name##_con_callback,															\
									description, FCVAR_CLIENT_CAN_EXECUTE | FCVAR_LINKED_CONCOMMAND);													\
	void name##_callback(const CCommand &args, CCSPlayerController *player)
