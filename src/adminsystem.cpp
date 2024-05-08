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


#include "adminsystem.h"
#include "KeyValues.h"
#include "interfaces/interfaces.h"
#include "filesystem.h"
#include "icvar.h"
#include "playermanager.h"
#include "commands.h"
#include "ctimer.h"
#include "detours.h"
#include "discord.h"
#include "utils/entity.h"
#include "entity/cbaseentity.h"
#include "entity/cparticlesystem.h"
#include "entity/cgamerules.h"
#include "gamesystem.h"
#include "votemanager.h"
#include "map_votes.h"
#include <vector>

extern IVEngineServer2 *g_pEngineServer2;
extern CGameEntitySystem *g_pEntitySystem;
extern CGlobalVars *gpGlobals;
extern CCSGameRules *g_pGameRules;

CAdminSystem* g_pAdminSystem = nullptr;

CUtlMap<uint32, CChatCommand *> g_CommandList(0, 0, DefLessFunc(uint32));

void PrintSingleAdminAction(const char* pszAdminName, const char* pszTargetName, const char* pszAction, const char* pszAction2 = "", const char* prefix = CHAT_PREFIX)
{
	ClientPrintAll(HUD_PRINTTALK, "%s" ADMIN_PREFIX "%s %s%s.", prefix, pszAdminName, pszAction, pszTargetName, pszAction2);
}

void PrintMultiAdminAction(ETargetType nType, const char* pszAdminName, const char* pszAction, const char* pszAction2 = "", const char* prefix = CHAT_PREFIX)
{
	switch (nType)
	{
	case ETargetType::ALL:
		ClientPrintAll(HUD_PRINTTALK, "%s" ADMIN_PREFIX "%s everyone%s.", prefix, pszAdminName, pszAction, pszAction2);
		break;
	case ETargetType::T:
		ClientPrintAll(HUD_PRINTTALK, "%s" ADMIN_PREFIX "%s terrorists%s.", prefix, pszAdminName, pszAction, pszAction2);
		break;
	case ETargetType::CT:
		ClientPrintAll(HUD_PRINTTALK, "%s" ADMIN_PREFIX "%s counter-terrorists%s.", prefix, pszAdminName, pszAction, pszAction2);
		break;
	}
}

CON_COMMAND_F(c_reload_admins, "Reload admin config", FCVAR_SPONLY | FCVAR_LINKED_CONCOMMAND)
{
	if (!g_pAdminSystem->LoadAdmins())
		return;

	for (int i = 0; i < gpGlobals->maxClients; i++)
	{
		ZEPlayer* pPlayer = g_playerManager->GetPlayer(i);

		if (!pPlayer || pPlayer->IsFakeClient() || !pPlayer->IsAuthenticated())
			continue;

		pPlayer->CheckAdmin();
	}

	Message("Admins reloaded\n");
}

CON_COMMAND_F(c_reload_infractions, "Reload infractions file", FCVAR_SPONLY | FCVAR_LINKED_CONCOMMAND)
{
	if (!g_pAdminSystem->LoadInfractions())
		return;

	for (int i = 0; i < gpGlobals->maxClients; i++)
	{
		ZEPlayer* pPlayer = g_playerManager->GetPlayer(i);

		if (!pPlayer || pPlayer->IsFakeClient())
			continue;

		pPlayer->CheckInfractions();
	}

	Message("Infractions reloaded\n");
}

CON_COMMAND_CHAT_FLAGS(ban, "<name> <minutes|0 (permament)> - ban a player", ADMFLAG_BAN)
{
	if (args.ArgC() < 3)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Usage: !ban <name> <duration/0 (permanent)>");
		return;
	}

	int iCommandPlayer = player ? player->GetPlayerSlot() : -1;
	int iNumClients = 0;
	int pSlot[MAXPLAYERS];

	if (g_playerManager->TargetPlayerString(iCommandPlayer, args[1], iNumClients, pSlot) > ETargetType::PLAYER)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "You can only target individual players for banning.");
		return;
	}

	if (!iNumClients)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Target not found.");
		return;
	}

	if (iNumClients > 1)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "More than one client matched.");
		return;
	}

	int iDuration = ParseTimeInput(args[2]);

	if (iDuration < 0)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Invalid duration.");
		return;
	}
	CCSPlayerController* pTarget = CCSPlayerController::FromSlot(pSlot[0]);

	if (!pTarget)
		return;

	ZEPlayer* pTargetPlayer = g_playerManager->GetPlayer(pSlot[0]);

	if (pTargetPlayer->IsFakeClient())
		return;

	if (!pTargetPlayer->IsAuthenticated())
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "%s is not yet authenticated, consider kicking instead or please wait a moment and try again.", pTarget->GetPlayerName());
		return;
	}

	CInfractionBase *infraction = new CBanInfraction(iDuration, pTargetPlayer->GetSteamId64());

	g_pAdminSystem->AddInfraction(infraction);
	infraction->ApplyInfraction(pTargetPlayer);
	g_pAdminSystem->SaveInfractions();

	const char *pszCommandPlayerName = player ? player->GetPlayerName() : "Console";

	if (iDuration > 0)
		PrintSingleAdminAction(pszCommandPlayerName, pTarget->GetPlayerName(), "banned", (" for " + FormatTime(iDuration, false)).c_str());
	else
		PrintSingleAdminAction(pszCommandPlayerName, pTarget->GetPlayerName(), "permanently banned");
}

CON_COMMAND_CHAT_FLAGS(unban, "<steamid64> - unbans a player. Takes decimal STEAMID64", ADMFLAG_BAN)
{
	if (args.ArgC() < 2)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Usage: !unban <steamid64>");
		return;
	}

	uint64 iTargetSteamId64 = V_StringToUint64(args[1], 0);

	bool bResult = g_pAdminSystem->FindAndRemoveInfractionSteamId64(iTargetSteamId64, CInfractionBase::Ban);

	if (!bResult)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Couldn't find user with STEAMID64 <%llu> in ban infractions.", iTargetSteamId64);
		return;		
	}

	g_pAdminSystem->SaveInfractions();

	// no need to broadcast this
	ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "User with STEAMID64 <%llu> has been unbanned.", iTargetSteamId64);
}

CON_COMMAND_CHAT_FLAGS(mute, "<name> <duration|0 (permament)> - mutes a player", ADMFLAG_CHAT)
{
	if (args.ArgC() < 3)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Usage: !mute <name> <duration/0 (permanent)>");
		return;
	}

	int iCommandPlayer = player ? player->GetPlayerSlot() : -1;
	int iNumClients = 0;
	int pSlot[MAXPLAYERS];

	ETargetType nType = g_playerManager->TargetPlayerString(iCommandPlayer, args[1], iNumClients, pSlot);

	if (!iNumClients)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Target not found.");
		return;
	}

	if (nType == ETargetType::PLAYER && iNumClients > 1)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "More than one client matched.");
		return;
	}

	int iDuration = ParseTimeInput(args[2]);

	if (iDuration < 0)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Invalid duration.");
		return;
	}

	if (iDuration == 0 && nType >= ETargetType::ALL)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "You may only permanently mute individuals.");
		return;
	}

	const char *pszCommandPlayerName = player ? player->GetPlayerName() : "Console";

	for (int i = 0; i < iNumClients; i++)
	{
		CCSPlayerController* pTarget = CCSPlayerController::FromSlot(pSlot[i]);

		if (!pTarget)
			continue;

		ZEPlayer* pTargetPlayer = g_playerManager->GetPlayer(pSlot[i]);

		if (pTargetPlayer->IsFakeClient())
			continue;

		if (!pTargetPlayer->IsAuthenticated())
		{
			ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "%s is not yet authenticated, please wait a moment and try again.", pTarget->GetPlayerName());
			continue;
		}

		CInfractionBase* infraction = new CMuteInfraction(iDuration, pTargetPlayer->GetSteamId64());

		// We're overwriting the infraction, so remove the previous one first
		g_pAdminSystem->FindAndRemoveInfraction(pTargetPlayer, CInfractionBase::Mute);
		g_pAdminSystem->AddInfraction(infraction);
		infraction->ApplyInfraction(pTargetPlayer);
		g_pAdminSystem->SaveInfractions();

		if (iDuration > 0)
			PrintSingleAdminAction(pszCommandPlayerName, pTarget->GetPlayerName(), "muted", (" for " + FormatTime(iDuration, false)).c_str());
		else
			PrintSingleAdminAction(pszCommandPlayerName, pTarget->GetPlayerName(), "permanently muted");
	}

	g_pAdminSystem->SaveInfractions();

	PrintMultiAdminAction(nType, pszCommandPlayerName, "muted", (" for " + FormatTime(iDuration, false)).c_str());
}

CON_COMMAND_CHAT_FLAGS(unmute, "<name> - unmutes a player", ADMFLAG_CHAT)
{
	if (args.ArgC() < 2)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Usage: !unmute <name>");
		return;
	}

	int iCommandPlayer = player ? player->GetPlayerSlot() : -1;
	int iNumClients = 0;
	int pSlot[MAXPLAYERS];

	ETargetType nType = g_playerManager->TargetPlayerString(iCommandPlayer, args[1], iNumClients, pSlot);

	if (!iNumClients)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Target not found.");
		return;
	}

	if (nType == ETargetType::PLAYER && iNumClients > 1)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "More than one client matched.");
		return;
	}

	const char *pszCommandPlayerName = player ? player->GetPlayerName() : "Console";

	for (int i = 0; i < iNumClients; i++)
	{
		CCSPlayerController* pTarget = CCSPlayerController::FromSlot(pSlot[i]);

		if (!pTarget)
			continue;

		ZEPlayer *pTargetPlayer = g_playerManager->GetPlayer(pSlot[i]);

		if (pTargetPlayer->IsFakeClient())
			continue;

		if (!g_pAdminSystem->FindAndRemoveInfraction(pTargetPlayer, CInfractionBase::Mute) && nType < ETargetType::ALL)
		{
			ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "%s is not muted.", pTarget->GetPlayerName());
			continue;
		}

		if (nType < ETargetType::ALL)
			PrintSingleAdminAction(pszCommandPlayerName, pTarget->GetPlayerName(), "unmuted");
	}

	g_pAdminSystem->SaveInfractions();

	PrintMultiAdminAction(nType, pszCommandPlayerName, "unmuted");
}

CON_COMMAND_CHAT_FLAGS(gag, "<name> <duration|0 (permanent)> - gag a player", ADMFLAG_CHAT)
{
	if (args.ArgC() < 3)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Usage: !gag <name> <duration/0 (permanent)>");
		return;
	}

	int iCommandPlayer = player ? player->GetPlayerSlot() : -1;
	int iNumClients = 0;
	int pSlot[MAXPLAYERS];

	ETargetType nType = g_playerManager->TargetPlayerString(iCommandPlayer, args[1], iNumClients, pSlot);

	if (!iNumClients)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Target not found.");
		return;
	}

	if (nType == ETargetType::PLAYER && iNumClients > 1)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "More than one client matched.");
		return;
	}

	int iDuration = ParseTimeInput(args[2]);

	if (iDuration < 0)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Invalid duration.");
		return;
	}

	if (iDuration == 0 && nType >= ETargetType::ALL)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "You may only permanently gag individuals.");
		return;
	}

	const char *pszCommandPlayerName = player ? player->GetPlayerName() : "Console";

	for (int i = 0; i < iNumClients; i++)
	{
		CCSPlayerController* pTarget = CCSPlayerController::FromSlot(pSlot[i]);

		if (!pTarget)
			continue;

		ZEPlayer *pTargetPlayer = g_playerManager->GetPlayer(pSlot[i]);

		if (pTargetPlayer->IsFakeClient())
			continue;

		if (!pTargetPlayer->IsAuthenticated())
		{
			ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "%s is not yet authenticated, please wait a moment and try again.", pTarget->GetPlayerName());
			continue;
		}

		CInfractionBase *infraction = new CGagInfraction(iDuration, pTargetPlayer->GetSteamId64());

		// We're overwriting the infraction, so remove the previous one first
		g_pAdminSystem->FindAndRemoveInfraction(pTargetPlayer, CInfractionBase::Gag);
		g_pAdminSystem->AddInfraction(infraction);
		infraction->ApplyInfraction(pTargetPlayer);

		if (nType >= ETargetType::ALL)
			continue;

		if (iDuration > 0)
			PrintSingleAdminAction(pszCommandPlayerName, pTarget->GetPlayerName(), "gagged", (" for " + FormatTime(iDuration, false)).c_str());
		else
			PrintSingleAdminAction(pszCommandPlayerName, pTarget->GetPlayerName(), "permanently gagged");
	}

	g_pAdminSystem->SaveInfractions();

	PrintMultiAdminAction(nType, pszCommandPlayerName, "gagged", (" for " + FormatTime(iDuration, false)).c_str());
}

CON_COMMAND_CHAT_FLAGS(ungag, "<name> - ungags a player", ADMFLAG_CHAT)
{
	if (args.ArgC() < 2)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Usage: !ungag <name>");
		return;
	}

	int iCommandPlayer = player ? player->GetPlayerSlot() : -1;
	int iNumClients = 0;
	int pSlot[MAXPLAYERS];

	ETargetType nType = g_playerManager->TargetPlayerString(iCommandPlayer, args[1], iNumClients, pSlot);

	if (!iNumClients)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Target not found.");
		return;
	}

	if (nType == ETargetType::PLAYER && iNumClients > 1)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "More than one client matched.");
		return;
	}

	const char *pszCommandPlayerName = player ? player->GetPlayerName() : "Console";

	for (int i = 0; i < iNumClients; i++)
	{
		CCSPlayerController* pTarget = CCSPlayerController::FromSlot(pSlot[i]);

		if (!pTarget)
			continue;

		ZEPlayer *pTargetPlayer = g_playerManager->GetPlayer(pSlot[i]);

		if (pTargetPlayer->IsFakeClient())
			continue;

		if (!g_pAdminSystem->FindAndRemoveInfraction(pTargetPlayer, CInfractionBase::Gag) && nType < ETargetType::ALL)
		{
			ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "%s is not gagged.", pTarget->GetPlayerName());
			continue;
		}

		if (nType < ETargetType::ALL)
			PrintSingleAdminAction(pszCommandPlayerName, pTarget->GetPlayerName(), "ungagged");
	}

	g_pAdminSystem->SaveInfractions();

	PrintMultiAdminAction(nType, pszCommandPlayerName, "ungagged");
}

CON_COMMAND_CHAT_FLAGS(kick, "<name> - kick a player", ADMFLAG_KICK)
{
	if (args.ArgC() < 2)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Usage: !kick <name>");
		return;
	}

	int iCommandPlayer = player ? player->GetPlayerSlot() : -1;
	int iNumClients = 0;
	int pSlot[MAXPLAYERS];

	if (g_playerManager->TargetPlayerString(iCommandPlayer, args[1], iNumClients, pSlot) == ETargetType::PLAYER && iNumClients > 1)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "More than one client matched.");
		return;
	}

	if (!iNumClients)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Target not found.");
		return;
	}

	const char *pszCommandPlayerName = player ? player->GetPlayerName() : "Console";

	for (int i = 0; i < iNumClients; i++)
	{
		CCSPlayerController* pTarget = CCSPlayerController::FromSlot(pSlot[i]);

		if (!pTarget)
			continue;

		ZEPlayer* pTargetPlayer = g_playerManager->GetPlayer(pSlot[i]);
		
		g_pEngineServer2->DisconnectClient(pTargetPlayer->GetPlayerSlot(), NETWORK_DISCONNECT_KICKED);

		PrintSingleAdminAction(pszCommandPlayerName, pTarget->GetPlayerName(), "kicked");
	}
}

CON_COMMAND_CHAT_FLAGS(slay, "<name> - slay a player", ADMFLAG_SLAY)
{
	if (args.ArgC() < 2)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Usage: !slay <name>");
		return;
	}

	int iCommandPlayer = player ? player->GetPlayerSlot() : -1;
	int iNumClients = 0;
	int pSlots[MAXPLAYERS];

	ETargetType nType = g_playerManager->TargetPlayerString(iCommandPlayer, args[1], iNumClients, pSlots);

	if (!iNumClients)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Target not found.");
		return;
	}

	if (nType == ETargetType::PLAYER && iNumClients > 1)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "More than one client matched.");
		return;
	}

	const char *pszCommandPlayerName = player ? player->GetPlayerName() : "Console";

	for (int i = 0; i < iNumClients; i++)
	{
		CCSPlayerController* pTarget = CCSPlayerController::FromSlot(pSlots[i]);

		if (!pTarget)
			continue;

		pTarget->GetPawn()->CommitSuicide(false, true);

		if (nType < ETargetType::ALL)
			PrintSingleAdminAction(pszCommandPlayerName, pTarget->GetPlayerName(), "slayed");
	}

	PrintMultiAdminAction(nType, pszCommandPlayerName, "slayed");
}

CON_COMMAND_CHAT_FLAGS(slap, "<name> [damage] - slap a player", ADMFLAG_SLAY)
{
	if (args.ArgC() < 2)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Usage: !slap <name> <optional damage>");
		return;
	}

	int iCommandPlayer = player ? player->GetPlayerSlot() : -1;
	int iNumClients = 0;
	int pSlots[MAXPLAYERS];

	ETargetType nType = g_playerManager->TargetPlayerString(iCommandPlayer, args[1], iNumClients, pSlots);

	if (!iNumClients)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Target not found.");
		return;
	}

	if (nType == ETargetType::PLAYER && iNumClients > 1)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "More than one client matched.");
		return;
	}

	const char *pszCommandPlayerName = player ? player->GetPlayerName() : "Console";

	for (int i = 0; i < iNumClients; i++)
	{
		CBasePlayerController *pTarget = (CBasePlayerController *)g_pEntitySystem->GetEntityInstance((CEntityIndex)(pSlots[i] + 1));

		if (!pTarget)
			continue;

		CBasePlayerPawn *pPawn = pTarget->m_hPawn();

		if (!pPawn)
			continue;

		// Taken directly from sourcemod
		Vector velocity = pPawn->m_vecAbsVelocity;
		velocity.x += ((rand() % 180) + 50) * (((rand() % 2) == 1) ? -1 : 1);
		velocity.y += ((rand() % 180) + 50) * (((rand() % 2) == 1) ? -1 : 1);
		velocity.z += rand() % 200 + 100;
		pPawn->SetAbsVelocity(velocity);

		float flDamage = V_StringToFloat32 (args[2], 0);
			
		if (flDamage > 0)
		{
			// Default to the world
			CBaseEntity *pAttacker = (CBaseEntity*)g_pEntitySystem->GetEntityInstance(CEntityIndex(0));

			if (player)
				pAttacker = player->GetPlayerPawn();

			CTakeDamageInfo info(pAttacker, pAttacker, nullptr, flDamage, DMG_GENERIC);
			pPawn->TakeDamage(info);
		}

		if (nType < ETargetType::ALL)
			PrintSingleAdminAction(pszCommandPlayerName, pTarget->GetPlayerName(), "slapped");
	}

	PrintMultiAdminAction(nType, pszCommandPlayerName, "slapped");
}

CON_COMMAND_CHAT_FLAGS(goto, "<name> - teleport to a player", ADMFLAG_SLAY)
{
	// Only players can use this command at all
	if (!player)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "You cannot use this command from the server console.");
		return;
	}

	if (args.ArgC() < 2)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Usage: !goto <name>");
		return;
	}

	int iNumClients = 0;
	int pSlots[MAXPLAYERS];

	if (g_playerManager->TargetPlayerString(player->GetPlayerSlot(), args[1], iNumClients, pSlots) > ETargetType::PLAYER || iNumClients > 1)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Target too ambiguous.");
		return;
	}

	if (!iNumClients)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Target not found.");
		return;
	}

	for (int i = 0; i < iNumClients; i++)
	{
		CCSPlayerController* pTarget = CCSPlayerController::FromSlot(pSlots[i]);

		if (!pTarget)
			continue;

		Vector newOrigin = pTarget->GetPawn()->GetAbsOrigin();

		player->GetPawn()->Teleport(&newOrigin, nullptr, nullptr);

		PrintSingleAdminAction(player->GetPlayerName(), pTarget->GetPlayerName(), "teleported to");
	}
}

CON_COMMAND_CHAT_FLAGS(bring, "<name> - bring a player", ADMFLAG_SLAY)
{
	if (!player)
	{
		ClientPrint(player, HUD_PRINTCONSOLE, CHAT_PREFIX "You cannot use this command from the server console.");
		return;
	}

	if (args.ArgC() < 2)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Usage: !bring <name>");
		return;
	}

	int iNumClients = 0;
	int pSlots[MAXPLAYERS];

	ETargetType nType = g_playerManager->TargetPlayerString(player->GetPlayerSlot(), args[1], iNumClients, pSlots);

	if (!iNumClients)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Target not found.");
		return;
	}

	if (nType == ETargetType::PLAYER && iNumClients > 1)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "More than one client matched.");
		return;
	}

	for (int i = 0; i < iNumClients; i++)
	{
		CCSPlayerController* pTarget = CCSPlayerController::FromSlot(pSlots[i]);

		if (!pTarget)
			continue;

		Vector newOrigin = player->GetPawn()->GetAbsOrigin();

		pTarget->GetPawn()->Teleport(&newOrigin, nullptr, nullptr);

		if (nType < ETargetType::ALL)
			PrintSingleAdminAction(player->GetPlayerName(), pTarget->GetPlayerName(), "brought");
	}

	PrintMultiAdminAction(nType, player->GetPlayerName(), "brought");
}

CON_COMMAND_CHAT_FLAGS(setteam, "<name> <team (0-3)> - set a player's team", ADMFLAG_SLAY)
{
	if (args.ArgC() < 3)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Usage: !setteam <name> <team (0-3)>");
		return;
	}

	int iCommandPlayer = player ? player->GetPlayerSlot() : -1;
	int iNumClients = 0;
	int pSlots[MAXPLAYERS];

	ETargetType nType = g_playerManager->TargetPlayerString(iCommandPlayer, args[1], iNumClients, pSlots);

	if (!iNumClients)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Target not found.");
		return;
	}

	if (nType == ETargetType::PLAYER && iNumClients > 1)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "More than one client matched.");
		return;
	}

	int iTeam = V_StringToInt32(args[2], -1);

	if (iTeam < CS_TEAM_NONE || iTeam > CS_TEAM_CT)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Invalid team specified, range is 0-3.");
		return;
	}

	const char *pszCommandPlayerName = player ? player->GetPlayerName() : "Console";

	constexpr const char *teams[] = {"none", "spectators", "terrorists", "counter-terrorists"};

	char szAction[64];
	V_snprintf(szAction, sizeof(szAction), " to %s", teams[iTeam]);

	for (int i = 0; i < iNumClients; i++)
	{
		CCSPlayerController* pTarget = CCSPlayerController::FromSlot(pSlots[i]);

		if (!pTarget)
			continue;

		pTarget->SwitchTeam(iTeam);

		if (nType < ETargetType::ALL)
			PrintSingleAdminAction(pszCommandPlayerName, pTarget->GetPlayerName(), "moved", szAction);
	}

	PrintMultiAdminAction(nType, pszCommandPlayerName, "moved", szAction);
}

CON_COMMAND_CHAT_FLAGS(noclip, "- toggle noclip on yourself", ADMFLAG_SLAY | ADMFLAG_CHEATS)
{
	if (!player)
	{
		ClientPrint(player, HUD_PRINTCONSOLE, CHAT_PREFIX "You cannot use this command from the server console.");
		return;
	}

	CBasePlayerPawn *pPawn = player->m_hPawn();

	if (!pPawn)
		return;

	if (pPawn->m_iHealth() <= 0)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "You cannot noclip while dead!");
		return;
	}

	if (pPawn->m_nActualMoveType() == MOVETYPE_NOCLIP)
	{
		pPawn->SetMoveType(MOVETYPE_WALK);
		ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX ADMIN_PREFIX "exited noclip.", player->GetPlayerName());
	}
	else
	{
		pPawn->SetMoveType(MOVETYPE_NOCLIP);
		ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX ADMIN_PREFIX "entered noclip.", player->GetPlayerName());
	}
}

CON_COMMAND_CHAT_FLAGS(reload_discord_bots, "- Reload discord bot config", ADMFLAG_ROOT)
{
	g_pDiscordBotManager->LoadDiscordBotsConfig();
	Message("Discord bot config reloaded\n");
}

CON_COMMAND_CHAT_FLAGS(entfire, "<name> <input> [parameter] - fire outputs at entities", ADMFLAG_RCON)
{
	if (args.ArgC() < 3)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Usage: !entfire <name> <input> <optional parameter>");
		return;
	}

	int iFoundEnts = 0;

	CBaseEntity *pTarget = nullptr;

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
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Target not found.");
	else
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Input successful on %i entities.", iFoundEnts);
}

CON_COMMAND_CHAT_FLAGS(entfirepawn, "<name> <inpu> [parameter] - fire outputs at player pawns", ADMFLAG_RCON)
{
	if (args.ArgC() < 3)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Usage: !entfirepawn <name> <input> <optional parameter>");
		return;
	}

	int iCommandPlayer = player ? player->GetPlayerSlot() : -1;
	int iNumClients = 0;
	int pSlots[MAXPLAYERS];

	ETargetType nType = g_playerManager->TargetPlayerString(iCommandPlayer, args[1], iNumClients, pSlots);

	if (!iNumClients)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Target not found.");
		return;
	}

	if (nType == ETargetType::PLAYER && iNumClients > 1)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "More than one client matched.");
		return;
	}

	int iFoundEnts = 0;

	for (int i = 0; i < iNumClients; i++)
	{
		CCSPlayerController *pTarget = CCSPlayerController::FromSlot(pSlots[i]);

		if (!pTarget || !pTarget->GetPawn())
			continue;

		pTarget->GetPawn()->AcceptInput(args[2], args[3], player, player);
		iFoundEnts++;
	}

	ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Input successful on %i player pawns.", iFoundEnts);
}

CON_COMMAND_CHAT_FLAGS(entfirecontroller, "<name> <input> [parameter] - fire outputs at player controllers", ADMFLAG_RCON)
{
	if (args.ArgC() < 3)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Usage: !entfirecontroller <name> <input> <optional parameter>");
		return;
	}

	int iCommandPlayer = player ? player->GetPlayerSlot() : -1;
	int iNumClients = 0;
	int pSlots[MAXPLAYERS];

	ETargetType nType = g_playerManager->TargetPlayerString(iCommandPlayer, args[1], iNumClients, pSlots);

	if (!iNumClients)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Target not found.");
		return;
	}

	if (nType == ETargetType::PLAYER && iNumClients > 1)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "More than one client matched.");
		return;
	}

	int iFoundEnts = 0;

	for (int i = 0; i < iNumClients; i++)
	{
		CCSPlayerController *pTarget = CCSPlayerController::FromSlot(pSlots[i]);

		if (!pTarget)
			continue;

		pTarget->AcceptInput(args[2], args[3], player, player);
		iFoundEnts++;
	}

	ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Input successful on %i player controllers.", iFoundEnts);
}

CON_COMMAND_CHAT_FLAGS(map, "<mapname> - change map", ADMFLAG_CHANGEMAP)
{
	if (args.ArgC() < 2)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Usage: !map <mapname>");
		return;
	}

	if (!g_pEngineServer2->IsMapValid(args[1]))
	{
		// Injection prevention, because we pass user input to ServerCommand
		for (int i = 0; i < strlen(args[1]); i++)
		{
			if (args[1][i] == ';')
				return;
		}

		std::string sCommand;

		// Check if input is numeric (workshop ID)
		// Not safe to expose until crashing on failed workshop addon downloads is fixed
		/*if (V_StringToUint64(args[1], 0) != 0)
		{
			sCommand = "host_workshop_map " + std::string(args[1]);
		}*/
		if (g_bVoteManagerEnable && g_pMapVoteSystem->GetMapIndexFromSubstring(args[1]) != -1)
		{
			sCommand = "host_workshop_map " + std::to_string(g_pMapVoteSystem->GetMapWorkshopId(g_pMapVoteSystem->GetMapIndexFromSubstring(args[1])));
		}
		else
		{
			ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Failed to find a map matching %s.", args[1]);
			return;
		}

		ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX "Changing map to %s...", args[1]);

		new CTimer(5.0f, false, true, [sCommand]()
		{
			g_pEngineServer2->ServerCommand(sCommand.c_str());
			return -1.0f;
		});

		return;
	}

	// Copy the string, since we're passing this into a timer
	char szMapName[MAX_PATH];
	V_strncpy(szMapName, args[1], sizeof(szMapName));

	ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX "Changing map to %s...", szMapName);

	new CTimer(5.0f, false, true, [szMapName]()
	{
		g_pEngineServer2->ChangeLevel(szMapName, nullptr);
		return -1.0f;
	});
}

CON_COMMAND_CHAT_FLAGS(hsay, "<message> - say something as a hud hint", ADMFLAG_CHAT)
{
	if (args.ArgC() < 2)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Usage: !hsay <message>");
		return;
	}

	ClientPrintAll(HUD_PRINTCENTER, "%s", args.ArgS());
}

CON_COMMAND_CHAT_FLAGS(rcon, "<command> - send a command to server console", ADMFLAG_RCON)
{
	if (!player)
	{
		ClientPrint(player, HUD_PRINTCONSOLE, CHAT_PREFIX "You are already on the server console.");
		return;
	}

	if (args.ArgC() < 2)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Usage: !rcon <command>");
		return;
	}

	g_pEngineServer2->ServerCommand(args.ArgS());
}

CON_COMMAND_CHAT_FLAGS(extend, "<minutes> - extend current map (negative value reduces map duration)", ADMFLAG_CHANGEMAP)
{
	if (args.ArgC() < 2)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Usage: !extend <minutes>");
		return;
	}

	int iExtendTime = V_StringToInt32(args[1], 0);

	ConVar* cvar = g_pCVar->GetConVar(g_pCVar->FindConVar("mp_timelimit"));

	// CONVAR_TODO
	// HACK: values is actually the cvar value itself, hence this ugly cast.
	float flTimelimit = *(float *)&cvar->values;

	if (gpGlobals->curtime - g_pGameRules->m_flGameStartTime > flTimelimit * 60)
		flTimelimit = (gpGlobals->curtime - g_pGameRules->m_flGameStartTime) / 60.0f + iExtendTime;
	else
	{
		if (flTimelimit == 1)
			flTimelimit = 0;
		flTimelimit += iExtendTime;
	}

	if (flTimelimit <= 0)
		flTimelimit = 1;

	// CONVAR_TODO
	char buf[32];
	V_snprintf(buf, sizeof(buf), "mp_timelimit %.6f", flTimelimit);
	g_pEngineServer2->ServerCommand(buf);

	const char* pszCommandPlayerName = player ? player->GetPlayerName() : "Console";

	if (iExtendTime < 0)
		ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX ADMIN_PREFIX "shortened map time %i minutes.", pszCommandPlayerName, iExtendTime * -1);
	else
		ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX ADMIN_PREFIX "extended map time %i minutes.", pszCommandPlayerName, iExtendTime);
}

CON_COMMAND_CHAT_FLAGS(pm, "<name> <message> - Private message a player. This will also show to all online admins", ADMFLAG_GENERIC)
{
	if (args.ArgC() < 3)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Usage: /pm <name> <message>");
		return;
	}

	if (player)
	{
		ZEPlayer* ply = player->GetZEPlayer();
		if (!ply)
			return;
		if (ply->IsGagged())
		{
			ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "You may not private message players while gagged.");
			return;
		}
	}

	int iCommandPlayer = player ? player->GetPlayerSlot() : -1;
	int iNumClients = 0;
	int pSlot[MAXPLAYERS];

	if (g_playerManager->TargetPlayerString(iCommandPlayer, args[1], iNumClients, pSlot) > ETargetType::SELF)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "You can only private message individual players.");
		return;
	}

	if (!iNumClients)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Target not found.");
		return;
	}

	if (iNumClients > 1)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "More than one client matched.");
		return;
	}

	CCSPlayerController* pTarget = CCSPlayerController::FromSlot(pSlot[0]);
	if (!pTarget)
		return;

	ZEPlayer* pTargetPlayer = pTarget->GetZEPlayer();

	std::string strMessage = GetReason(args, 1, false);

	const char* pszName = player ? player->GetPlayerName() : "CONSOLE";

	if (player == pTarget)
	{
	//Player is PMing themselves (bind to display message in chat probably), so no need to echo to all admins
		ClientPrint(player, HUD_PRINTTALK, "\x0A[SELF]\x0C %s\1: \x0B%s", pszName, strMessage.c_str());
		return;
	}

	for (int i = 0; i < gpGlobals->maxClients; i++)
	{
		ZEPlayer* pPlayer = g_playerManager->GetPlayer(i);

		if (!pPlayer || pTargetPlayer == pPlayer)
			continue;

		if (pPlayer->IsAdminFlagSet(ADMFLAG_GENERIC) && CCSPlayerController::FromSlot(i) != player)
			ClientPrint(CCSPlayerController::FromSlot(i), HUD_PRINTTALK, "\x0A[PM to %s]\x0C %s\1: \x0B%s", pTarget->GetPlayerName(), pszName, strMessage.c_str());
	}

	ClientPrint(player, HUD_PRINTTALK, "\x0A[PM to %s]\x0C %s\1: \x0B%s", pTarget->GetPlayerName(), pszName, strMessage.c_str());
	ClientPrint(pTarget, HUD_PRINTTALK, "\x0A[PM]\x0C %s\1: \x0B%s", pszName, strMessage.c_str());
	Message("[PM to %s] %s: %s\n", pTarget->GetPlayerName(), pszName, strMessage.c_str());
}

CON_COMMAND_CHAT_FLAGS(who, "- List the flags of all online players", ADMFLAG_GENERIC)
{
	std::vector<std::tuple<std::string, std::string, uint64>> rgNameSlotID;

	for (size_t i = 0; i < gpGlobals->maxClients; i++)
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
		else if (strName.length() > 20)
			strName = strName.substr(0, 17) + "...";

		if (pPlayer->IsFakeClient())
		{
			rgNameSlotID.push_back(std::tuple<std::string, std::string, uint64>(strName, "BOT", 0));
			continue;
		}

		uint64 iSteamID = pPlayer->IsAuthenticated() ? pPlayer->GetSteamId64() : pPlayer->GetUnauthenticatedSteamId64();
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
				strFlags = "NONE";
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

	ClientPrint(player, HUD_PRINTCONSOLE, "c_who output: %i client%s", rgNameSlotID.size(), rgNameSlotID.size() == 1 ? "" : "s");
	ClientPrint(player, HUD_PRINTCONSOLE, "|----------------------|----------------------------------------------------|-------------------|");
	ClientPrint(player, HUD_PRINTCONSOLE, "|         Name         |                       Flags                        |    Steam64 ID     |");
	ClientPrint(player, HUD_PRINTCONSOLE, "|----------------------|----------------------------------------------------|-------------------|");
	for (auto [strPlayerName, strFlags, iSteamID] : rgNameSlotID)
	{

		if (strPlayerName.length() % 2 == 1)
			strPlayerName = strPlayerName + ' ';
		if (strPlayerName.length() < 20)
			strPlayerName = std::string((20 - strPlayerName.length()) / 2, ' ') + strPlayerName + std::string((20 - strPlayerName.length()) / 2, ' ');

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
	ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Check console for output.");
}

CON_COMMAND_CHAT(status, "<name> - Checks a player's active punishments. Non-admins may only check their own punishments")
{
	int iCommandPlayer = player ? player->GetPlayerSlot() : -1;
	int iNumClients;
	int pSlot[MAXPLAYERS];
	ETargetType nType = ETargetType::SELF;
	ZEPlayer* pTargetPlayer = nullptr;
	bool bIsAdmin = iCommandPlayer == -1 || g_playerManager->GetPlayer(iCommandPlayer)->IsAdminFlagSet(ADMFLAG_GENERIC);
	std::string target = !bIsAdmin || args.ArgC() == 1 ? "" : args[1];

	if (bIsAdmin && target.length() > 0)
	{
		iNumClients = 0;
		target = args[1];
		nType = g_playerManager->TargetPlayerString(iCommandPlayer, args[1], iNumClients, pSlot);
	}
	else
	{
		iNumClients = 1;
		pSlot[0] = iCommandPlayer;
	}

	if (iNumClients > 1)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "More than one client matched.");
		return;
	}
	else if (iNumClients <= 0)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Target not found.");
		return;
	}

	CCSPlayerController* pTarget = CCSPlayerController::FromSlot(pSlot[0]);
	if (!pTarget)
		return;

	pTargetPlayer = g_playerManager->GetPlayer(pSlot[0]);

	if (pTargetPlayer->IsFakeClient())
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Cannot target bot.");
		return;
	}
		
	if (!pTargetPlayer->IsMuted() && !pTargetPlayer->IsGagged())
	{
		if (target.length() == 0)
			ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "You have no active punishments.");
		else
			ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "%s has no active punishments.", pTarget->GetPlayerName());
		return;
	}

	std::string punishment = "";
	if (pTargetPlayer->IsMuted() && pTargetPlayer->IsGagged())
		punishment = "\2gagged\1 and \2muted\1";
	else if (pTargetPlayer->IsMuted())
		punishment = "\2muted\1";
	else if (pTargetPlayer->IsGagged())
		punishment = "\2gagged\1";

	ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "%s %s.",
				target.length() == 0 ? "You are" : (target + " is").c_str(), punishment.c_str());
}

CON_COMMAND_CHAT_FLAGS(listdc, "- List recently disconnected players and their Steam64 IDs", ADMFLAG_GENERIC)
{
	g_pAdminSystem->ShowDisconnectedPlayers(player);
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
#endif

CAdminSystem::CAdminSystem()
{
	LoadAdmins();
	LoadInfractions();

	// Fill out disconnected player list with empty objects which we overwrite as players leave
	for (int i = 0; i < 20; i++)
		m_rgDCPly[i] = std::tuple<std::string, uint64, std::string>("", 0, "");
	m_iDCPlyIndex = 0;
}

bool CAdminSystem::LoadAdmins()
{
	m_vecAdmins.Purge();
	KeyValues* pKV = new KeyValues("admins");
	KeyValues::AutoDelete autoDelete(pKV);

	const char *pszPath = "addons/cs2fixes/configs/admins.cfg";

	if (!pKV->LoadFromFile(g_pFullFileSystem, pszPath))
	{
		Warning("Failed to load %s\n", pszPath);
		return false;
	}
	for (KeyValues* pKey = pKV->GetFirstSubKey(); pKey; pKey = pKey->GetNextKey())
	{
		const char *pszName = pKey->GetName();
		const char *pszSteamID = pKey->GetString("steamid", nullptr);
		const char *pszFlags = pKey->GetString("flags", nullptr);

		if (!pszSteamID)
		{
			Warning("Admin entry %s is missing 'steam' key\n", pszName);
			return false;
		}

		if (!pszFlags)
		{
			Warning("Admin entry %s is missing 'flags' key\n", pszName);
			return false;
		}

		ConMsg("Loaded admin %s\n", pszName);
		ConMsg(" - Steam ID %5s\n", pszSteamID);
		ConMsg(" - Flags %5s\n", pszFlags);

		uint64 iFlags = ParseFlags(pszFlags);

		// Let's just use steamID64 for now
		m_vecAdmins.AddToTail(CAdmin(pszName, atoll(pszSteamID), iFlags));
	}

	return true;
}

bool CAdminSystem::LoadInfractions()
{
	m_vecInfractions.PurgeAndDeleteElements();
	KeyValues* pKV = new KeyValues("infractions");
	KeyValues::AutoDelete autoDelete(pKV);

	const char *pszPath = "addons/cs2fixes/data/infractions.txt";

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
bool CAdminSystem::ApplyInfractions(ZEPlayer *player)
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

bool CAdminSystem::FindAndRemoveInfraction(ZEPlayer *player, CInfractionBase::EInfractionType type)
{
	FOR_EACH_VEC(m_vecInfractions, i)
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
	FOR_EACH_VEC(m_vecInfractions, i)
	{
		if (m_vecInfractions[i]->GetSteamId64() == steamid64 && m_vecInfractions[i]->GetType() == type)
		{
			m_vecInfractions.Remove(i);
			
			return true;
		}
	}

	return false;
}

CAdmin *CAdminSystem::FindAdmin(uint64 iSteamID)
{
	FOR_EACH_VEC(m_vecAdmins, i)
	{
		if (m_vecAdmins[i].GetSteamID() == iSteamID)
			return &m_vecAdmins[i];
	}

	return nullptr;
}

uint64 CAdminSystem::ParseFlags(const char* pszFlags)
{
	uint64 flags = 0;
	size_t length = V_strlen(pszFlags);

	for (size_t i = 0; i < length; i++)
	{
		char c = tolower(pszFlags[i]);
		if (c < 'a' || c > 'z')
			continue;

		if (c == 'z')
			return -1; // all flags

		flags |= ((uint64)1 << (c - 'a'));
	}

	return flags;
}

void CAdminSystem::AddDisconnectedPlayer(const char* pszName, uint64 xuid, const char* pszIP)
{
	auto plyInfo = std::make_tuple(pszName, xuid, pszIP);
	for (auto& dcPlyInfo : m_rgDCPly)
	{
		if (std::get<1>(dcPlyInfo) == std::get<1>(plyInfo))
			return;
	}
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
					ClientPrint(pAdmin, HUD_PRINTTALK, CHAT_PREFIX "Disconnected player(s) displayed in console.");
				ClientPrint(pAdmin, HUD_PRINTCONSOLE, "Disconnected Player(s):");
				bAnyDCedPlayers = true;
			}

			std::string strTemp = std::get<0>(ply) + "\n\tSteam64 ID - " + std::to_string(std::get<1>(ply)) + "\n\tIP Address - " + std::get<2>(ply);
			ClientPrint(pAdmin, HUD_PRINTCONSOLE, "%i. %s", i, strTemp.c_str());
		}
	}
	if (!bAnyDCedPlayers)
		ClientPrint(pAdmin, HUD_PRINTTALK, CHAT_PREFIX "No players have disconnected yet.");
}

void CBanInfraction::ApplyInfraction(ZEPlayer *player)
{
	g_pEngineServer2->DisconnectClient(player->GetPlayerSlot(), NETWORK_DISCONNECT_KICKBANADDED); // "Kicked and banned"
}

void CMuteInfraction::ApplyInfraction(ZEPlayer* player)
{
	player->SetMuted(true);
}

void CMuteInfraction::UndoInfraction(ZEPlayer *player)
{
	player->SetMuted(false);
}

void CGagInfraction::ApplyInfraction(ZEPlayer *player)
{
	player->SetGagged(true);
}

void CGagInfraction::UndoInfraction(ZEPlayer *player)
{
	player->SetGagged(false);
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

int ParseTimeInput(std::string strTime)
{
	if (strTime.length() == 0 || std::find_if(strTime.begin(), strTime.end(), [](char c) { return c == '-'; }) != strTime.end())
		return -1;

	std::string strNumbers = "";
	std::copy_if(strTime.begin(), strTime.end(), std::back_inserter(strNumbers), [](char c) { return std::isdigit(c); });

	if (strNumbers.length() == 0)
		return -1;
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
	std::string strReason = args.ArgS();

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
		std::copy_if(strReason.cbegin(), strReason.cend(), std::back_inserter(strOutput), [](unsigned char c) {return c < 128; });
	else
		strOutput = strReason;

	// Clean up both ends of string very inefficiently...
	while (strOutput.length() > 0 && (strOutput.at(0) == ' ' || strOutput.at(0) == '\"'))
		strOutput = strOutput.substr(1);
	while (strOutput.length() > 0 && (strOutput.at(strOutput.length() - 1) == ' ' || strOutput.at(strOutput.length() - 1) == '\"'))
		strOutput = strOutput.substr(0, strOutput.length() - 1);

	return strOutput;
}