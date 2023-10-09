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

extern IVEngineServer2 *g_pEngineServer2;
extern CEntitySystem *g_pEntitySystem;

CAdminSystem* g_pAdminSystem;

CUtlMap<uint32, FnChatCommandCallback_t> g_CommandList(0, 0, DefLessFunc(uint32));

#define ADMIN_PREFIX "Admin %s has "

CON_COMMAND_F(c_reload_admins, "Reload admin config", FCVAR_SPONLY | FCVAR_LINKED_CONCOMMAND)
{
	if (!g_pAdminSystem->LoadAdmins())
		return;

	for (int i = 0; i < MAXPLAYERS; i++)
	{
		ZEPlayer* pPlayer = g_playerManager->GetPlayer(i);

		if (!pPlayer || pPlayer->IsFakeClient())
			continue;

		pPlayer->CheckAdmin();
	}

	Message("Admins reloaded\n");
}

CON_COMMAND_F(c_reload_infractions, "Reload infractions file", FCVAR_SPONLY | FCVAR_LINKED_CONCOMMAND)
{
	if (!g_pAdminSystem->LoadInfractions())
		return;

	for (int i = 0; i < MAXPLAYERS; i++)
	{
		ZEPlayer* pPlayer = g_playerManager->GetPlayer(i);

		if (!pPlayer || pPlayer->IsFakeClient())
			continue;

		pPlayer->CheckInfractions();
	}

	Message("Infractions reloaded\n");
}

CON_COMMAND_CHAT(ban, "ban a player")
{
	if (!player)
		return;

	int iCommandPlayer = player->GetPlayerSlot();

	ZEPlayer *pPlayer = g_playerManager->GetPlayer(iCommandPlayer);

	if (!pPlayer->IsAdminFlagSet(ADMFLAG_BAN))
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX"You don't have access to this command.");
		return;
	}

	if (args.ArgC() < 3)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX"Usage: !ban <name> <duration/0 (permanent)>");
		return;
	}

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

	char* end;
	int iDuration = strtol(args[2], &end, 10);

	if (*end)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX"Invalid duration.");
		return;
	}

	CBasePlayerController* pTarget = (CBasePlayerController*)g_pEntitySystem->GetBaseEntity((CEntityIndex)(pSlot[0] + 1));

	if (!pTarget)
		return;

	ZEPlayer* pTargetPlayer = g_playerManager->GetPlayer(pSlot[0]);

	if (pTargetPlayer->IsFakeClient())
		return;

	CInfractionBase *infraction = new CBanInfraction(iDuration, pTargetPlayer->GetSteamId64());

	g_pAdminSystem->AddInfraction(infraction);
	infraction->ApplyInfraction(pTargetPlayer);
	g_pAdminSystem->SaveInfractions();

	ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX ADMIN_PREFIX "banned %s for %i minutes.", player->GetPlayerName(), pTarget->GetPlayerName(), iDuration);
}

CON_COMMAND_CHAT(mute, "mutes a player")
{
	if (!player)
		return;

	int iCommandPlayer = player->GetPlayerSlot();

	ZEPlayer* pPlayer = g_playerManager->GetPlayer(player->GetPlayerSlot());

	if (!pPlayer->IsAdminFlagSet(ADMFLAG_BAN))
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX"You don't have access to this command.");
		return;
	}

	if (args.ArgC() < 3)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX"Usage: !mute <name> <duration/0 (permanent)>");
		return;
	}

	int iNumClients = 0;
	int pSlot[MAXPLAYERS];

	ETargetType nType = g_playerManager->TargetPlayerString(iCommandPlayer, args[1], iNumClients, pSlot);

	if (!iNumClients)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX"Target not found.");
		return;
	}

	char* end;
	int iDuration = strtol(args[2], &end, 10);

	if (*end)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX"Invalid duration.");
		return;
	}

	for (int i = 0; i < iNumClients; i++)
	{
		CBasePlayerController* pTarget = (CBasePlayerController*)g_pEntitySystem->GetBaseEntity((CEntityIndex)(pSlot[i] + 1));

		if (!pTarget)
			continue;

		ZEPlayer* pTargetPlayer = g_playerManager->GetPlayer(pSlot[i]);

		if (pTargetPlayer->IsFakeClient())
			continue;

		CInfractionBase* infraction = new CMuteInfraction(iDuration, pTargetPlayer->GetSteamId64());

		// We're overwriting the infraction, so remove the previous one first
		g_pAdminSystem->FindAndRemoveInfraction(pTargetPlayer, CInfractionBase::Mute);
		g_pAdminSystem->AddInfraction(infraction);
		infraction->ApplyInfraction(pTargetPlayer);
		g_pAdminSystem->SaveInfractions();

		if (nType < ETargetType::ALL)
			ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX ADMIN_PREFIX "muted %s for %i minutes.", player->GetPlayerName(), pTarget->GetPlayerName(), iDuration);
	}

	g_pAdminSystem->SaveInfractions();

	switch (nType)
	{
	case ETargetType::ALL:
		ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX ADMIN_PREFIX "muted everyone for %i minutes.", player->GetPlayerName(), iDuration);
		break;
	case ETargetType::T:
		ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX ADMIN_PREFIX "muted terrorists for %i minutes.", player->GetPlayerName(), iDuration);
		break;
	case ETargetType::CT:
		ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX ADMIN_PREFIX "muted counter-terrorists for %i minutes.", player->GetPlayerName(), iDuration);
		break;
	}
}

CON_COMMAND_CHAT(unmute, "unmutes a player")
{
	if (!player)
		return;

	int iCommandPlayer = player->GetPlayerSlot();

	ZEPlayer *pPlayer = g_playerManager->GetPlayer(player->GetPlayerSlot());

	if (!pPlayer->IsAdminFlagSet(ADMFLAG_UNBAN))
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "You don't have access to this command.");
		return;
	}

	if (args.ArgC() < 2)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Usage: !unmute <name>");
		return;
	}

	int iNumClients = 0;
	int pSlot[MAXPLAYERS];

	ETargetType nType = g_playerManager->TargetPlayerString(iCommandPlayer, args[1], iNumClients, pSlot);

	if (!iNumClients)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Target not found.");
		return;
	}

	for (int i = 0; i < iNumClients; i++)
	{
		CBasePlayerController *pTarget = (CBasePlayerController *)g_pEntitySystem->GetBaseEntity((CEntityIndex)(pSlot[i] + 1));

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
			ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX ADMIN_PREFIX "unmuted %s.", player->GetPlayerName(), pTarget->GetPlayerName());
	}

	g_pAdminSystem->SaveInfractions();

	switch (nType)
	{
	case ETargetType::ALL:
		ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX ADMIN_PREFIX "unmuted everyone.", player->GetPlayerName());
		break;
	case ETargetType::T:
		ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX ADMIN_PREFIX "unmuted terrorists.", player->GetPlayerName());
		break;
	case ETargetType::CT:
		ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX ADMIN_PREFIX "unmuted counter-terrorists.", player->GetPlayerName());
		break;
	}
}

CON_COMMAND_CHAT(gag, "gag a player")
{
	if (!player)
		return;

	int iCommandPlayer = player->GetPlayerSlot();

	ZEPlayer *pPlayer = g_playerManager->GetPlayer(player->GetPlayerSlot());

	if (!pPlayer->IsAdminFlagSet(ADMFLAG_BAN))
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "You don't have access to this command.");
		return;
	}

	if (args.ArgC() < 3)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Usage: !gag <name> <duration/0 (permanent)>");
		return;
	}

	int iNumClients = 0;
	int pSlot[MAXPLAYERS];

	ETargetType nType = g_playerManager->TargetPlayerString(iCommandPlayer, args[1], iNumClients, pSlot);

	if (!iNumClients)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Target not found.");
		return;
	}

	char *end;
	int iDuration = strtol(args[2], &end, 10);

	if (*end)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Invalid duration.");
		return;
	}

	for (int i = 0; i < iNumClients; i++)
	{
		CBasePlayerController *pTarget = (CBasePlayerController *)g_pEntitySystem->GetBaseEntity((CEntityIndex)(pSlot[i] + 1));

		if (!pTarget)
			continue;

		ZEPlayer *pTargetPlayer = g_playerManager->GetPlayer(pSlot[i]);

		if (pTargetPlayer->IsFakeClient())
			continue;

		CInfractionBase *infraction = new CGagInfraction(iDuration, pTargetPlayer->GetSteamId64());

		// We're overwriting the infraction, so remove the previous one first
		g_pAdminSystem->FindAndRemoveInfraction(pTargetPlayer, CInfractionBase::Gag);
		g_pAdminSystem->AddInfraction(infraction);
		infraction->ApplyInfraction(pTargetPlayer);

		if (nType < ETargetType::ALL)
			ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX ADMIN_PREFIX "gagged %s for %i minutes.", player->GetPlayerName(), pTarget->GetPlayerName(), iDuration);
	}

	g_pAdminSystem->SaveInfractions();

	switch (nType)
	{
	case ETargetType::ALL:
		ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX ADMIN_PREFIX "gagged everyone for %i minutes.", player->GetPlayerName(), iDuration);
		break;
	case ETargetType::T:
		ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX ADMIN_PREFIX "gagged terrorists for %i minutes.", player->GetPlayerName(), iDuration);
		break;
	case ETargetType::CT:
		ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX ADMIN_PREFIX "gagged counter-terrorists for %i minutes.", player->GetPlayerName(), iDuration);
		break;
	}
}

CON_COMMAND_CHAT(ungag, "ungags a player")
{
	if (!player)
		return;

	int iCommandPlayer = player->GetPlayerSlot();

	ZEPlayer *pPlayer = g_playerManager->GetPlayer(player->GetPlayerSlot());

	if (!pPlayer->IsAdminFlagSet(ADMFLAG_UNBAN))
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "You don't have access to this command.");
		return;
	}

	if (args.ArgC() < 2)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Usage: !ungag <name>");
		return;
	}

	int iNumClients = 0;
	int pSlot[MAXPLAYERS];

	ETargetType nType = g_playerManager->TargetPlayerString(iCommandPlayer, args[1], iNumClients, pSlot);

	if (!iNumClients)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Target not found.");
		return;
	}

	for (int i = 0; i < iNumClients; i++)
	{
		CBasePlayerController *pTarget = (CBasePlayerController *)g_pEntitySystem->GetBaseEntity((CEntityIndex)(pSlot[i] + 1));

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
			ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX ADMIN_PREFIX "ungagged %s.", player->GetPlayerName(), pTarget->GetPlayerName());
	}

	g_pAdminSystem->SaveInfractions();

	switch (nType)
	{
	case ETargetType::ALL:
		ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX ADMIN_PREFIX "ungagged everyone.", player->GetPlayerName());
		break;
	case ETargetType::T:
		ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX ADMIN_PREFIX "ungagged terrorists.", player->GetPlayerName());
		break;
	case ETargetType::CT:
		ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX ADMIN_PREFIX "ungagged counter-terrorists.", player->GetPlayerName());
		break;
	}
}

CON_COMMAND_CHAT(kick, "kick a player")
{
	if (!player)
		return;

	int iCommandPlayer = player->GetPlayerSlot();

	ZEPlayer* pPlayer = g_playerManager->GetPlayer(player->GetPlayerSlot());

	if (!pPlayer->IsAdminFlagSet(ADMFLAG_KICK))
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX"You don't have access to this command.");
		return;
	}

	if (args.ArgC() < 2)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX"Usage: !kick <name>");
		return;
	}

	int iNumClients = 0;
	int pSlot[MAXPLAYERS];

	g_playerManager->TargetPlayerString(iCommandPlayer, args[1], iNumClients, pSlot);

	if (!iNumClients)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX"Target not found.");
		return;
	}

	for (int i = 0; i < iNumClients; i++)
	{
		CBasePlayerController* pTarget = (CBasePlayerController*)g_pEntitySystem->GetBaseEntity((CEntityIndex)(pSlot[i] + 1));

		if (!pTarget)
			continue;

		ZEPlayer* pTargetPlayer = g_playerManager->GetPlayer(pSlot[i]);
		
		g_pEngineServer2->DisconnectClient(pTargetPlayer->GetPlayerSlot(), NETWORK_DISCONNECT_KICKED);

		ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX ADMIN_PREFIX "kicked %s.", player->GetPlayerName(), pTarget->GetPlayerName());
	}
}

CON_COMMAND_CHAT(slay, "slay a player")
{
	if (!player)
		return;

	int iCommandPlayer = player->GetPlayerSlot();

	ZEPlayer *pPlayer = g_playerManager->GetPlayer(player->GetPlayerSlot());

	if (!pPlayer->IsAdminFlagSet(ADMFLAG_SLAY))
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX"You don't have access to this command.");
		return;
	}

	if (args.ArgC() < 2)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX"Usage: !slay <name>");
		return;
	}

	int iNumClients = 0;
	int pSlots[MAXPLAYERS];

	ETargetType nType = g_playerManager->TargetPlayerString(iCommandPlayer, args[1], iNumClients, pSlots);

	if (!iNumClients)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX"Target not found.");
		return;
	}

	for (int i = 0; i < iNumClients; i++)
	{
		CBasePlayerController *pTarget = (CBasePlayerController *)g_pEntitySystem->GetBaseEntity((CEntityIndex)(pSlots[i] + 1));

		if (!pTarget)
			continue;

		pTarget->GetPawn()->CommitSuicide(false, true);

		if (nType < ETargetType::ALL)
			ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX ADMIN_PREFIX "slayed %s.", player->GetPlayerName(), pTarget->GetPlayerName());
	}

	switch (nType)
	{
	case ETargetType::ALL:
		ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX ADMIN_PREFIX "slayed everyone.", player->GetPlayerName());
		break;
	case ETargetType::T:
		ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX ADMIN_PREFIX "slayed terrorists.", player->GetPlayerName());
		break;
	case ETargetType::CT:
		ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX ADMIN_PREFIX "slayed counter-terrorists.", player->GetPlayerName());
		break;
	}
}

CON_COMMAND_CHAT(goto, "teleport to a player")
{
	if (!player)
		return;

	int iCommandPlayer = player->GetPlayerSlot();

	ZEPlayer *pPlayer = g_playerManager->GetPlayer(player->GetPlayerSlot());

	if (!pPlayer->IsAdminFlagSet(ADMFLAG_SLAY))
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "You don't have access to this command.");
		return;
	}

	if (args.ArgC() < 2)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Usage: !goto <name>");
		return;
	}

	int iNumClients = 0;
	int pSlots[MAXPLAYERS];

	if (g_playerManager->TargetPlayerString(iCommandPlayer, args[1], iNumClients, pSlots) != ETargetType::PLAYER || iNumClients > 1)
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
		CBasePlayerController *pTarget = (CBasePlayerController *)g_pEntitySystem->GetBaseEntity((CEntityIndex)(pSlots[i] + 1));

		if (!pTarget)
			continue;

		Vector newOrigin = pTarget->GetPawn()->GetAbsOrigin();

		player->GetPawn()->Teleport(&newOrigin, nullptr, nullptr);

		ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX ADMIN_PREFIX "Teleported to %s.", pTarget->GetPlayerName());
	}
}

CON_COMMAND_CHAT(bring, "bring a player")
{
	if (!player)
		return;

	int iCommandPlayer = player->GetPlayerSlot();

	ZEPlayer *pPlayer = g_playerManager->GetPlayer(player->GetPlayerSlot());

	if (!pPlayer->IsAdminFlagSet(ADMFLAG_SLAY))
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "You don't have access to this command.");
		return;
	}

	if (args.ArgC() < 2)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Usage: !bring <name>");
		return;
	}

	int iNumClients = 0;
	int pSlots[MAXPLAYERS];

	ETargetType nType = g_playerManager->TargetPlayerString(iCommandPlayer, args[1], iNumClients, pSlots);

	if (!iNumClients)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Target not found.");
		return;
	}

	for (int i = 0; i < iNumClients; i++)
	{
		CBasePlayerController *pTarget = (CBasePlayerController *)g_pEntitySystem->GetBaseEntity((CEntityIndex)(pSlots[i] + 1));

		if (!pTarget)
			continue;

		Vector newOrigin = player->GetPawn()->GetAbsOrigin();

		pTarget->GetPawn()->Teleport(&newOrigin, nullptr, nullptr);

		if (nType < ETargetType::ALL)
			ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX ADMIN_PREFIX "brought %s.", player->GetPlayerName(), pTarget->GetPlayerName());
	}

	switch (nType)
	{
	case ETargetType::ALL:
		ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX ADMIN_PREFIX "brought everyone.", player->GetPlayerName());
		break;
	case ETargetType::T:
		ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX ADMIN_PREFIX "brought terrorists.", player->GetPlayerName());
		break;
	case ETargetType::CT:
		ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX ADMIN_PREFIX "brought counter-terrorists.", player->GetPlayerName());
		break;
	}
}

CON_COMMAND_CHAT(map, "change map")
{
	if (!player)
		return;

	int iCommandPlayer = player->GetPlayerSlot();

	ZEPlayer *pPlayer = g_playerManager->GetPlayer(iCommandPlayer);

	if (!pPlayer->IsAdminFlagSet(ADMFLAG_CHANGEMAP))
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX"You don't have access to this command.");
		return;
	}

	if (args.ArgC() < 2)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX"Usage: !map <mapname>");
		return;
	}

	if (!g_pEngineServer2->IsMapValid(args[1]))
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX"Invalid map specified.");
		return;
	}

	ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX"Changing map to %s...", args[1]);

	char buf[MAX_PATH];
	V_snprintf(buf, sizeof(buf), "changelevel %s", args[1]);

	new CTimer(5.0f, false, false, [buf]()
	{
		g_pEngineServer2->ServerCommand(buf);
	});
}

CON_COMMAND_CHAT(hsay, "say something as a hud hint")
{
	if (!player)
		return;

	int iCommandPlayer = player->GetPlayerSlot();

	ZEPlayer *pPlayer = g_playerManager->GetPlayer(iCommandPlayer);

	if (!pPlayer->IsAdminFlagSet(ADMFLAG_CHAT))
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "You don't have access to this command.");
		return;
	}

	if (args.ArgC() < 2)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Usage: !hsay <message>");
		return;
	}

	ClientPrintAll(HUD_PRINTCENTER, "%s", args.ArgS());
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
		int iEndTime = pKey->GetInt("endtime", -1);
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
			AddInfraction(new CBanInfraction(iEndTime, iSteamId));
			break;
		case CInfractionBase::Mute:
			AddInfraction(new CMuteInfraction(iEndTime, iSteamId));
			break;
		case CInfractionBase::Gag:
			AddInfraction(new CGagInfraction(iEndTime, iSteamId));
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
		int timestamp = m_vecInfractions[i]->GetTimestamp();
		if (timestamp != 0 && timestamp < std::time(0))
			continue;

		char buf[5];
		V_snprintf(buf, sizeof(buf), "%d", i);
		pSubKey = new KeyValues(buf);
		pSubKey->AddUint64("steamid", m_vecInfractions[i]->GetSteamId64());
		pSubKey->AddInt("endtime", m_vecInfractions[i]->GetTimestamp());
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

void CAdminSystem::ApplyInfractions(ZEPlayer *player)
{
	FOR_EACH_VEC(m_vecInfractions, i)
	{
		// Undo the infraction just briefly while checking if it ran out
		if (m_vecInfractions[i]->GetSteamId64() == player->GetSteamId64())
			m_vecInfractions[i]->UndoInfraction(player);

		int timestamp = m_vecInfractions[i]->GetTimestamp();
		if (timestamp != 0 && timestamp <= std::time(0))
		{
			m_vecInfractions.Remove(i);
			continue;
		}

		if (m_vecInfractions[i]->GetSteamId64() == player->GetSteamId64())
			m_vecInfractions[i]->ApplyInfraction(player);
	}
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
	g_pEngineServer2->DisconnectClient(player->GetPlayerSlot().Get(), NETWORK_DISCONNECT_REJECT_BANNED); // "Kicked and banned"
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