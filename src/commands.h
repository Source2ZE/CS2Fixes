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
#include "adminsystem.h"
#include <vector>

#define COMMAND_PREFIX "c_"
#define CHAT_PREFIX	" \7[CS2Fixes]\1 "

typedef void (*FnChatCommandCallback_t)(const CCommand &args, CCSPlayerController *player);

class CChatCommand;

extern CUtlMap<uint32, CChatCommand*> g_CommandList;

extern bool g_bEnableHide;
extern bool g_bEnableStopSound;

void ClientPrintAll(int destination, const char *msg, ...);
void ClientPrint(CBasePlayerController *player, int destination, const char *msg, ...);

// Just a wrapper class so we're able to insert the callback
class CChatCommand
{
public:
	CChatCommand(const char *cmd, FnChatCommandCallback_t callback, const char *description, uint64 flags = ADMFLAG_NONE) :
		m_pfnCallback(callback), m_sName(cmd), m_sDescription(description), m_nFlags(flags)
	{
		g_CommandList.Insert(hash_32_fnv1a_const(cmd), this);
	}

	void operator()(const CCommand &args, CCSPlayerController *player)
	{
		// Only allow connected players to run chat commands
		if (player && !player->IsConnected())
			return;

		// If the command is run from server console, ignore admin flags
		if (player && !CheckCommandAccess(player, m_nFlags))
			return;

		m_pfnCallback(args, player);
	}

	static bool CheckCommandAccess(CBasePlayerController *pPlayer, uint64 flags);

	const char* GetName() { return m_sName.c_str(); }
	const char* GetDescription() { return m_sDescription.c_str(); }
	uint64 GetFlags() { return m_nFlags; }

private:
	FnChatCommandCallback_t m_pfnCallback;
	uint64 m_nFlags;
	std::string m_sName;
	std::string m_sDescription;
};

struct WeaponMapEntry_t
{
	std::vector<std::string> aliases;
	const char* szClassName;
	const char* szWeaponName;
	int iPrice;
	uint16 iItemDefIndex;
	gear_slot_t iGearSlot;
	int maxAmount = 0;
};

void RegisterWeaponCommands();
void ParseChatCommand(const char *, CCSPlayerController *);

#define CON_COMMAND_CHAT_FLAGS(name, description, flags)																						\
	void name##_callback(const CCommand &args, CCSPlayerController *player);																			\
	static CChatCommand name##_chat_command(#name, name##_callback, description, flags);														\
	static void name##_con_callback(const CCommandContext &context, const CCommand &args)																\
	{																																					\
		CCSPlayerController *pController = nullptr;																										\
		if (context.GetPlayerSlot().Get() != -1)																										\
			pController = (CCSPlayerController *)g_pEntitySystem->GetBaseEntity((CEntityIndex)(context.GetPlayerSlot().Get() + 1));						\
																																						\
		name##_chat_command(args, pController);																											\
	}																																					\
	static ConCommandRefAbstract name##_ref;																											\
	static ConCommand name##_command(&name##_ref, COMMAND_PREFIX #name, name##_con_callback,															\
									description, FCVAR_CLIENT_CAN_EXECUTE | FCVAR_LINKED_CONCOMMAND);													\
	void name##_callback(const CCommand &args, CCSPlayerController *player)

#define CON_COMMAND_CHAT(name, description) CON_COMMAND_CHAT_FLAGS(name, description, ADMFLAG_NONE)
