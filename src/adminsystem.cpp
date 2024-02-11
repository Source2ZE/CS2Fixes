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

#include "protobuf/generated/usermessages.pb.h"

#include "adminsystem.h"
#include "KeyValues.h"
#include "interfaces/interfaces.h"
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
#include <vector>

extern IVEngineServer2 *g_pEngineServer2;
extern CGameEntitySystem *g_pEntitySystem;
extern CGlobalVars *gpGlobals;
extern CCSGameRules *g_pGameRules;

CAdminSystem* g_pAdminSystem = nullptr;

CUtlMap<uint32, CChatCommand *> g_CommandList(0, 0, DefLessFunc(uint32));

static std::string g_sBeaconParticle = "particles/testsystems/test_cross_product.vpcf";

FAKE_STRING_CVAR(cs2f_admin_beacon_particle, ".vpcf file to be precached and used for admin beacon", g_sBeaconParticle, false)

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

	if (g_playerManager->TargetPlayerString(iCommandPlayer, args[1], iNumClients, pSlot) != ETargetType::PLAYER || iNumClients > 1)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX"You can only target individual players for banning.");
		return;
	}

	if (!iNumClients)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX"Target not found.");
		return;
	}

	int iDuration = V_StringToInt32(args[2], -1);

	if (iDuration == -1)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX"Invalid duration.");
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

	ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX ADMIN_PREFIX "banned %s for %i minutes.", pszCommandPlayerName, pTarget->GetPlayerName(), iDuration);

	if (iDuration > 0)
	{
		char szAction[64];
		V_snprintf(szAction, sizeof(szAction), " for %i minutes", iDuration);
		PrintSingleAdminAction(pszCommandPlayerName, pTarget->GetPlayerName(), "banned", szAction);
	}
	else
	{
		PrintSingleAdminAction(pszCommandPlayerName, pTarget->GetPlayerName(), "permanently banned");
	}
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

	int iDuration = V_StringToInt32(args[2], -1);

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

	char szAction[64];
	V_snprintf(szAction, sizeof(szAction), " for %i minutes", iDuration);

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
			PrintSingleAdminAction(pszCommandPlayerName, pTarget->GetPlayerName(), "muted", szAction);
		else
			PrintSingleAdminAction(pszCommandPlayerName, pTarget->GetPlayerName(), "permanently muted");
	}

	g_pAdminSystem->SaveInfractions();

	PrintMultiAdminAction(nType, pszCommandPlayerName, "muted", szAction);
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

	int iDuration = V_StringToInt32(args[2], -1);

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

	char szAction[64];
	V_snprintf(szAction, sizeof(szAction), " for %i minutes", iDuration);

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
			PrintSingleAdminAction(pszCommandPlayerName, pTarget->GetPlayerName(), "gagged", szAction);
		else
			PrintSingleAdminAction(pszCommandPlayerName, pTarget->GetPlayerName(), "permanently gagged");
	}

	g_pAdminSystem->SaveInfractions();

	PrintMultiAdminAction(nType, pszCommandPlayerName, "gagged", szAction);
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

	g_playerManager->TargetPlayerString(iCommandPlayer, args[1], iNumClients, pSlot);

	if (!iNumClients)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX"Target not found.");
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
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX"Target not found.");
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

	const char *pszCommandPlayerName = player ? player->GetPlayerName() : "Console";

	for (int i = 0; i < iNumClients; i++)
	{
		CBasePlayerController *pTarget = (CBasePlayerController *)g_pEntitySystem->GetBaseEntity((CEntityIndex)(pSlots[i] + 1));

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

		int iDamage = V_StringToInt32(args[2], 0);
			
		if (iDamage > 0)
			pPawn->TakeDamage(iDamage);

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

	if (g_playerManager->TargetPlayerString(player->GetPlayerSlot(), args[1], iNumClients, pSlots) != ETargetType::PLAYER || iNumClients > 1)
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

	Z_CBaseEntity *pTarget = nullptr;

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
		while (pTarget = UTIL_FindEntityByName(pTarget, args[1], player))
		{
			pTarget->AcceptInput(args[2], args[3], player, player);
			iFoundEnts++;
		}
	}

	if (!iFoundEnts)
	{
		while (pTarget = UTIL_FindEntityByClassname(pTarget, args[1]))
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
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX"Usage: !map <mapname>");
		return;
	}

	if (!g_pEngineServer2->IsMapValid(args[1]))
	{
		// This might be a workshop map, and right now there's no easy way to get the list from a collection
		// So blindly attempt the change for now, as the command does nothing if the map isn't found
		std::string sCommand = "ds_workshop_changelevel " + std::string(args[1]);

		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Attempting a map change to %s from the workshop collection...", args[1]);
		ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX "Changing map to %s...", args[1]);

		new CTimer(5.0f, false, [sCommand]()
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

	new CTimer(5.0f, false, [szMapName]()
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

void PrecacheAdminBeaconParticle(IEntityResourceManifest* pResourceManifest)
{
	pResourceManifest->AddResource(g_sBeaconParticle.c_str());
}

void KillBeacon(int playerSlot)
{
	ZEPlayer* pPlayer = g_playerManager->GetPlayer(playerSlot);

	if (!pPlayer)
		return;

	CParticleSystem* pParticle = pPlayer->GetBeaconParticle();

	if (!pParticle)
		return;

	pParticle->AcceptInput("DestroyImmediately");

	// delayed Kill because default particle is being silly and remains floating if not Destroyed first
	CHandle<CParticleSystem> hParticle = pParticle->GetHandle();
	new CTimer(0.02f, false, [hParticle]()
	{
		CParticleSystem* particle = hParticle.Get();
		if (particle)
			particle->AcceptInput("Kill");
		return -1.0f;
	});
}

void CreateBeacon(int playerSlot)
{
	CCSPlayerController* pTarget = CCSPlayerController::FromSlot(playerSlot);

	Vector vecAbsOrigin = pTarget->GetPawn()->GetAbsOrigin();

	vecAbsOrigin.z += 10;

	CParticleSystem* particle = (CParticleSystem*)CreateEntityByName("info_particle_system");

	CEntityKeyValues* pKeyValues = new CEntityKeyValues();

	pKeyValues->SetString("effect_name", g_sBeaconParticle.c_str());
	pKeyValues->SetInt("tint_cp", 1);
	pKeyValues->SetVector("origin", vecAbsOrigin);
	// ugly angle change because default particle is rotated
	if (strcmp(g_sBeaconParticle.c_str(), "particles/testsystems/test_cross_product.vpcf") == 0)
		pKeyValues->SetQAngle("angles", QAngle(90, 0, 0));
	
	particle->DispatchSpawn(pKeyValues);
	particle->SetParent(pTarget->GetPawn());

	ZEPlayer* pPlayer = g_playerManager->GetPlayer(playerSlot);
	
	pPlayer->SetBeaconParticle(particle);

	CHandle<CParticleSystem> hParticle = particle->GetHandle();

	// timer persists through map change so serial reset on StartupServer is not needed
	new CTimer(0.0f, true, [playerSlot, hParticle]()
	{
		CCSPlayerController* pPlayer = CCSPlayerController::FromSlot(playerSlot);
		
		if (!pPlayer || pPlayer->m_iTeamNum < CS_TEAM_T || !pPlayer->m_hPlayerPawn->IsAlive())
		{
			KillBeacon(playerSlot);
			return -1.0f;
		}

		CParticleSystem* pParticle = hParticle.Get();

		if (!pParticle)
		{
			return -1.0f;
		}

		// team-based tint of Control Point 1
		if (pPlayer->m_iTeamNum == CS_TEAM_T)
			pParticle->m_clrTint->SetColor(185, 93, 63, 255);
		else
			pParticle->m_clrTint->SetColor(40, 100, 255, 255);
		
		pParticle->AcceptInput("Start");
		// delayed DestroyImmediately input so particle effect can be replayed (and default particle doesn't bug out)
		new CTimer(0.5f, false, [hParticle]()
		{
			CParticleSystem* particle = hParticle.Get();
			if (particle)
				particle->AcceptInput("DestroyImmediately");
			return -1.0f;
		});

		return 1.0f;
	});
}

void PerformBeacon(int playerSlot)
{
	ZEPlayer *pPlayer = g_playerManager->GetPlayer(playerSlot);

	if (!pPlayer->GetBeaconParticle())
		CreateBeacon(playerSlot);
	else
		KillBeacon(playerSlot);
}

CON_COMMAND_CHAT_FLAGS(beacon, "Toggle beacon on a player", ADMFLAG_GENERIC)
{
	if (args.ArgC() < 2)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Usage: !beacon <name>");
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

	const char *pszCommandPlayerName = player ? player->GetPlayerName() : "Console";

	for (int i = 0; i < iNumClients; i++)
	{
		CCSPlayerController* pTarget = CCSPlayerController::FromSlot(pSlots[i]);

		if (!pTarget)
			continue;

		PerformBeacon(pSlots[i]);

		if (nType < ETargetType::ALL)
			PrintSingleAdminAction(pszCommandPlayerName, pTarget->GetPlayerName(), "toggled beacon on");
	}

	PrintMultiAdminAction(nType, pszCommandPlayerName, "toggled beacon on");
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

	const char *pszPath = "addons/cs2fixes/data/infractions.txt";

	if (!pKV->SaveToFile(g_pFullFileSystem, pszPath))
		Warning("Failed to save infractions to %s", pszPath);
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