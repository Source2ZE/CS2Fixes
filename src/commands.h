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
#include "adminsystem.h"
#include "convar.h"
#include "entity/ccsplayercontroller.h"
#include "leader.h"
#include <vector>

class CChatCommand;

extern CConVar<bool> g_cvarEnableCommands;
extern CConVar<bool> g_cvarEnableAdminCommands;
extern std::map<uint32, std::shared_ptr<CChatCommand>>& CommandList();
extern CConVar<bool> g_cvarEnableStopSound;
extern CConVar<bool> g_cvarEnableNoShake;
extern CConVar<float> g_cvarMaxShakeAmp;
extern CConVar<bool> g_cvarEnableHide;

#define CMDFLAG_NONE (0)
#define CMDFLAG_NOHELP (1 << 0) // Don't show in !help menu

#define COMMAND_PREFIX "c_"
#define CHAT_PREFIX " \7[CS2Fixes]\1 "

typedef void (*FnChatCommandCallback_t)(const CCommand& args, CCSPlayerController* player);

void ClientPrintAll(int destination, const char* msg, ...);
void ClientPrint(CCSPlayerController* player, int destination, const char* msg, ...);

// Just a wrapper class so we're able to insert the callback
class CChatCommand
{
public:
	CChatCommand(const char* cmd, FnChatCommandCallback_t callback, const char* description, uint64 adminFlags, uint64 cmdFlags) :
		m_pfnCallback(callback), m_sName(cmd), m_sDescription(description), m_nAdminFlags(adminFlags), m_nCmdFlags(cmdFlags)
	{}

	static std::shared_ptr<CChatCommand> Create(const char* cmd, FnChatCommandCallback_t callback, const char* description, uint64 adminFlags = ADMFLAG_NONE, uint64 cmdFlags = CMDFLAG_NONE)
	{
		auto pCommand = std::make_shared<CChatCommand>(cmd, callback, description, adminFlags, cmdFlags);

		CommandList().insert(std::make_pair(hash_32_fnv1a_const(cmd), pCommand));
		return pCommand;
	}

	void operator()(const CCommand& args, CCSPlayerController* player)
	{
		// Server disabled ALL chat commands
		if (!g_cvarEnableCommands.Get())
			return;

		// Server disabled admin chat commands
		if (!g_cvarEnableAdminCommands.Get() && m_nAdminFlags > ADMFLAG_NONE)
			return;

		// Only allow connected players to run chat commands
		if (player && !player->IsConnected())
			return;

		// If the command is run from server console, ignore admin flags
		if (player && !CheckCommandAccess(player, m_nAdminFlags))
			return;

		m_pfnCallback(args, player);
	}

	static bool CheckCommandAccess(CCSPlayerController* pPlayer, uint64 flags);

	const char* GetName() { return m_sName.c_str(); }
	const char* GetDescription() { return m_sDescription.c_str(); }
	uint64 GetAdminFlags() { return m_nAdminFlags; }
	bool IsCommandFlagSet(uint64 iFlag)
	{
		return !iFlag || (m_nCmdFlags & iFlag);
	}

private:
	FnChatCommandCallback_t m_pfnCallback;
	std::string m_sName;
	std::string m_sDescription;
	uint64 m_nAdminFlags;
	uint64 m_nCmdFlags;
};

void RegisterWeaponCommands();
void ParseChatCommand(const char*, CCSPlayerController*);

#define CON_COMMAND_CHAT_FLAGS(name, description, flags)                                                                               \
	void name##_callback(const CCommand& args, CCSPlayerController* player);                                                           \
	static auto name##_chat_command = CChatCommand::Create(#name, name##_callback, description, flags);                                \
	static void name##_con_callback(const CCommandContext& context, const CCommand& args)                                              \
	{                                                                                                                                  \
		CCSPlayerController* pController = nullptr;                                                                                    \
		if (context.GetPlayerSlot().Get() != -1)                                                                                       \
			pController = (CCSPlayerController*)g_pEntitySystem->GetEntityInstance((CEntityIndex)(context.GetPlayerSlot().Get() + 1)); \
                                                                                                                                       \
		(*name##_chat_command)(args, pController);                                                                                     \
	}                                                                                                                                  \
	static ConCommand name##_command(COMMAND_PREFIX #name, name##_con_callback,                                                        \
									 description, FCVAR_CLIENT_CAN_EXECUTE | FCVAR_LINKED_CONCOMMAND);                                 \
	void name##_callback(const CCommand& args, CCSPlayerController* player)

#define CON_COMMAND_CHAT(name, description) CON_COMMAND_CHAT_FLAGS(name, description, ADMFLAG_NONE)
#define CON_COMMAND_CHAT_LEADER(name, description) CON_COMMAND_CHAT_FLAGS(name, description, FLAG_LEADER)

class CLoggingListener : public ILoggingListener
{
public:
	void SetPlayer(CCSPlayerController* pController) { m_pController = pController; }

	void Log(const LoggingContext_t* pContext, const tchar* pMessage) override
	{
		if (m_pController)
			ClientPrint(m_pController, HUD_PRINTCONSOLE, pMessage);
	}

private:
	CCSPlayerController* m_pController = nullptr;
};

extern CLoggingListener g_LoggingListener;
