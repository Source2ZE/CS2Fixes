/**
 * =============================================================================
 * CS2Fixes
 * Copyright (C) 2023-2026 Source2ZE
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

#include "adminsystem.h"
#include "KeyValues.h"
#include "commands.h"
#include "ctimer.h"
#include "detours.h"
#include "discord.h"
#include "entity/cbaseentity.h"
#include "entity/cgamerules.h"
#include "entity/cparticlesystem.h"
#include "entwatch.h"
#include "filesystem.h"
#include "gamesystem.h"
#include "hud_manager.h"
#include "icvar.h"
#include "interfaces/interfaces.h"
#include "map_votes.h"
#include "playermanager.h"
#include "translations.h"
#include "utils/entity.h"
#include "votemanager.h"
#include <fstream>
#include <vector>

#undef snprintf
#include "vendor/nlohmann/json.hpp"

CAdminSystem* g_pAdminSystem = nullptr;

void ParseInfraction(const CCommand& args, CCSPlayerController* pAdmin, bool bAdding, CInfractionBase::EInfractionType infType);
const char* GetActionPhrase(CInfractionBase::EInfractionType infType, GrammarTense iTense, bool bAdding);

void PrintMultiAdminAction(ETargetType nType, const char* pszAdminName, const char* pszAction, const char* pszAction2 = "", const char* prefix = CHAT_PREFIX)
{
	switch (nType)
	{
		case ETargetType::ALL:
			ClientPrintAllT(HUD_PRINTTALK, prefix, "{Admin.ActionEveryone}", pszAdminName, pszAction);
			break;
		case ETargetType::SPECTATOR:
			ClientPrintAllT(HUD_PRINTTALK, prefix, "{Admin.ActionSpectators}", pszAdminName, pszAction);
			break;
		case ETargetType::T:
			ClientPrintAllT(HUD_PRINTTALK, prefix, "{Admin.ActionTerrorists}", pszAdminName, pszAction);
			break;
		case ETargetType::CT:
			ClientPrintAllT(HUD_PRINTTALK, prefix, "{Admin.ActionCounterTerrorists}", pszAdminName, pszAction);
			break;
		case ETargetType::DEAD:
			ClientPrintAllT(HUD_PRINTTALK, prefix, "{Admin.ActionDeadPlayers}", pszAdminName, pszAction);
			break;
		case ETargetType::ALIVE:
			ClientPrintAllT(HUD_PRINTTALK, prefix, "{Admin.ActionAlivePlayers}", pszAdminName, pszAction);
			break;
		case ETargetType::BOT:
			ClientPrintAllT(HUD_PRINTTALK, prefix, "{Admin.ActionBots}", pszAdminName, pszAction);
			break;
		case ETargetType::HUMAN:
			ClientPrintAllT(HUD_PRINTTALK, prefix, "{Admin.ActionHumans}", pszAdminName, pszAction);
			break;
		case ETargetType::ALL_BUT_SELF:
			ClientPrintAllT(HUD_PRINTTALK, prefix, "{Admin.ActionEveryoneExceptSelf}", pszAdminName, pszAction, pszAdminName, pszAction2);
			break;
		case ETargetType::ALL_BUT_RANDOM:
			ClientPrintAllT(HUD_PRINTTALK, prefix, "{Admin.ActionEveryoneExceptRandom}", pszAdminName, pszAction);
			break;
		case ETargetType::ALL_BUT_RANDOM_T:
			ClientPrintAllT(HUD_PRINTTALK, prefix, "{Admin.ActionEveryoneExceptRandomT}", pszAdminName, pszAction);
			break;
		case ETargetType::ALL_BUT_RANDOM_CT:
			ClientPrintAllT(HUD_PRINTTALK, prefix, "{Admin.ActionEveryoneExceptRandomCT}", pszAdminName, pszAction);
			break;
		case ETargetType::ALL_BUT_RANDOM_SPEC:
			ClientPrintAllT(HUD_PRINTTALK, prefix, "{Admin.ActionEveryoneExceptRandomSpec}", pszAdminName, pszAction);
			break;
		case ETargetType::ALL_BUT_AIM:
			ClientPrintAllT(HUD_PRINTTALK, prefix, "{Admin.ActionEveryoneExceptAim}", pszAdminName, pszAction);
			break;
		case ETargetType::ALL_BUT_SPECTATOR:
			ClientPrintAllT(HUD_PRINTTALK, prefix, "{Admin.ActionNonSpectators}", pszAdminName, pszAction);
			break;
		case ETargetType::ALL_BUT_T:
			ClientPrintAllT(HUD_PRINTTALK, prefix, "{Admin.ActionNonTerrorists}", pszAdminName, pszAction);
			break;
		case ETargetType::ALL_BUT_CT:
			ClientPrintAllT(HUD_PRINTTALK, prefix, "{Admin.ActionNonCounterTerrorists}", pszAdminName, pszAction);
			break;
	}
}

CON_COMMAND_CHAT_FLAGS(debugclientlist, "- Debug client list", ADMFLAG_ROOT)
{
	if (!GetClientList())
	{
		ClientPrintT(player, HUD_PRINTCONSOLE, "{Admin.ClientListNull}");
		return;
	}

	for (int i = 0; i < GetClientList()->Count(); i++)
	{
		CServerSideClient* pClient = (*GetClientList())[i];

		if (!pClient)
			continue;

		ClientPrint(player, HUD_PRINTCONSOLE, "slot: %i address: %s signonstate: %i spawngroups: %i name: %s", pClient->GetPlayerSlot().Get(), pClient->GetRemoteAddress()->ToString(), pClient->GetSignonState(), pClient->m_vecLoadedSpawnGroups.Count(), pClient->GetClientName());
	}
}

CON_COMMAND_F(c_reload_admins, "- Reload admin config", FCVAR_SPONLY | FCVAR_LINKED_CONCOMMAND)
{
	if (!g_pAdminSystem->LoadAdmins() || !GetGlobals())
		return;

	for (int i = 0; i < GetGlobals()->maxClients; i++)
	{
		ZEPlayer* pPlayer = g_playerManager->GetPlayer(i);

		if (!pPlayer || pPlayer->IsFakeClient() || !pPlayer->IsAuthenticated())
			continue;

		pPlayer->CheckAdmin();
	}

	Message("Admins reloaded\n");
}

CON_COMMAND_F(c_reload_infractions, "- Reload infractions file", FCVAR_SPONLY | FCVAR_LINKED_CONCOMMAND)
{
	if (!g_pAdminSystem->LoadInfractions() || !GetGlobals())
		return;

	for (int i = 0; i < GetGlobals()->maxClients; i++)
	{
		ZEPlayer* pPlayer = g_playerManager->GetPlayer(i);

		if (!pPlayer || pPlayer->IsFakeClient())
			continue;

		pPlayer->CheckInfractions();
	}

	Message("Infractions reloaded\n");
}

CON_COMMAND_CHAT_FLAGS(ban, "<name> <minutes|0 (permament)> - Ban a player", ADMFLAG_BAN)
{
	ParseInfraction(args, player, true, CInfractionBase::EInfractionType::Ban);
}

CON_COMMAND_CHAT_FLAGS(unban, "<steamid64> - Unban a player. Takes decimal STEAMID64", ADMFLAG_BAN)
{
	if (args.ArgC() < 2)
	{
		ClientPrintT(player, HUD_PRINTTALK, CHAT_PREFIX "{Admin.UnbanUsage}");
		return;
	}

	uint64 iTargetSteamId64 = V_StringToUint64(args[1], 0);

	bool bResult = g_pAdminSystem->FindAndRemoveInfractionSteamId64(iTargetSteamId64, CInfractionBase::Ban);

	if (!bResult)
	{
		ClientPrintT(player, HUD_PRINTTALK, CHAT_PREFIX "{Admin.UnbanNotFound}", iTargetSteamId64);
		return;
	}

	g_pAdminSystem->SaveInfractions();

	// no need to broadcast this
	ClientPrintT(player, HUD_PRINTTALK, CHAT_PREFIX "{Admin.UnbanSuccess}", iTargetSteamId64);
}

CON_COMMAND_CHAT_FLAGS(mute, "<name> <duration|0 (permament)> - Mute a player", ADMFLAG_CHAT)
{
	ParseInfraction(args, player, true, CInfractionBase::EInfractionType::Mute);
}

CON_COMMAND_CHAT_FLAGS(unmute, "<name> - Unmute a player", ADMFLAG_CHAT)
{
	ParseInfraction(args, player, false, CInfractionBase::EInfractionType::Mute);
}

CON_COMMAND_CHAT_FLAGS(gag, "<name> <duration|0 (permanent)> - Gag a player", ADMFLAG_CHAT)
{
	ParseInfraction(args, player, true, CInfractionBase::EInfractionType::Gag);
}

CON_COMMAND_CHAT_FLAGS(ungag, "<name> - Ungag a player", ADMFLAG_CHAT)
{
	ParseInfraction(args, player, false, CInfractionBase::EInfractionType::Gag);
}

CON_COMMAND_CHAT_FLAGS(eban, "<name> <duration|0 (permanent)> - Ban a player from picking up items", ADMFLAG_BAN)
{
	if (!g_cvarEnableEntWatch.Get())
		return;

	ParseInfraction(args, player, true, CInfractionBase::EInfractionType::Eban);
}

CON_COMMAND_CHAT_FLAGS(eunban, "<name> - Unban a player from picking up items", ADMFLAG_BAN)
{
	if (!g_cvarEnableEntWatch.Get())
		return;

	ParseInfraction(args, player, false, CInfractionBase::EInfractionType::Eban);
}

CON_COMMAND_CHAT_FLAGS(kick, "<name> - Kick a player", ADMFLAG_KICK)
{
	if (args.ArgC() < 2)
	{
		ClientPrintT(player, HUD_PRINTTALK, CHAT_PREFIX "{Admin.KickUsage}");
		return;
	}

	int iNumClients = 0;
	int pSlots[MAXPLAYERS];
	ETargetType nType;

	if (!g_playerManager->CanTargetPlayers(player, args[1], iNumClients, pSlots, NO_TARGET_BLOCKS, nType))
		return;

	const char* pszCommandPlayerName = player ? player->GetPlayerName() : CONSOLE_NAME;

	for (int i = 0; i < iNumClients; i++)
	{
		CCSPlayerController* pTarget = CCSPlayerController::FromSlot(pSlots[i]);
		ZEPlayer* pTargetPlayer = pTarget->GetZEPlayer();

		g_pEngineServer2->DisconnectClient(pTargetPlayer->GetPlayerSlot(), NETWORK_DISCONNECT_KICKED, "Kicked by an admin");

		if (iNumClients == 1)
			ClientPrintAllT(HUD_PRINTTALK, CHAT_PREFIX "{Admin.ActionKicked}", pszCommandPlayerName, pTarget->GetPlayerName());
	}
	if (iNumClients > 1)
		PrintMultiAdminAction(nType, pszCommandPlayerName, "kicked");
}

CON_COMMAND_CHAT_FLAGS(slay, "<name> - Slay a player", ADMFLAG_SLAY)
{
	if (args.ArgC() < 2)
	{
		ClientPrintT(player, HUD_PRINTTALK, CHAT_PREFIX "{Admin.SlayUsage}");
		return;
	}

	int iNumClients = 0;
	int pSlots[MAXPLAYERS];
	ETargetType nType;

	if (!g_playerManager->CanTargetPlayers(player, args[1], iNumClients, pSlots, NO_DEAD, nType))
		return;

	const char* pszCommandPlayerName = player ? player->GetPlayerName() : CONSOLE_NAME;

	for (int i = 0; i < iNumClients; i++)
	{
		CCSPlayerController* pTarget = CCSPlayerController::FromSlot(pSlots[i]);
		pTarget->GetPawn()->CommitSuicide(false, true);

		if (iNumClients == 1)
			ClientPrintAllT(HUD_PRINTTALK, CHAT_PREFIX "{Admin.ActionSlayed}", pszCommandPlayerName, pTarget->GetPlayerName());
	}

	if (iNumClients > 1)
		PrintMultiAdminAction(nType, pszCommandPlayerName, "slayed");
}

CON_COMMAND_CHAT_FLAGS(slap, "<name> [damage] - Slap a player", ADMFLAG_SLAY)
{
	if (args.ArgC() < 2)
	{
		ClientPrintT(player, HUD_PRINTTALK, CHAT_PREFIX "{Admin.SlapUsage}");
		return;
	}

	int iNumClients = 0;
	int pSlots[MAXPLAYERS];
	ETargetType nType;

	if (!g_playerManager->CanTargetPlayers(player, args[1], iNumClients, pSlots, NO_DEAD, nType))
		return;

	const char* pszCommandPlayerName = player ? player->GetPlayerName() : CONSOLE_NAME;

	for (int i = 0; i < iNumClients; i++)
	{
		CBasePlayerController* pTarget = (CBasePlayerController*)g_pEntitySystem->GetEntityInstance((CEntityIndex)(pSlots[i] + 1));
		CBasePlayerPawn* pPawn = pTarget->m_hPawn();

		if (!pPawn)
			continue;

		// Taken directly from sourcemod
		Vector velocity = pPawn->m_vecAbsVelocity;
		velocity.x += ((rand() % 180) + 50) * (((rand() % 2) == 1) ? -1 : 1);
		velocity.y += ((rand() % 180) + 50) * (((rand() % 2) == 1) ? -1 : 1);
		velocity.z += rand() % 200 + 100;
		pPawn->SetAbsVelocity(velocity);

		float flDamage = V_StringToFloat32(args[2], 0);

		if (flDamage > 0)
		{
			// Default to the world
			CBaseEntity* pAttacker = (CBaseEntity*)g_pEntitySystem->GetEntityInstance(CEntityIndex(0));

			if (player)
				pAttacker = player->GetPlayerPawn();

			CTakeDamageInfo info(pAttacker, pAttacker, nullptr, flDamage, DMG_GENERIC);
			pPawn->TakeDamage(info);
		}

		if (iNumClients == 1)
			ClientPrintAllT(HUD_PRINTTALK, CHAT_PREFIX "{Admin.ActionSlapped}", pszCommandPlayerName, pTarget->GetPlayerName());
	}

	if (iNumClients > 1)
		PrintMultiAdminAction(nType, pszCommandPlayerName, "slapped");
}

CON_COMMAND_CHAT_FLAGS(goto, "<name> - Teleport to a player", ADMFLAG_SLAY)
{
	// Only players can use this command at all
	if (!player)
	{
		ClientPrintT(player, HUD_PRINTTALK, CHAT_PREFIX "{General.NoServerConsole}");
		return;
	}

	if (args.ArgC() < 2)
	{
		ClientPrintT(player, HUD_PRINTTALK, CHAT_PREFIX "{Admin.GotoUsage}");
		return;
	}

	int iNumClients = 0;
	int pSlots[MAXPLAYERS];

	if (!g_playerManager->CanTargetPlayers(player, args[1], iNumClients, pSlots, NO_SELF | NO_MULTIPLE | NO_DEAD | NO_IMMUNITY))
		return;

	CCSPlayerController* pTarget = CCSPlayerController::FromSlot(pSlots[0]);

	Vector newOrigin = pTarget->GetPawn()->GetAbsOrigin();

	player->GetPawn()->Teleport(&newOrigin, nullptr, nullptr);

	ClientPrintAllT(HUD_PRINTTALK, CHAT_PREFIX "{Admin.ActionTeleportedTo}", player->GetPlayerName(), pTarget->GetPlayerName());
}

CON_COMMAND_CHAT_FLAGS(bring, "<name> - Bring a player", ADMFLAG_SLAY)
{
	if (!player)
	{
		ClientPrintT(player, HUD_PRINTCONSOLE, CHAT_PREFIX "{General.NoServerConsole}");
		return;
	}

	if (args.ArgC() < 2)
	{
		ClientPrintT(player, HUD_PRINTTALK, CHAT_PREFIX "{Admin.BringUsage}");
		return;
	}

	int iNumClients = 0;
	int pSlots[MAXPLAYERS];
	ETargetType nType;

	if (!g_playerManager->CanTargetPlayers(player, args[1], iNumClients, pSlots, NO_SELF | NO_DEAD, nType))
		return;

	for (int i = 0; i < iNumClients; i++)
	{
		CCSPlayerController* pTarget = CCSPlayerController::FromSlot(pSlots[i]);

		Vector newOrigin = player->GetPawn()->GetAbsOrigin();

		pTarget->GetPawn()->Teleport(&newOrigin, nullptr, nullptr);

		if (iNumClients == 1)
			ClientPrintAllT(HUD_PRINTTALK, CHAT_PREFIX "{Admin.ActionBrought}", player->GetPlayerName(), pTarget->GetPlayerName());
	}

	if (iNumClients > 1)
		PrintMultiAdminAction(nType, player->GetPlayerName(), "brought");
}

CON_COMMAND_CHAT_FLAGS(noclip, "[name] - Toggle noclip on a player", ADMFLAG_CHEATS)
{
	int iNumClients = 0;
	int pSlots[MAXPLAYERS];

	if (!g_playerManager->CanTargetPlayers(player, args.ArgC() < 2 ? "@me" : args[1], iNumClients, pSlots, NO_MULTIPLE | NO_DEAD | NO_SPECTATOR))
		return;

	CCSPlayerController* pTarget = CCSPlayerController::FromSlot(pSlots[0]);
	CBasePlayerPawn* pPawn = pTarget->m_hPawn();
	const char* pszCommandPlayerName = player ? player->GetPlayerName() : CONSOLE_NAME;

	if (!pPawn)
		return;

	if (pPawn->m_nActualMoveType() == MOVETYPE_NOCLIP)
	{
		pPawn->SetMoveType(MOVETYPE_WALK);
		ClientPrintAllT(HUD_PRINTTALK, CHAT_PREFIX "{Admin.ActionNoclipDisabled}", pszCommandPlayerName, pTarget->GetPlayerName());
	}
	else
	{
		pPawn->SetMoveType(MOVETYPE_NOCLIP);
		ClientPrintAllT(HUD_PRINTTALK, CHAT_PREFIX "{Admin.ActionNoclipEnabled}", pszCommandPlayerName, pTarget->GetPlayerName());
	}
}

CON_COMMAND_CHAT_FLAGS(reload_discord_bots, "- Reload discord bot config", ADMFLAG_ROOT)
{
	g_pDiscordBotManager->LoadDiscordBotsConfig();
	Message("Discord bot config reloaded\n");
}

CON_COMMAND_CHAT_FLAGS(entfire, "<name> <input> [parameter] - Fire outputs at entities", ADMFLAG_RCON)
{
	if (args.ArgC() < 3)
	{
		ClientPrintT(player, HUD_PRINTTALK, CHAT_PREFIX "{Admin.EntfireUsage}");
		return;
	}

	int iFoundEnts = 0;

	CBaseEntity* pTarget = nullptr;

	// The idea here is to only use one of the targeting modes at once, prioritizing !picker then targetname/!self then classname
	// Try picker first, FindEntityByName can also take !picker but it always uses player 0 so we have to do this ourselves
	if (player && !V_strcmp("!picker", args[1]))
	{
		pTarget = UTIL_FindPickerEntity(player);

		if (pTarget)
		{
			pTarget->AcceptInput(args[2], args[3], player, player);
			iFoundEnts++;
		}
	}

	// !self would resolve to the player controller, so here's a convenient alias to get the pawn instead
	if (player && !V_strcmp("!selfpawn", args[1]))
	{
		pTarget = player->GetPawn();

		if (pTarget)
		{
			pTarget->AcceptInput(args[2], args[3], player, player);
			iFoundEnts++;
		}
	}

	if (!iFoundEnts)
	{
		while ((pTarget = UTIL_FindEntityByName(pTarget, args[1], player)))
		{
			pTarget->AcceptInput(args[2], args[3], player, player);
			iFoundEnts++;
		}
	}

	if (!iFoundEnts)
	{
		while ((pTarget = UTIL_FindEntityByClassname(pTarget, args[1])))
		{
			pTarget->AcceptInput(args[2], args[3], player, player);
			iFoundEnts++;
		}
	}

	if (!iFoundEnts)
		ClientPrintT(player, HUD_PRINTTALK, CHAT_PREFIX "{Admin.EntfireTargetNotFound}");
	else
		ClientPrintT(player, HUD_PRINTTALK, CHAT_PREFIX "{Admin.EntfireSuccess}", iFoundEnts);
}

CON_COMMAND_CHAT_FLAGS(entfirepawn, "<name> <input> [parameter] - Fire outputs at player pawns", ADMFLAG_RCON)
{
	if (args.ArgC() < 3)
	{
		ClientPrintT(player, HUD_PRINTTALK, CHAT_PREFIX "{Admin.EntfirepawnUsage}");
		return;
	}

	int iNumClients = 0;
	int pSlots[MAXPLAYERS];
	ETargetType nType;

	if (!g_playerManager->CanTargetPlayers(player, args[1], iNumClients, pSlots, NO_TARGET_BLOCKS, nType))
		return;

	int iFoundEnts = 0;

	for (int i = 0; i < iNumClients; i++)
	{
		CCSPlayerController* pTarget = CCSPlayerController::FromSlot(pSlots[i]);

		if (!pTarget->GetPawn())
			continue;

		pTarget->GetPawn()->AcceptInput(args[2], args[3], player, player);
		iFoundEnts++;
	}

	ClientPrintT(player, HUD_PRINTTALK, CHAT_PREFIX "{Admin.EntfirepawnSuccess}", iFoundEnts);
}

CON_COMMAND_CHAT_FLAGS(entfirecontroller, "<name> <input> [parameter] - Fire outputs at player controllers", ADMFLAG_RCON)
{
	if (args.ArgC() < 3)
	{
		ClientPrintT(player, HUD_PRINTTALK, CHAT_PREFIX "{Admin.EntfirecontrollerUsage}");
		return;
	}

	int iNumClients = 0;
	int pSlots[MAXPLAYERS];
	ETargetType nType;

	if (!g_playerManager->CanTargetPlayers(player, args[1], iNumClients, pSlots, NO_TARGET_BLOCKS, nType))
		return;

	int iFoundEnts = 0;

	for (int i = 0; i < iNumClients; i++)
	{
		CCSPlayerController* pTarget = CCSPlayerController::FromSlot(pSlots[i]);
		pTarget->AcceptInput(args[2], args[3], player, player);
		iFoundEnts++;
	}

	ClientPrintT(player, HUD_PRINTTALK, CHAT_PREFIX "{Admin.EntfirecontrollerSuccess}", iFoundEnts);
}

CON_COMMAND_CHAT_FLAGS(hsay, "<message> - Say something as a hud hint", ADMFLAG_CHAT)
{
	if (args.ArgC() < 2)
	{
		ClientPrintT(player, HUD_PRINTTALK, CHAT_PREFIX "{Admin.HsayUsage}");
		return;
	}

	SendHudMessageAll(
		10, EHudPriority::AdminHSay, "<span class='fontSize-l'><span color='#FFFFFF'>ADMIN: </span><span color='#D11313'>%s</span></span>",
		EscapeHTMLSpecialCharacters(args.ArgS()).c_str());
}

CLoggingListener g_LoggingListener;

CON_COMMAND_CHAT_FLAGS(rcon, "<command> - Send a command to server console", ADMFLAG_RCON)
{
	if (!player)
	{
		ClientPrintT(player, HUD_PRINTCONSOLE, CHAT_PREFIX "{Admin.AlreadyServerConsole}");
		return;
	}

	if (args.ArgC() < 2)
	{
		ClientPrintT(player, HUD_PRINTTALK, CHAT_PREFIX "{Admin.RconUsage}");
		return;
	}

	// Normally this should be done on plugin init, but for whatever reason "log_flags <channel> +donotecho" crashes AFTER this
	ExecuteOnce(
		LoggingSystem_RegisterBackdoorLoggingListener(&g_LoggingListener);
		LoggingSystem_EnableBackdoorLoggingListeners(true););

	// We don't have the equivalent of ServerExecute (to immediately execute commands) in source2, so manually find and execute the command

	ConCommandRef cmdRef(args[1], true);

	if (cmdRef.IsValidRef())
	{
		// Some commands are blocked by UTIL_IsCommandIssuedByServerAdmin which only allows slot -1 on dedicated servers
		CCommandContext context(CT_FIRST_SPLITSCREEN_CLIENT, -1);

		CCommand newArgs;
		newArgs.Tokenize(args.ArgS());

		ClientPrintT(player, HUD_PRINTTALK, CHAT_PREFIX "{Admin.RconExecuted}");

		g_LoggingListener.SetPlayer(player);
		cmdRef.Dispatch(context, newArgs);
		g_LoggingListener.SetPlayer(nullptr);

		return;
	}

	ConVarRefAbstract cvarRef(args[1], true);

	if (cvarRef.IsValidRef())
	{
		ClientPrintT(player, HUD_PRINTTALK, CHAT_PREFIX "{Admin.RconExecuted}");

		if (args.ArgC() == 2)
		{
			g_LoggingListener.SetPlayer(player);
			ConVar_PrintDescription(&cvarRef);
			g_LoggingListener.SetPlayer(nullptr);

			return;
		}

		CCommand newArgs;
		newArgs.Tokenize(args.ArgS());

		g_LoggingListener.SetPlayer(player);
		cvarRef.SetString(newArgs.ArgS());
		g_LoggingListener.SetPlayer(nullptr);

		return;
	}

	// Just in case it's not an actual ConCommand (for example an alias)
	g_pEngineServer2->ServerCommand(args.ArgS());

	ClientPrintT(player, HUD_PRINTTALK, CHAT_PREFIX "{Admin.RconExecutedSimple}");
}

CON_COMMAND_CHAT_FLAGS(extend, "<minutes> - Extend current map (negative value reduces map duration)", ADMFLAG_CHANGEMAP)
{
	if (args.ArgC() < 2)
	{
		ClientPrintT(player, HUD_PRINTTALK, CHAT_PREFIX "{Admin.ExtendUsage}");
		return;
	}

	int iExtendTime = V_StringToInt32(args[1], 0);

	if (iExtendTime == 0)
	{
		ClientPrintT(player, HUD_PRINTTALK, CHAT_PREFIX "{Admin.ExtendNoChange}");
		return;
	}

	g_pVoteManager->ExtendMap(iExtendTime);

	const char* pszCommandPlayerName = player ? player->GetPlayerName() : CONSOLE_NAME;

	if (iExtendTime < 0)
		ClientPrintAllT(HUD_PRINTTALK, CHAT_PREFIX "{Admin.ExtendShortened}", pszCommandPlayerName, iExtendTime * -1);
	else
		ClientPrintAllT(HUD_PRINTTALK, CHAT_PREFIX "{Admin.ExtendExtended}", pszCommandPlayerName, iExtendTime);
}

CON_COMMAND_CHAT_FLAGS(pm, "<name> <message> - Private message a player. This will also show to all online admins", ADMFLAG_GENERIC)
{
	if (!GetGlobals())
		return;

	if (args.ArgC() < 3)
	{
		ClientPrintT(player, HUD_PRINTTALK, CHAT_PREFIX "{Admin.PMUsage}");
		return;
	}

	if (player)
	{
		ZEPlayer* ply = player->GetZEPlayer();
		if (!ply)
			return;
		if (ply->IsGagged())
		{
			ClientPrintT(player, HUD_PRINTTALK, CHAT_PREFIX "{Admin.PMGagged}");
			return;
		}
	}

	int iNumClients = 0;
	int pSlots[MAXPLAYERS];
	ETargetType nType;

	if (!g_playerManager->CanTargetPlayers(player, args[1], iNumClients, pSlots, NO_MULTIPLE | NO_IMMUNITY | NO_BOT, nType))
		return;

	CCSPlayerController* pTarget = CCSPlayerController::FromSlot(pSlots[0]);
	ZEPlayer* pTargetPlayer = pTarget->GetZEPlayer();

	std::string strMessage = GetReason(args, 1, false);

	const char* pszName = player ? player->GetPlayerName() : CONSOLE_NAME;

	if (player == pTarget)
	{
		// Player is PMing themselves (bind to display message in chat probably), so no need to echo to all admins
		ClientPrintT(player, HUD_PRINTTALK, "{Admin.PMToSelf}", pszName, strMessage.c_str());
		return;
	}

	for (int i = 0; i < GetGlobals()->maxClients; i++)
	{
		ZEPlayer* pPlayer = g_playerManager->GetPlayer(i);

		if (!pPlayer || pTargetPlayer == pPlayer)
			continue;

		if (pPlayer->IsAdminFlagSet(ADMFLAG_GENERIC) && CCSPlayerController::FromSlot(i) != player)
			ClientPrintT(CCSPlayerController::FromSlot(i), HUD_PRINTTALK, "{Admin.PMToOthers}", pTarget->GetPlayerName(), pszName, strMessage.c_str());
	}

	ClientPrintT(player, HUD_PRINTTALK, "{Admin.PMToSender}", pTarget->GetPlayerName(), pszName, strMessage.c_str());
	ClientPrintT(pTarget, HUD_PRINTTALK, "{Admin.PMToTarget}", pszName, strMessage.c_str());
	Message("[PM to %s] %s: %s\n", pTarget->GetPlayerName(), pszName, strMessage.c_str());
}

size_t CountCharacters(const std::string& str)
{
	size_t count = 0;
	for (size_t i = 0; i < str.length(); ++i)
	{
		// Check if byte is a UTF-8 lead byte (indicating a new character)
		if ((str[i] & 0xC0) != 0x80)
			++count;
	}
	return count;
}

CON_COMMAND_CHAT_FLAGS(who, "- List the flags of all online players", ADMFLAG_GENERIC)
{
	if (!GetGlobals())
		return;

	std::vector<std::tuple<std::string, std::string, uint64>> rgNameSlotID;

	for (size_t i = 0; i < GetGlobals()->maxClients; i++)
	{
		CCSPlayerController* ccsPly = CCSPlayerController::FromSlot(i);

		if (!ccsPly)
			continue;

		ZEPlayer* pPlayer = ccsPly->GetZEPlayer();

		if (!pPlayer)
			continue;

		std::string strName = ccsPly->GetPlayerName();
		if (strName.length() == 0)
			strName = "< blank >";
		else if (CountCharacters(strName) > 20)
			strName = strName.substr(0, 17) + "..."; // This may extra shorten unicode character names, but whatever

		if (pPlayer->IsFakeClient())
		{
			rgNameSlotID.push_back(std::tuple<std::string, std::string, uint64>(strName, "BOT", 0));
			continue;
		}

		if (!pPlayer->IsAuthenticated())
		{
			rgNameSlotID.push_back(std::tuple<std::string, std::string, uint64>(strName, "UNAUTHENTICATED", pPlayer->GetUnauthenticatedSteamId64()));
			continue;
		}

		uint64 iSteamID = pPlayer->GetSteamId64();
		uint64 iFlags = pPlayer->GetAdminFlags();
		std::string strFlags = "";

		if (iFlags & ADMFLAG_ROOT)
			strFlags = "ROOT";
		else
		{
			if (iFlags & ADMFLAG_RESERVATION)
				strFlags.append(", RESERVATION");
			if (iFlags & ADMFLAG_GENERIC)
				strFlags.append(", GENERIC");
			if (iFlags & ADMFLAG_KICK)
				strFlags.append(", KICK");
			if (iFlags & ADMFLAG_BAN)
				strFlags.append(", BAN");
			if (iFlags & ADMFLAG_UNBAN)
				strFlags.append(", UNBAN");
			if (iFlags & ADMFLAG_SLAY)
				strFlags.append(", SLAY");
			if (iFlags & ADMFLAG_CHANGEMAP)
				strFlags.append(", CHANGEMAP");
			if (iFlags & ADMFLAG_CONVARS)
				strFlags.append(", CONVARS");
			if (iFlags & ADMFLAG_CONFIG)
				strFlags.append(", CONFIG");
			if (iFlags & ADMFLAG_CHAT)
				strFlags.append(", CHAT");
			if (iFlags & ADMFLAG_VOTE)
				strFlags.append(", VOTE");
			if (iFlags & ADMFLAG_PASSWORD)
				strFlags.append(", PASSWORD");
			if (iFlags & ADMFLAG_RCON)
				strFlags.append(", RCON");
			if (iFlags & ADMFLAG_CHEATS)
				strFlags.append(", CHEATS");
			if (iFlags & ADMFLAG_CUSTOM1)
				strFlags.append(", CUSTOM1");
			if (iFlags & ADMFLAG_CUSTOM2)
				strFlags.append(", CUSTOM2");
			if (iFlags & ADMFLAG_CUSTOM3)
				strFlags.append(", CUSTOM3");
			if (iFlags & ADMFLAG_CUSTOM4)
				strFlags.append(", CUSTOM4");
			if (iFlags & ADMFLAG_CUSTOM5)
				strFlags.append(", CUSTOM5");
			if (iFlags & ADMFLAG_CUSTOM6)
				strFlags.append(", CUSTOM6");
			if (iFlags & ADMFLAG_CUSTOM7)
				strFlags.append(", CUSTOM7");
			if (iFlags & ADMFLAG_CUSTOM8)
				strFlags.append(", CUSTOM8");
			if (iFlags & ADMFLAG_CUSTOM9)
				strFlags.append(", CUSTOM9");
			if (iFlags & ADMFLAG_CUSTOM10)
				strFlags.append(", CUSTOM10");
			if (iFlags & ADMFLAG_CUSTOM11)
				strFlags.append(", CUSTOM11");

			if (strFlags.length() > 1)
				strFlags = strFlags.substr(2);
			else
				strFlags = "-";
		}

		rgNameSlotID.push_back(std::tuple<std::string, std::string, uint64>(strName, strFlags, iSteamID));
	}
	std::sort(rgNameSlotID.begin(), rgNameSlotID.end(), [](auto const& a, auto const& b) {
		std::string f = std::get<0>(a);
		std::string s = std::get<0>(b);
		std::transform(f.begin(), f.end(), f.begin(), [](unsigned char c) { return c > 127 ? 127 : std::tolower(c); });
		std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return c > 127 ? 127 : std::tolower(c); });
		return f < s;
	});

	if (rgNameSlotID.size() == 0)
	{
		ClientPrintT(player, HUD_PRINTTALK, "{Admin.NoClientsOnline}");
		return;
	}

	ClientPrintT(player, HUD_PRINTCONSOLE, "{Admin.WhoOutput}", rgNameSlotID.size(), rgNameSlotID.size() == 1 ? "" : "s");
	ClientPrint(player, HUD_PRINTCONSOLE, "|----------------------|----------------------------------------------------|-------------------|");
	ClientPrint(player, HUD_PRINTCONSOLE, "|         Name         |                       Flags                        |    Steam64 ID     |");
	ClientPrint(player, HUD_PRINTCONSOLE, "|----------------------|----------------------------------------------------|-------------------|");
	for (auto [strPlayerName, strFlags, iSteamID] : rgNameSlotID)
	{
		if (CountCharacters(strPlayerName) % 2 == 1)
			strPlayerName = strPlayerName + ' ';
		if (CountCharacters(strPlayerName) < 20)
			strPlayerName = std::string((20 - CountCharacters(strPlayerName)) / 2, ' ') + strPlayerName + std::string((20 - CountCharacters(strPlayerName)) / 2, ' ');

		if (strFlags.length() <= 50)
		{
			if (strFlags.length() % 2 == 1)
				strFlags = strFlags + ' ';
			if (strFlags.length() < 50)
				strFlags = std::string((50 - strFlags.length()) / 2, ' ') + strFlags + std::string((50 - strFlags.length()) / 2, ' ');

			if (iSteamID != 0)
				ClientPrint(player, HUD_PRINTCONSOLE, "| %s | %s | %lli |", strPlayerName.c_str(), strFlags.c_str(), iSteamID);
			else
				ClientPrint(player, HUD_PRINTCONSOLE, "| %s | %s | 00000000000000000 |", strPlayerName.c_str(), strFlags.c_str());
		}
		else
		{
			int iIndexToCut = strFlags.substr(0, 50).find_last_of(',') + 1;
			std::string strTemp = strFlags.substr(0, iIndexToCut);
			if (strTemp.length() % 2 == 1)
				strTemp = strTemp + ' ';
			if (strTemp.length() < 50)
				strTemp = std::string((50 - strTemp.length()) / 2, ' ') + strTemp + std::string((50 - strTemp.length()) / 2, ' ');
			strFlags = strFlags.substr(iIndexToCut + 1);

			if (iSteamID != 0)
				ClientPrint(player, HUD_PRINTCONSOLE, "| %s | %s | %lli |", strPlayerName.c_str(), strTemp.c_str(), iSteamID);
			else
				ClientPrint(player, HUD_PRINTCONSOLE, "| %s | %s | 00000000000000000 |", strPlayerName.c_str(), strTemp.c_str());
			while (strFlags.length() > 0)
			{
				iIndexToCut = strFlags.substr(0, 50).find_last_of(',') + 1;
				if (iIndexToCut == 0 || iIndexToCut + 1 > strFlags.length() || strFlags.length() < 50)
				{
					strTemp = strFlags;
					strFlags = "";
				}
				else
				{
					strTemp = strFlags.substr(0, iIndexToCut);
					strFlags = strFlags.substr(iIndexToCut + 1);
				}
				if (strTemp.length() % 2 == 1)
					strTemp = ' ' + strTemp;
				if (strTemp.length() < 50)
					strTemp = std::string((50 - strTemp.length()) / 2, ' ') + strTemp + std::string((50 - strTemp.length()) / 2, ' ');
				ClientPrint(player, HUD_PRINTCONSOLE, "|                      | %s |                   |", strTemp.c_str());
			}
		}
	}
	ClientPrint(player, HUD_PRINTCONSOLE, "|----------------------|----------------------------------------------------|-------------------|");
	if (player)
		ClientPrintT(player, HUD_PRINTTALK, CHAT_PREFIX "{General.SeeConsole}");
}

CON_COMMAND_CHAT(status, "<name> - Checks a player's active punishments. Non-admins may only check their own punishments")
{
	int iNumClients = 0;
	int pSlots[MAXPLAYERS];
	ETargetType nType;
	ZEPlayer* pTargetPlayer = nullptr;
	bool bIsAdmin = !player || player->GetZEPlayer()->IsAdminFlagSet(ADMFLAG_GENERIC);
	std::string strTarget = (!bIsAdmin || args.ArgC() == 1) ? "@me" : args[1];

	if (!g_playerManager->CanTargetPlayers(player, strTarget.c_str(), iNumClients, pSlots, NO_UNAUTHENTICATED | NO_MULTIPLE | NO_BOT, nType))
		return;

	CCSPlayerController* pTarget = CCSPlayerController::FromSlot(pSlots[0]);
	pTargetPlayer = pTarget->GetZEPlayer();

	if (!pTargetPlayer->IsMuted() && !pTargetPlayer->IsGagged())
	{
		if (pTarget == player)
			ClientPrintT(player, HUD_PRINTTALK, CHAT_PREFIX "{Admin.StatusNoPunishmentsSelf}");
		else
			ClientPrintT(player, HUD_PRINTTALK, CHAT_PREFIX "{Admin.StatusNoPunishments}", pTarget->GetPlayerName());
		return;
	}

	if (pTarget == player)
	{
		if (pTargetPlayer->IsMuted() && pTargetPlayer->IsGagged())
			ClientPrintT(player, HUD_PRINTTALK, CHAT_PREFIX "{Admin.StatusMutedAndGagged}");
		else if (pTargetPlayer->IsMuted())
			ClientPrintT(player, HUD_PRINTTALK, CHAT_PREFIX "{Admin.StatusMuted}");
		else if (pTargetPlayer->IsGagged())
			ClientPrintT(player, HUD_PRINTTALK, CHAT_PREFIX "{Admin.StatusGagged}");
	}
	else
	{
		if (pTargetPlayer->IsMuted() && pTargetPlayer->IsGagged())
			ClientPrintT(player, HUD_PRINTTALK, CHAT_PREFIX "{Admin.StatusTargetMutedAndGagged}", pTarget->GetPlayerName());
		else if (pTargetPlayer->IsMuted())
			ClientPrintT(player, HUD_PRINTTALK, CHAT_PREFIX "{Admin.StatusTargetMuted}", pTarget->GetPlayerName());
		else if (pTargetPlayer->IsGagged())
			ClientPrintT(player, HUD_PRINTTALK, CHAT_PREFIX "{Admin.StatusTargetGagged}", pTarget->GetPlayerName());
	}
}

CON_COMMAND_CHAT_FLAGS(listdc, "- List recently disconnected players and their Steam64 IDs", ADMFLAG_GENERIC)
{
	g_pAdminSystem->ShowDisconnectedPlayers(player);
}

CON_COMMAND_CHAT_FLAGS(endround, "- Immediately ends the round, client-side variant of endround", ADMFLAG_RCON)
{
	if (g_pGameRules)
		g_pGameRules->TerminateRound(0.0f, CSRoundEndReason::Draw);
}

CON_COMMAND_CHAT_FLAGS(money, "<name> <amount> - Set a player's amount of money", ADMFLAG_CHEATS)
{
	if (args.ArgC() < 3)
	{
		ClientPrintT(player, HUD_PRINTTALK, CHAT_PREFIX "{Admin.MoneyUsage}");
		return;
	}

	int iMoney = V_StringToInt32(args[2], -1);

	if (iMoney < 0)
	{
		ClientPrintT(player, HUD_PRINTTALK, CHAT_PREFIX "{Admin.MoneyInvalidAmount}");
		return;
	}

	int iNumClients = 0;
	int pSlots[MAXPLAYERS];
	ETargetType nType;

	if (!g_playerManager->CanTargetPlayers(player, args[1], iNumClients, pSlots, NO_TARGET_BLOCKS, nType))
		return;

	const char* pszCommandPlayerName = player ? player->GetPlayerName() : CONSOLE_NAME;

	for (int i = 0; i < iNumClients; i++)
	{
		CCSPlayerController* pTarget = CCSPlayerController::FromSlot(pSlots[i]);

		if (!pTarget)
			continue;

		pTarget->m_pInGameMoneyServices->m_iAccount = iMoney;

		if (iNumClients == 1)
			ClientPrintAllT(HUD_PRINTTALK, CHAT_PREFIX "{Admin.ActionSetMoney}", pszCommandPlayerName, iMoney, pTarget->GetPlayerName());
	}

	if (iNumClients > 1)
	{
		char szAction[64];
		V_snprintf(szAction, sizeof(szAction), "set $%i money on", iMoney);
		PrintMultiAdminAction(nType, pszCommandPlayerName, szAction, "");
	}
}

CON_COMMAND_CHAT_FLAGS(health, "<name> <health> - Set a player's health", ADMFLAG_CHEATS)
{
	if (args.ArgC() < 3)
	{
		ClientPrintT(player, HUD_PRINTTALK, CHAT_PREFIX "{Admin.HealthUsage}");
		return;
	}

	int iHealth = V_StringToInt32(args[2], -1);

	if (iHealth < 1)
	{
		ClientPrintT(player, HUD_PRINTTALK, CHAT_PREFIX "{Admin.HealthInvalidAmount}");
		return;
	}

	int iNumClients = 0;
	int pSlots[MAXPLAYERS];
	ETargetType nType;

	if (!g_playerManager->CanTargetPlayers(player, args[1], iNumClients, pSlots, NO_DEAD | NO_SPECTATOR, nType))
		return;

	const char* pszCommandPlayerName = player ? player->GetPlayerName() : CONSOLE_NAME;

	for (int i = 0; i < iNumClients; i++)
	{
		CCSPlayerController* pTarget = CCSPlayerController::FromSlot(pSlots[i]);

		if (!pTarget)
			continue;

		CCSPlayerPawn* pPawn = pTarget->GetPlayerPawn();

		if (!pPawn)
			continue;

		if (pPawn->m_iMaxHealth < iHealth)
			pPawn->m_iMaxHealth = iHealth;

		pPawn->m_iHealth = iHealth;

		if (iNumClients == 1)
			ClientPrintAllT(HUD_PRINTTALK, CHAT_PREFIX "{Admin.ActionSetHealth}", pszCommandPlayerName, iHealth, pTarget->GetPlayerName());
	}

	if (iNumClients > 1)
	{
		char szAction[64];
		V_snprintf(szAction, sizeof(szAction), "set %i health on", iHealth);
		PrintMultiAdminAction(nType, pszCommandPlayerName, szAction, "");
	}
}

CON_COMMAND_CHAT_FLAGS(setpos, "<x y z> - Set your origin", ADMFLAG_CHEATS)
{
	if (!player)
	{
		ClientPrintT(player, HUD_PRINTTALK, CHAT_PREFIX "{General.NoServerConsole}");
		return;
	}

	CBasePlayerPawn* pPawn = player->GetPawn();

	if (!pPawn)
		return;

	if (pPawn->m_iTeamNum() < CS_TEAM_T || !pPawn->IsAlive())
	{
		ClientPrintT(player, HUD_PRINTTALK, CHAT_PREFIX "{Admin.SetposMustBeAlive}");
		return;
	}

	Vector origin;
	V_StringToVector(args.ArgS(), origin);

	char szOrigin[64];
	V_snprintf(szOrigin, sizeof(szOrigin), "%f %f %f", origin.x, origin.y, origin.z);

	pPawn->Teleport(&origin, nullptr, nullptr);
	ClientPrintAllT(HUD_PRINTTALK, CHAT_PREFIX "{Admin.ActionTeleportedToPos}", player->GetPlayerName(), szOrigin);
}

CON_COMMAND_CHAT_FLAGS(strip, "<name> - Strip all the weapons/items of a player", ADMFLAG_CHEATS)
{
	if (args.ArgC() < 2)
	{
		ClientPrintT(player, HUD_PRINTTALK, CHAT_PREFIX "{Admin.StripUsage}");
		return;
	}

	int iNumClients = 0;
	int pSlots[MAXPLAYERS];
	ETargetType nType;

	if (!g_playerManager->CanTargetPlayers(player, args[1], iNumClients, pSlots, NO_DEAD | NO_SPECTATOR, nType))
		return;

	const char* pszCommandPlayerName = player ? player->GetPlayerName() : CONSOLE_NAME;

	for (int i = 0; i < iNumClients; i++)
	{
		CCSPlayerController* pTarget = CCSPlayerController::FromSlot(pSlots[i]);

		if (!pTarget)
			continue;

		CCSPlayerPawn* pPawn = pTarget->GetPlayerPawn();

		if (!pPawn)
			continue;

		CCSPlayer_ItemServices* pItemServices = pPawn->m_pItemServices();

		if (!pItemServices)
			return;

		pItemServices->StripPlayerWeapons(true);

		if (iNumClients == 1)
			ClientPrintAllT(HUD_PRINTTALK, CHAT_PREFIX "{Admin.ActionStripped}", pszCommandPlayerName, pTarget->GetPlayerName());
	}

	if (iNumClients > 1)
		PrintMultiAdminAction(nType, pszCommandPlayerName, "stripped", "");
}

CON_COMMAND_CHAT_FLAGS(give, "<name> <weapon> - Give a weapon/item to a player", ADMFLAG_CHEATS)
{
	if (args.ArgC() < 3)
	{
		ClientPrintT(player, HUD_PRINTTALK, CHAT_PREFIX "{Admin.GiveUsage}");
		return;
	}

	const WeaponInfo_t* pWeaponInfo = FindWeaponInfoByClassCaseInsensitive(args[2]);

	if (!pWeaponInfo)
	{
		ClientPrintT(player, HUD_PRINTTALK, CHAT_PREFIX "{Admin.GiveInvalidWeapon}", args[2]);
		return;
	}

	int iNumClients = 0;
	int pSlots[MAXPLAYERS];
	ETargetType nType;

	if (!g_playerManager->CanTargetPlayers(player, args[1], iNumClients, pSlots, NO_DEAD | NO_SPECTATOR, nType))
		return;

	const char* pszCommandPlayerName = player ? player->GetPlayerName() : CONSOLE_NAME;

	for (int i = 0; i < iNumClients; i++)
	{
		CCSPlayerController* pTarget = CCSPlayerController::FromSlot(pSlots[i]);

		if (!pTarget)
			continue;

		CCSPlayerPawn* pPawn = pTarget->GetPlayerPawn();

		if (!pPawn)
			continue;

		CCSPlayer_ItemServices* pItemServices = pPawn->m_pItemServices;
		CCSPlayer_WeaponServices* pWeaponServices = pPawn->m_pWeaponServices;

		if (!pItemServices || !pWeaponServices)
			return;

		if (pWeaponInfo->m_eSlot == GEAR_SLOT_RIFLE || pWeaponInfo->m_eSlot == GEAR_SLOT_PISTOL)
		{
			CUtlVector<CHandle<CBasePlayerWeapon>>* weapons = pWeaponServices->m_hMyWeapons();

			FOR_EACH_VEC(*weapons, i)
			{
				CBasePlayerWeapon* weapon = (*weapons)[i].Get();

				if (!weapon)
					continue;

				if (weapon->GetWeaponVData()->m_GearSlot() == pWeaponInfo->m_eSlot)
				{
					pWeaponServices->DropWeapon(weapon);
					break;
				}
			}
		}

		CBasePlayerWeapon* pWeapon = pItemServices->GiveNamedItemAws(pWeaponInfo->m_pClass);

		if (!pWeapon)
		{
			ClientPrintT(player, HUD_PRINTTALK, CHAT_PREFIX "{Admin.GiveFailed}", pWeaponInfo->m_pClass);
			return;
		}

		if (iNumClients == 1)
			ClientPrintAllT(HUD_PRINTTALK, CHAT_PREFIX "{Admin.ActionGave}", pszCommandPlayerName, args[2], pTarget->GetPlayerName());
	}

	if (iNumClients > 1)
	{
		char szAction[64];
		V_snprintf(szAction, sizeof(szAction), "given %s to", args[2]);
		PrintMultiAdminAction(nType, pszCommandPlayerName, szAction, "");
	}
}

#ifdef _DEBUG
CON_COMMAND_CHAT_FLAGS(add_dc, "<name> <SteamID 64> <IP Address> - Adds a fake player to disconnected player list for testing", ADMFLAG_GENERIC)
{
	if (args.ArgC() < 3)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Usage: !add_dc <name> <Steam64 ID> <IP Address>");
		return;
	}

	std::string strSteamID = args[2];
	if (strSteamID.length() != 17 || std::find_if(strSteamID.begin(), strSteamID.end(), [](unsigned char c) { return !std::isdigit(c); }) != strSteamID.end())
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Invalid Steam64 ID.");
		return;
	}
	// stoll should be exception safe with above check
	uint64 iSteamID = std::stoll(strSteamID);

	g_pAdminSystem->AddDisconnectedPlayer(args[1], iSteamID, args[3]);
}

CON_COMMAND_CHAT_FLAGS(setteam, "<name> <team (0-3)> - Set a player's team", ADMFLAG_CHEATS)
{
	if (args.ArgC() < 3)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Usage: !setteam <name> <team (0-3)>");
		return;
	}

	int iTeam = V_StringToInt32(args[2], -1);

	if (iTeam < CS_TEAM_NONE || iTeam > CS_TEAM_CT)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Invalid team specified, range is 0-3.");
		return;
	}

	int iNumClients = 0;
	int pSlots[MAXPLAYERS];
	ETargetType nType;

	if (!g_playerManager->CanTargetPlayers(player, args[1], iNumClients, pSlots, NO_TARGET_BLOCKS, nType))
		return;

	const char* pszCommandPlayerName = player ? player->GetPlayerName() : CONSOLE_NAME;

	constexpr const char* teams[] = {"none", "spectators", "terrorists", "counter-terrorists"};

	for (int i = 0; i < iNumClients; i++)
	{
		CCSPlayerController* pTarget = CCSPlayerController::FromSlot(pSlots[i]);

		if (!pTarget)
			continue;

		pTarget->SwitchTeam(iTeam);

		if (iNumClients == 1)
			ClientPrintAllT(HUD_PRINTTALK, CHAT_PREFIX "{Admin.ActionMovedToTeam}", pszCommandPlayerName, pTarget->GetPlayerName(), teams[iTeam]);
	}

	if (iNumClients > 1)
	{
		char szAction[64];
		V_snprintf(szAction, sizeof(szAction), "moved to %s", teams[iTeam]);
		PrintMultiAdminAction(nType, pszCommandPlayerName, szAction, "");
	}
}
#endif

void CAdmin::SetFlags(uint64 iFlags)
{
	CAdminBase::SetFlags(iFlags);

	if (!GetGlobals())
		return;

	ZEPlayer* zpAdmin = g_playerManager->GetPlayerFromSteamId(m_iSteamID);
	if (!zpAdmin) // Authentication is checked in GetPlayerFromSteamId, so dont need to redo it here
		return;

	zpAdmin->SetAdminFlags(iFlags);
}

void CAdmin::SetImmunity(std::uint32_t iAdminImmunity)
{
	if (iAdminImmunity > INT_MAX)
		iAdminImmunity = INT_MAX;
	CAdminBase::SetImmunity(iAdminImmunity);

	if (!GetGlobals())
		return;

	ZEPlayer* zpAdmin = g_playerManager->GetPlayerFromSteamId(m_iSteamID);
	if (!zpAdmin) // Authentication is checked in GetPlayerFromSteamId, so dont need to redo it here
		return;

	zpAdmin->SetAdminImmunity(static_cast<int>(iAdminImmunity)); // should be safe to cast due to earlier check
}

CAdminSystem::CAdminSystem()
{
	LoadAdmins();
	LoadInfractions();

	// Fill out disconnected player list with empty objects which we overwrite as players leave
	for (int i = 0; i < 20; i++)
		m_rgDCPly[i] = std::tuple<std::string, uint64, std::string>("", 0, "");
	m_iDCPlyIndex = 0;
}

// TODO: Remove this once servers have been given a few months to update cs2fixes
bool CAdminSystem::ConvertAdminsKVToJSON()
{
	KeyValues* pKV = new KeyValues("admins");
	KeyValues::AutoDelete autoDelete(pKV);

	const char* pszPath = "addons/cs2fixes/configs/admins.cfg";

	if (!pKV->LoadFromFile(g_pFullFileSystem, pszPath))
	{
		Warning("Failed to load %s\n", pszPath);
		return false;
	}

	ordered_json jAdmins;

	jAdmins["Admins"] = ordered_json(ordered_json::value_t::object);

	for (KeyValues* pKey = pKV->GetFirstSubKey(); pKey; pKey = pKey->GetNextKey())
	{
		ordered_json jAdmin;

		if (!pKey->FindKey("steamid"))
		{
			Warning("Admin entry %s is missing 'steam' key\n", pKey->GetName());
			return false;
		}

		jAdmin["name"] = pKey->GetName();

		if (pKey->FindKey("flags"))
			jAdmin["flags"] = pKey->GetString("flags", nullptr);

		if (pKey->FindKey("immunity"))
			jAdmin["immunity"] = pKey->GetInt("immunity", 0);

		jAdmins["Admins"][pKey->GetString("steamid", "")] = jAdmin;
	}

	const char* pszJsonPath = "addons/cs2fixes/configs/admins.jsonc";
	const char* pszKVConfigRenamePath = "addons/cs2fixes/configs/admins_old.cfg";
	char szPath[MAX_PATH];
	V_snprintf(szPath, sizeof(szPath), "%s%s%s", Plat_GetGameDirectory(), "/csgo/", pszJsonPath);
	std::ofstream jsonFile(szPath);

	if (!jsonFile.is_open())
	{
		Panic("Failed to open %s\n", pszJsonPath);
		return false;
	}

	jsonFile << std::setfill('\t') << std::setw(1) << jAdmins << std::endl;

	char szKVRenamePath[MAX_PATH];
	V_snprintf(szPath, sizeof(szPath), "%s%s%s", Plat_GetGameDirectory(), "/csgo/", pszPath);
	V_snprintf(szKVRenamePath, sizeof(szPath), "%s%s%s", Plat_GetGameDirectory(), "/csgo/", pszKVConfigRenamePath);

	std::rename(szPath, szKVRenamePath);

	// remove old cfg example if it exists
	const char* pszKVExamplePath = "addons/cs2fixes/configs/admins.cfg.example";
	V_snprintf(szPath, sizeof(szPath), "%s%s%s", Plat_GetGameDirectory(), "/csgo/", pszKVExamplePath);
	std::remove(szPath);

	Message("Successfully converted KV1 admins.cfg to JSON format at %s\n", pszJsonPath);
	return true;
}

bool CAdminSystem::LoadAdmins()
{
	m_mapAdmins.clear();
	m_mapAdminGroups.clear();

	const char* pszJsonPath = "addons/cs2fixes/configs/admins.jsonc";
	char szPath[MAX_PATH];
	V_snprintf(szPath, sizeof(szPath), "%s%s%s", Plat_GetGameDirectory(), "/csgo/", pszJsonPath);
	std::ifstream jsonFile(szPath);

	if (!jsonFile.is_open())
	{
		if (!ConvertAdminsKVToJSON())
		{
			Panic("Failed to open %s and convert KV1 admins.cfg to JSON format, admins are not loaded!\n", pszJsonPath);
			return false;
		}

		jsonFile.open(szPath);
	}

	ordered_json jAdminConfig = ordered_json::parse(jsonFile, nullptr, false, true);

	if (jAdminConfig.is_discarded())
	{
		Panic("Failed parsing JSON from %s, admins are not loaded!\n", pszJsonPath);
		return false;
	}

	ordered_json jAdmins = jAdminConfig.value("Admins", ordered_json());

	if (jAdmins.empty())
	{
		Panic("Failed parsing JSON from %s, admins are not loaded!\n", pszJsonPath);
		return false;
	}

	ordered_json jGroups = jAdminConfig.value("Groups", ordered_json());
	for (auto it = jGroups.cbegin(); it != jGroups.cend(); ++it)
	{
		const json& jGroup = it.value();

		if (jGroup.contains("immunity") && !jGroup["immunity"].is_number())
		{
			Panic("Group '%s' has non-numeric 'immunity' field\n", it.key().c_str());
			return false;
		}

		CAdminBase group = CAdminBase(ParseFlags(jGroup.value("flags", "")), jGroup.value("immunity", 0));
		m_mapAdminGroups.emplace(it.key(), group);

		ConMsg("Loaded group %s\n", it.key().c_str());
		ConMsg(" - Flags: %s\n", StringifyFlags(group.GetFlags()).c_str());
		ConMsg(" - Immunity: %i\n", group.GetImmunity());
	}

	for (auto it = jAdmins.cbegin(); it != jAdmins.cend(); ++it)
	{
		const json& jAdmin = it.value();

		if (jAdmin.contains("immunity") && !jAdmin["immunity"].is_number())
		{
			Panic("Admin '%s' has non-numeric 'immunity' field\n", it.key().c_str());
			return false;
		}

		uint64 iFlags = ParseFlags(jAdmin.value("flags", ""));
		int iImmunity = jAdmin.value("immunity", 0);

		ordered_json jInheritedGroups = jAdmin.value("groups", ordered_json());
		for (const auto& groupName : jInheritedGroups)
		{
			if (!groupName.is_string())
			{
				Panic("Admin '%s' has invalid group name in 'groups' array\n", it.key().c_str());
				return false;
			}

			const std::string& name = groupName.get<std::string>();

			auto jt = m_mapAdminGroups.find(name);
			if (jt == m_mapAdminGroups.end())
			{
				Panic("Admin '%s' has invalid group name '%s'\n", it.key().c_str(), name.c_str());
				return false;
			}

			const CAdminBase& group = jt->second;

			iFlags |= group.GetFlags();
			iImmunity = std::max(iImmunity, group.GetImmunity());
		}

		uint64 iSteamID = atoll(it.key().c_str());
		CAdmin admin = CAdmin(jAdmin.value("name", ""), iFlags, iImmunity, iSteamID);
		m_mapAdmins.emplace(iSteamID, admin);

		ConMsg("Loaded admin %s\n", it.key().c_str());
		ConMsg(" - Name: %s\n", admin.GetName().c_str());
		ConMsg(" - Flags: %s\n", StringifyFlags(admin.GetFlags()).c_str());
		ConMsg(" - Immunity: %i\n", admin.GetImmunity());
	}
	return true;
}

// If an admin with this iSteamID already exists, just update Flags and Immunity.
void CAdminSystem::AddOrUpdateAdmin(uint64 iSteamID, uint64 iFlags, int iAdminImmunity)
{
	CAdmin* admin = FindAdmin(iSteamID);
	if (!admin)
	{
		m_mapAdmins.emplace(iSteamID, CAdmin("< blank >", iFlags, iAdminImmunity, iSteamID));
		admin = FindAdmin(iSteamID);
	}

	// Set these even if we created a new admin with the flags, as these have
	// extra logic to apply new values to the player if they are currently online
	admin->SetFlags(iFlags);
	admin->SetImmunity(iAdminImmunity);
}

bool CAdminSystem::LoadInfractions()
{
	m_vecInfractions.PurgeAndDeleteElements();
	KeyValues* pKV = new KeyValues("infractions");
	KeyValues::AutoDelete autoDelete(pKV);

	const char* pszPath = "addons/cs2fixes/data/infractions.txt";

	if (!pKV->LoadFromFile(g_pFullFileSystem, pszPath))
	{
		Warning("Failed to load %s\n", pszPath);
		return false;
	}

	for (KeyValues* pKey = pKV->GetFirstSubKey(); pKey; pKey = pKey->GetNextKey())
	{
		uint64 iSteamId = pKey->GetUint64("steamid", -1);
		time_t iEndTime = pKey->GetUint64("endtime", -1);
		int iType = pKey->GetInt("type", -1);

		if (iSteamId == -1)
		{
			Warning("Infraction entry is missing 'steam' key\n");
			return false;
		}

		if (iEndTime == -1)
		{
			Warning("Infraction entry is missing 'endtime' key\n");
			return false;
		}

		if (iType == -1)
		{
			Warning("Infraction entry is missing 'type' key\n");
			return false;
		}

		switch (iType)
		{
			case CInfractionBase::Ban:
				AddInfraction(new CBanInfraction(iEndTime, iSteamId, true));
				break;
			case CInfractionBase::Mute:
				AddInfraction(new CMuteInfraction(iEndTime, iSteamId, true));
				break;
			case CInfractionBase::Gag:
				AddInfraction(new CGagInfraction(iEndTime, iSteamId, true));
				break;
			default:
				Warning("Invalid infraction type %d\n", iType);
		}
	}

	return true;
}

void CAdminSystem::SaveInfractions()
{
	KeyValues* pKV = new KeyValues("infractions");
	KeyValues* pSubKey;
	KeyValues::AutoDelete autoDelete(pKV);

	FOR_EACH_VEC(m_vecInfractions, i)
	{
		time_t timestamp = m_vecInfractions[i]->GetTimestamp();
		if (timestamp != 0 && timestamp < std::time(0))
			continue;

		char buf[5];
		V_snprintf(buf, sizeof(buf), "%d", i);
		pSubKey = new KeyValues(buf);
		pSubKey->AddUint64("steamid", m_vecInfractions[i]->GetSteamId64());
		pSubKey->AddUint64("endtime", m_vecInfractions[i]->GetTimestamp());
		pSubKey->AddInt("type", m_vecInfractions[i]->GetType());

		pKV->AddSubKey(pSubKey);
	}

	char szPath[MAX_PATH];
	V_snprintf(szPath, sizeof(szPath), "%s%s", Plat_GetGameDirectory(), "/csgo/addons/cs2fixes/data/infractions.txt");

	// Create the directory in case it doesn't exist
	g_pFullFileSystem->CreateDirHierarchyForFile(szPath, nullptr);

	if (!pKV->SaveToFile(g_pFullFileSystem, szPath))
		Warning("Failed to save infractions to %s\n", szPath);
}

void CAdminSystem::AddInfraction(CInfractionBase* infraction)
{
	m_vecInfractions.AddToTail(infraction);
}

// This function can run at least twice when a player connects: Immediately upon client connection, and also upon getting authenticated by steam.
// It's also run when we're periodically checking for infraction expiry in the case of mutes/gags.
// This returns false only when called from ClientConnect and the player is banned in order to reject them.
bool CAdminSystem::ApplyInfractions(ZEPlayer* player)
{
	FOR_EACH_VEC(m_vecInfractions, i)
	{
		// Because this can run without the player being authenticated, and the fact that we're applying a ban/mute here,
		// we can immediately just use the steamid we got from the connecting player.
		uint64 iSteamID = player->IsAuthenticated() ? player->GetSteamId64() : player->GetUnauthenticatedSteamId64();

		// We're only interested in infractions concerning this player
		if (m_vecInfractions[i]->GetSteamId64() != iSteamID)
			continue;

		// Undo the infraction just briefly while checking if it ran out
		m_vecInfractions[i]->UndoInfraction(player);

		time_t timestamp = m_vecInfractions[i]->GetTimestamp();
		if (timestamp != 0 && timestamp <= std::time(0))
		{
			m_vecInfractions.Remove(i);
			continue;
		}

		// We are called from ClientConnect and the player is banned, immediately reject them
		if (!player->IsConnected() && m_vecInfractions[i]->GetType() == CInfractionBase::EInfractionType::Ban)
			return false;

		m_vecInfractions[i]->ApplyInfraction(player);
	}

	return true;
}

bool CAdminSystem::FindAndRemoveInfraction(ZEPlayer* player, CInfractionBase::EInfractionType type)
{
	FOR_EACH_VEC_BACK(m_vecInfractions, i)
	{
		if (m_vecInfractions[i]->GetSteamId64() == player->GetSteamId64() && m_vecInfractions[i]->GetType() == type)
		{
			m_vecInfractions[i]->UndoInfraction(player);
			m_vecInfractions.Remove(i);

			return true;
		}
	}

	return false;
}

bool CAdminSystem::FindAndRemoveInfractionSteamId64(uint64 steamid64, CInfractionBase::EInfractionType type)
{
	FOR_EACH_VEC_BACK(m_vecInfractions, i)
	{
		if (m_vecInfractions[i]->GetSteamId64() == steamid64 && m_vecInfractions[i]->GetType() == type)
		{
			m_vecInfractions.Remove(i);

			return true;
		}
	}

	return false;
}

CAdmin* CAdminSystem::FindAdmin(uint64 iSteamID)
{
	auto it = m_mapAdmins.find(iSteamID);
	if (it == m_mapAdmins.end())
		return nullptr;

	return &it->second;
}

uint64 CAdminSystem::ParseFlags(std::string strFlags)
{
	uint64 flags = 0;

	for (size_t i = 0; i < strFlags.length(); i++)
	{
		char c = tolower(strFlags[i]);
		if (c < 'a' || c > 'z')
			continue;

		if (c == 'z')
			return -1; // all flags

		flags |= ((uint64)1 << (c - 'a'));
	}

	return flags;
}

std::string CAdminSystem::StringifyFlags(uint64 iFlags)
{
	if (iFlags == static_cast<uint64>(-1))
		return "z"; // root / all permissions

	std::string strFlags;

	for (int i = 0; i < 25; ++i) // 'a' to 'y'
		if (iFlags & (static_cast<uint64>(1) << i))
			strFlags += static_cast<char>('a' + i);

	return strFlags;
}

void CAdminSystem::AddDisconnectedPlayer(const char* pszName, uint64 xuid, const char* pszIP)
{
	auto plyInfo = std::make_tuple(pszName, xuid, pszIP);
	for (auto& dcPlyInfo : m_rgDCPly)
		if (std::get<1>(dcPlyInfo) == std::get<1>(plyInfo))
			return;
	m_rgDCPly[m_iDCPlyIndex] = plyInfo;
	m_iDCPlyIndex = (m_iDCPlyIndex + 1) % 20;
}

void CAdminSystem::ShowDisconnectedPlayers(CCSPlayerController* const pAdmin)
{
	bool bAnyDCedPlayers = false;
	for (int i = 1; i <= 20; i++)
	{
		int index = (m_iDCPlyIndex - i) % 20;
		if (index < 0)
			index += 20;
		std::tuple<std::string, uint64, std::string> ply = m_rgDCPly[index];
		if (std::get<1>(ply) != 0)
		{
			if (!bAnyDCedPlayers)
			{
				if (pAdmin)
					ClientPrintT(pAdmin, HUD_PRINTTALK, CHAT_PREFIX "{Admin.DCPlayersConsole}");
				ClientPrintT(pAdmin, HUD_PRINTCONSOLE, "{Admin.DCPlayersHeader}");
				bAnyDCedPlayers = true;
			}

			ClientPrint(pAdmin, HUD_PRINTCONSOLE, "%i. %s", i, std::get<0>(ply).c_str());
			ClientPrint(pAdmin, HUD_PRINTCONSOLE, "\tSteam64 ID - %s", std::to_string(std::get<1>(ply)).c_str());

			ZEPlayer* zpAdmin = pAdmin->GetZEPlayer();
			if (zpAdmin && zpAdmin->IsAdminFlagSet(ADMFLAG_RCON))
				ClientPrintT(pAdmin, HUD_PRINTCONSOLE, "\t{Info.IPAddress} - %s", std::get<2>(ply).c_str());
		}
	}
	if (!bAnyDCedPlayers)
		ClientPrintT(pAdmin, HUD_PRINTTALK, CHAT_PREFIX "{Admin.NoDCPlayers}");
}

void CBanInfraction::ApplyInfraction(ZEPlayer* player)
{
	g_pEngineServer2->DisconnectClient(player->GetPlayerSlot(), NETWORK_DISCONNECT_KICKBANADDED, "Banned by an admin"); // "Kicked and banned"
}

void CMuteInfraction::ApplyInfraction(ZEPlayer* player)
{
	player->SetMuted(true);
}

void CMuteInfraction::UndoInfraction(ZEPlayer* player)
{
	player->SetMuted(false);
}

void CGagInfraction::ApplyInfraction(ZEPlayer* player)
{
	player->SetGagged(true);
}

void CGagInfraction::UndoInfraction(ZEPlayer* player)
{
	player->SetGagged(false);
}

void CEbanInfraction::ApplyInfraction(ZEPlayer* player)
{
	player->SetEbanned(true);
}

void CEbanInfraction::UndoInfraction(ZEPlayer* player)
{
	player->SetEbanned(false);
}

std::string FormatTime(std::time_t wTime, bool bInSeconds)
{
	if (bInSeconds)
	{
		if (wTime < 60)
			return std::to_string(static_cast<int>(std::floor(wTime))) + " second" + (wTime >= 2 ? "s" : "");
		wTime = wTime / 60;
	}

	if (wTime < 60)
		return std::to_string(static_cast<int>(std::floor(wTime))) + " minute" + (wTime >= 2 ? "s" : "");
	wTime = wTime / 60;

	if (wTime < 24)
		return std::to_string(static_cast<int>(std::floor(wTime))) + " hour" + (wTime >= 2 ? "s" : "");
	wTime = wTime / 24;

	if (wTime < 7)
		return std::to_string(static_cast<int>(std::floor(wTime))) + " day" + (wTime >= 2 ? "s" : "");
	wTime = wTime / 7;

	if (wTime < 4)
		return std::to_string(static_cast<int>(std::floor(wTime))) + " week" + (wTime >= 2 ? "s" : "");
	wTime = wTime / 4;

	return std::to_string(static_cast<int>(std::floor(wTime))) + " month" + (wTime >= 2 ? "s" : "");
}

int ParseTimeInput(std::string strTime, int iDefaultValue)
{
	// Check if first character is a minus sign, and just return negative 1 if so.
	if (strTime.length() > 0 && std::find_if(strTime.begin(), strTime.begin() + 1, [](char c) { return c == '-'; }) != (strTime.begin() + 1))
		return -1;

	std::string strNumbers = "";
	std::copy_if(strTime.begin(), strTime.end(), std::back_inserter(strNumbers), [](char c) { return std::isdigit(c); });

	if (strNumbers.length() == 0)
		return iDefaultValue;
	else if (strNumbers.length() > 9)
		// Really high number, just return perma
		return 0;

	// stoi should be exception safe here due to above checks
	int iDuration = std::stoi(strNumbers.c_str());

	if (iDuration == 0)
		return 0;
	else if (iDuration < 0)
		return -1;

	switch (strTime[strTime.length() - 1])
	{
		case 'h':
		case 'H':
			return iDuration * 60.0 > INT_MAX ? 0 : iDuration * 60;
		case 'd':
		case 'D':
			return iDuration * 60.0 * 24.0 > INT_MAX ? 0 : iDuration * 60 * 24;
		case 'w':
		case 'W':
			return iDuration * 60.0 * 24.0 * 7.0 > INT_MAX ? 0 : iDuration * 60 * 24 * 7;
		case 'm':
		case 'M':
			return iDuration * 60.0 * 24.0 * 30.0 > INT_MAX ? 0 : iDuration * 60 * 24 * 30;
		default:
			return iDuration;
	}
}

std::string GetReason(const CCommand& args, int iArgsBefore, bool bStripUnicode)
{
	if (args.ArgC() <= iArgsBefore + 1)
		return "";
	std::string strTemp = args.ArgS();
	std::string strReason = "";
	// Remove all double quotes
	std::copy_if(strTemp.cbegin(), strTemp.cend(), std::back_inserter(strReason), [](unsigned char c) { return c != '\"'; });

	for (size_t i = 1; i <= iArgsBefore; i++)
	{
		// Remove spaces if arguements were split up by them.
		while (strReason.length() > 0 && strReason.at(0) == ' ')
			strReason = strReason.substr(1);
		int iToRemove = std::string(args[i]).length();
		if (iToRemove >= strReason.length())
			return "";
		strReason = strReason.substr(iToRemove);
	}

	std::string strOutput = "";
	if (bStripUnicode)
		std::copy_if(strReason.cbegin(), strReason.cend(), std::back_inserter(strOutput), [](unsigned char c) { return c < 128; });
	else
		strOutput = strReason;

	// Clean up both ends of string very inefficiently...
	while (strOutput.length() > 0 && (strOutput.at(0) == ' ' || strOutput.at(0) == '\"'))
		strOutput = strOutput.substr(1);
	while (strOutput.length() > 0 && (strOutput.at(strOutput.length() - 1) == ' ' || strOutput.at(strOutput.length() - 1) == '\"'))
		strOutput = strOutput.substr(0, strOutput.length() - 1);

	return strOutput;
}

void ParseInfraction(const CCommand& args, CCSPlayerController* pAdmin, bool bAdding, CInfractionBase::EInfractionType infType)
{
	if (args.ArgC() < 2 || (bAdding && args.ArgC() < 3))
	{
		if (bAdding)
			ClientPrintT(pAdmin, HUD_PRINTTALK, CHAT_PREFIX "{Admin.InfractionUsageAdd}",
						GetActionPhrase(infType, PresentOrNoun, bAdding));
		else
			ClientPrintT(pAdmin, HUD_PRINTTALK, CHAT_PREFIX "{Admin.InfractionUsageRemove}",
						GetActionPhrase(infType, PresentOrNoun, bAdding));
		return;
	}

	int iDuration = bAdding ? ParseTimeInput(args[2]) : 0;
	if (bAdding && iDuration < 0)
	{
		ClientPrintT(pAdmin, HUD_PRINTTALK, CHAT_PREFIX "{Admin.InfractionInvalidDuration}");
		return;
	}

	int iNumClients = 0;
	int pSlots[MAXPLAYERS];
	ETargetType nType;

	uint64 iBlockedFlags = NO_RANDOM | NO_SELF | NO_BOT | NO_UNAUTHENTICATED;

	// Only allow multiple targetting for mutes that aren't perma (ie. !mute @all 1) for stopping mass mic spam
	if (infType != CInfractionBase::EInfractionType::Mute || (bAdding && iDuration == 0))
		iBlockedFlags |= NO_MULTIPLE;

	ETargetError eType = g_playerManager->GetPlayersFromString(pAdmin, args[1], iNumClients, pSlots, iBlockedFlags, nType);

	if (bAdding && iDuration == 0 && (eType == ETargetError::MULTIPLE || eType == ETargetError::RANDOM))
	{
		ClientPrintT(pAdmin, HUD_PRINTTALK, CHAT_PREFIX "{Admin.InfractionOnlyIndividuals}",
					GetActionPhrase(infType, GrammarTense::PresentOrNoun, bAdding));
		return;
	}
	else if (eType != ETargetError::NO_ERRORS)
	{
		std::string strError = g_playerManager->GetErrorString(eType);

		char szFormat[256];
		V_snprintf(szFormat, sizeof(szFormat), CHAT_PREFIX "%s", strError.c_str());
		ClientPrintT(pAdmin, HUD_PRINTTALK, szFormat);
		return;
	}

	const char* pszCommandPlayerName = pAdmin ? pAdmin->GetPlayerName() : CONSOLE_NAME;

	for (int i = 0; i < iNumClients; i++)
	{
		CCSPlayerController* pTarget = CCSPlayerController::FromSlot(pSlots[i]);
		ZEPlayer* zpTarget = pTarget->GetZEPlayer();

		if (!bAdding)
			g_pAdminSystem->FindAndRemoveInfraction(zpTarget, infType);
		else
		{
			CInfractionBase* infraction;
			switch (infType)
			{
				case CInfractionBase::Mute:
					infraction = new CMuteInfraction(iDuration, zpTarget->GetSteamId64());
					break;
				case CInfractionBase::Gag:
					infraction = new CGagInfraction(iDuration, zpTarget->GetSteamId64());
					break;
				case CInfractionBase::Ban:
					infraction = new CBanInfraction(iDuration, zpTarget->GetSteamId64());
					break;
				case CInfractionBase::Eban:
					infraction = new CEbanInfraction(iDuration, zpTarget->GetSteamId64());
					break;
				default:
					// This should never be reached, since we it means we are trying to apply an unimplemented block type
					ClientPrintT(pAdmin, HUD_PRINTTALK, CHAT_PREFIX "{Admin.InfractionImproperType}");
					return;
			}

			// We're overwriting the infraction, so remove the previous one first
			g_pAdminSystem->FindAndRemoveInfraction(zpTarget, infType);
			g_pAdminSystem->AddInfraction(infraction);
			infraction->ApplyInfraction(zpTarget);
		}

		if (iNumClients == 1 || (bAdding && iDuration == 0))
		{
			if (!bAdding)
				ClientPrintAllT(HUD_PRINTTALK, CHAT_PREFIX "{Admin.InfractionRemoved}", pszCommandPlayerName, GetActionPhrase(infType, GrammarTense::Past, bAdding), pTarget->GetPlayerName());
			else if (iDuration > 0)
				ClientPrintAllT(HUD_PRINTTALK, CHAT_PREFIX "{Admin.InfractionAppliedTemp}", pszCommandPlayerName, GetActionPhrase(infType, GrammarTense::Past, bAdding), pTarget->GetPlayerName(), FormatTime(iDuration, false).c_str());
			else
				ClientPrintAllT(HUD_PRINTTALK, CHAT_PREFIX "{Admin.InfractionAppliedPerm}", pszCommandPlayerName, GetActionPhrase(infType, GrammarTense::Past, bAdding), pTarget->GetPlayerName());
		}
	}

	if (iNumClients > 1)
	{
		if (!bAdding)
			PrintMultiAdminAction(nType, pszCommandPlayerName, GetActionPhrase(infType, GrammarTense::Past, bAdding), "");
		else
		{
			std::string strAction = GetActionPhrase(infType, GrammarTense::Past, bAdding);
			strAction.append(" for ");
			strAction.append(FormatTime(iDuration, false));
			PrintMultiAdminAction(nType, pszCommandPlayerName, strAction.c_str(), "");
		}
	}

	g_pAdminSystem->SaveInfractions();
}

// Returns a string matching the type of punishment and grammar tense specified
const char* GetActionPhrase(CInfractionBase::EInfractionType infType, GrammarTense iTense, bool bAdding)
{
	if (iTense == GrammarTense::PresentOrNoun)
	{
		switch (infType)
		{
			case CInfractionBase::Ban:
				return bAdding ? "ban" : "unban";
			case CInfractionBase::Mute:
				return bAdding ? "mute" : "unmute";
			case CInfractionBase::Gag:
				return bAdding ? "gag" : "ungag";
			case CInfractionBase::Eban:
				return bAdding ? "eban" : "eunban";
		}
	}
	else if (iTense == GrammarTense::Past)
	{
		switch (infType)
		{
			case CInfractionBase::Ban:
				return bAdding ? "banned" : "unbanned";
			case CInfractionBase::Mute:
				return bAdding ? "muted" : "unmuted";
			case CInfractionBase::Gag:
				return bAdding ? "gagged" : "ungagged";
			case CInfractionBase::Eban:
				return bAdding ? "ebanned" : "unebanned";
		}
	}
	else if (iTense == GrammarTense::Continuous)
	{
		switch (infType)
		{
			case CInfractionBase::Ban:
				return bAdding ? "banning" : "unbanning";
			case CInfractionBase::Mute:
				return bAdding ? "muting" : "unmuting";
			case CInfractionBase::Gag:
				return bAdding ? "gagging" : "ungagging";
			case CInfractionBase::Eban:
				return bAdding ? "ebanning" : "unebanning";
		}
	}
	return "";
}
