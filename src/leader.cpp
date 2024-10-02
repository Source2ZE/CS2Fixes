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

#include "leader.h"
#include "common.h"
#include "commands.h"
#include "gameevents.pb.h"
#include "zombiereborn.h"
#include "networksystem/inetworkmessages.h"

#include "tier0/memdbgon.h"

extern IVEngineServer2 *g_pEngineServer2;
extern CGameEntitySystem *g_pEntitySystem;
extern CGlobalVars *gpGlobals;
extern IGameEventManager2 *g_gameEventManager;

LeaderColor LeaderColorMap[] = {
	{"white",		Color(255, 255, 255, 255)}, // default if color finding func doesn't match any other color
	{"blue",		Color(40, 100, 255, 255)}, // Default CT color and first leader index
	{"orange",		Color(185, 93, 63, 255)}, // Default T color
	{"green",		Color(100, 230, 100, 255)},
	{"yellow",		Color(200, 200, 0, 255)},
	{"purple",		Color(164, 73, 255, 255)},
	{"red",			Color(214, 39, 40, 255)}, // Last leader index
};

const size_t g_nLeaderColorMapSize = sizeof(LeaderColorMap) / sizeof(LeaderColor);
CUtlVector<ZEPlayerHandle> g_vecLeaders;
int g_iLeaderIndex = 0;

// CONVAR_TODO
bool g_bEnableLeader = false;
static float g_flLeaderVoteRatio = 0.15;
static bool g_bLeaderActionsHumanOnly = true;
static bool g_bMutePingsIfNoLeader = true;
static std::string g_szLeaderModelPath = "";
static int g_iMarkerCount = 0;

FAKE_BOOL_CVAR(cs2f_leader_enable, "Whether to enable Leader features", g_bEnableLeader, false, false)
FAKE_FLOAT_CVAR(cs2f_leader_vote_ratio, "Vote ratio needed for player to become a leader", g_flLeaderVoteRatio, 0.2f, false)
FAKE_BOOL_CVAR(cs2f_leader_actions_ct_only, "Whether to allow leader actions (like !ldbeacon) only from human team", g_bLeaderActionsHumanOnly, true, false)
FAKE_BOOL_CVAR(cs2f_leader_mute_ping_no_leader, "Whether to mute player pings whenever there's no leader", g_bMutePingsIfNoLeader, true, false)
FAKE_STRING_CVAR(cs2f_leader_model_path, "Path to player model to be used for leaders", g_szLeaderModelPath, false)

int Leader_GetNeededLeaderVoteCount()
{
	int iOnlinePlayers = 0;

	for (int i = 0; i < gpGlobals->maxClients; i++)
	{
		ZEPlayer* pPlayer = g_playerManager->GetPlayer(i);

		if (pPlayer && !pPlayer->IsFakeClient())
		{
			iOnlinePlayers++;
		}
	}

	return (int)(iOnlinePlayers * g_flLeaderVoteRatio) + 1;
}

Color Leader_ColorFromString(const char* pszColorName)
{
	int iColorIndex = V_StringToInt32(pszColorName, -1);

	if (iColorIndex > -1)
		return LeaderColorMap[MIN(iColorIndex, g_nLeaderColorMapSize-1)].clColor;

	for (int i = 0; i < g_nLeaderColorMapSize; i++)
	{
		if (!V_stricmp(pszColorName, LeaderColorMap[i].pszColorName))
		{
			return LeaderColorMap[i].clColor;
		}
	}

	return LeaderColorMap[0].clColor;
}

bool Leader_NoLeaders()
{
	int iValidLeaders = 0;
	FOR_EACH_VEC_BACK(g_vecLeaders, i)
	{
		if (g_vecLeaders[i].IsValid())
			iValidLeaders++;
		else
			g_vecLeaders.Remove(i);
	}

	return !((bool)iValidLeaders);
}

void Leader_ApplyLeaderVisuals(CCSPlayerPawn *pPawn)
{
	CCSPlayerController *pController = CCSPlayerController::FromPawn(pPawn);
	ZEPlayer *pPlayer = pController->GetZEPlayer();

	if (!g_szLeaderModelPath.empty())
	{
		pPawn->SetModel(g_szLeaderModelPath.c_str());
		pPawn->AcceptInput("Skin", 0);
	}

	pPawn->m_clrRender = LeaderColorMap[pPlayer->GetLeaderIndex()].clColor;
}

void Leader_RemoveLeaderVisuals(CCSPlayerPawn *pPawn)
{
	g_pZRPlayerClassManager->ApplyPreferredOrDefaultHumanClassVisuals(pPawn);
}

bool Leader_CreateDefendMarker(ZEPlayer *pPlayer, Color clrTint, int iDuration)
{
	CCSPlayerController *pController = CCSPlayerController::FromSlot(pPlayer->GetPlayerSlot());
	CCSPlayerPawn *pPawn = (CCSPlayerPawn *)pController->GetPawn();

	if (g_iMarkerCount >= 5)
	{
		ClientPrint(pController, HUD_PRINTTALK, CHAT_PREFIX "Too many defend markers already active!");
		return false;
	}

	g_iMarkerCount++;

	new CTimer(iDuration, false, false, []()
	{
		if (g_iMarkerCount > 0)
			g_iMarkerCount--;

		return -1.0f;
	});

	CParticleSystem *pMarker = CreateEntityByName<CParticleSystem>("info_particle_system");

	Vector vecOrigin = pPawn->GetAbsOrigin();
	vecOrigin.z += 10;

	CEntityKeyValues* pKeyValues = new CEntityKeyValues();

	pKeyValues->SetString("effect_name", "particles/cs2fixes/leader_defend_mark.vpcf");
	pKeyValues->SetInt("tint_cp", 1);
	pKeyValues->SetColor("tint_cp_color", clrTint);
	pKeyValues->SetVector("origin", vecOrigin);
	pKeyValues->SetBool("start_active", true);

	pMarker->DispatchSpawn(pKeyValues);

	UTIL_AddEntityIOEvent(pMarker, "DestroyImmediately", nullptr, nullptr, "", iDuration);
	UTIL_AddEntityIOEvent(pMarker, "Kill", nullptr, nullptr, "", iDuration + 0.02f);

	return true;
}

void Leader_PostEventAbstract_Source1LegacyGameEvent(const uint64 *clients, const CNetMessage *pData)
{
	if (!g_bEnableLeader)
		return;

	auto pPBData = pData->ToPB<CMsgSource1LegacyGameEvent>();
	
	static int player_ping_id = g_gameEventManager->LookupEventId("player_ping");

	if (pPBData->eventid() != player_ping_id)
		return;

	// Don't kill ping visual when there's no leader, only mute the ping depending on cvar
	if (Leader_NoLeaders())
	{
		if (g_bMutePingsIfNoLeader)
			*(uint64 *)clients = 0;

		return;
	}

	IGameEvent *pEvent = g_gameEventManager->UnserializeEvent(*pPBData);

	ZEPlayer *pPlayer = g_playerManager->GetPlayer(pEvent->GetPlayerSlot("userid"));
	CCSPlayerController *pController = CCSPlayerController::FromSlot(pEvent->GetPlayerSlot("userid"));
	CBaseEntity *pEntity = (CBaseEntity*)g_pEntitySystem->GetEntityInstance(pEvent->GetEntityIndex("entityid"));

	g_gameEventManager->FreeEvent(pEvent);

	// no reason to block zombie pings. sound affected by sound block cvar
	if (pController->m_iTeamNum == CS_TEAM_T)
	{
		if (g_bMutePingsIfNoLeader)
			*(uint64 *)clients = 0;

		return;
	}

	// allow leader human pings
	if (pPlayer->IsLeader())
		return;

	// Remove entity responsible for visual part of the ping
	pEntity->Remove();

	// Block clients from playing the ping sound
	*(uint64 *)clients = 0;
}

void Leader_OnRoundStart(IGameEvent *pEvent)
{
	for (int i = 0; i < gpGlobals->maxClients; i++)
	{
		ZEPlayer *pPlayer = g_playerManager->GetPlayer((CPlayerSlot)i);

		if (pPlayer && !pPlayer->IsLeader())
			pPlayer->SetLeaderTracer(0);
	}

	g_iMarkerCount = 0;
}

// revisit this later with a TempEnt implementation
void Leader_BulletImpact(IGameEvent *pEvent)
{
	ZEPlayer *pPlayer = g_playerManager->GetPlayer(pEvent->GetPlayerSlot("userid"));

	if (!pPlayer)
		return;

	int iTracerIndex = pPlayer->GetLeaderTracer();

	if (!iTracerIndex)
		return;

	CCSPlayerPawn *pPawn = (CCSPlayerPawn *)pEvent->GetPlayerPawn("userid");
	CBasePlayerWeapon *pWeapon = pPawn->m_pWeaponServices->m_hActiveWeapon.Get();

	CParticleSystem* particle = CreateEntityByName<CParticleSystem>("info_particle_system");

	// Teleport particle to muzzle_flash attachment of player's weapon
	particle->AcceptInput("SetParent", "!activator", pWeapon, nullptr);
	particle->AcceptInput("SetParentAttachment", "muzzle_flash");

	CEntityKeyValues* pKeyValues = new CEntityKeyValues();

	// Event contains other end of the particle
	Vector vecData = Vector(pEvent->GetFloat("x"), pEvent->GetFloat("y"), pEvent->GetFloat("z"));
	Color clTint = LeaderColorMap[iTracerIndex].clColor;

	pKeyValues->SetString("effect_name", "particles/cs2fixes/leader_tracer.vpcf");
	pKeyValues->SetInt("data_cp", 1);
	pKeyValues->SetVector("data_cp_value", vecData);
	pKeyValues->SetInt("tint_cp", 2);
	pKeyValues->SetColor("tint_cp_color", clTint);
	pKeyValues->SetBool("start_active", true);

	particle->DispatchSpawn(pKeyValues);

	UTIL_AddEntityIOEvent(particle, "DestroyImmediately", nullptr, nullptr, "", 0.1f);
	UTIL_AddEntityIOEvent(particle, "Kill", nullptr, nullptr, "", 0.12f);
}

void Leader_Precache(IEntityResourceManifest *pResourceManifest)
{
	if (!g_szLeaderModelPath.empty())
		pResourceManifest->AddResource(g_szLeaderModelPath.c_str());
	pResourceManifest->AddResource("particles/cs2fixes/leader_tracer.vpcf");
	pResourceManifest->AddResource("particles/cs2fixes/leader_defend_mark.vpcf");
}

CON_COMMAND_CHAT(glow, "<name> [duration] - Toggle glow highlight on a player")
{
	ZEPlayer* pPlayer = player ? player->GetZEPlayer() : nullptr;
	bool bIsAdmin = pPlayer ? pPlayer->IsAdminFlagSet(ADMFLAG_GENERIC) : true;

	if (args.ArgC() < 2 && (bIsAdmin || (g_bEnableLeader && pPlayer->IsLeader())))
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Usage: !glow <name> [duration]");
		return;
	}

	Color color;
	int iDuration = 0;
	if (args.ArgC() == 3)
		iDuration = V_StringToInt32(args[2], 0);

	int iNumClients = 0;
	int pSlots[MAXPLAYERS];
	ETargetType nType;

	if (bIsAdmin) // Admin command logic
	{
		if (!g_playerManager->CanTargetPlayers(player, args[1], iNumClients, pSlots, NO_DEAD, nType))
			return;

		const char* pszCommandPlayerName = player ? player->GetPlayerName() : CONSOLE_NAME;

		for (int i = 0; i < iNumClients; i++)
		{
			CCSPlayerController* pTarget = CCSPlayerController::FromSlot(pSlots[i]);

			if (pTarget->m_iTeamNum < CS_TEAM_T)
				continue;

			// Exception - Use LeaderIndex color if Admin is also a Leader
			if (pPlayer && pPlayer->IsLeader())
				color = LeaderColorMap[pPlayer->GetLeaderIndex()].clColor;
			else
				color = pTarget->m_iTeamNum == CS_TEAM_T ? LeaderColorMap[2].clColor/*orange*/ : LeaderColorMap[1].clColor/*blue*/;

			ZEPlayer *pPlayerTarget = pTarget->GetZEPlayer();

			if (!pPlayerTarget->GetGlowModel())
				pPlayerTarget->StartGlow(color, iDuration);
			else
				pPlayerTarget->EndGlow();

			if (iNumClients == 1)
				PrintSingleAdminAction(pszCommandPlayerName, pTarget->GetPlayerName(), "toggled glow on", "", CHAT_PREFIX);
		}

		if (iNumClients > 1)
			PrintMultiAdminAction(nType, pszCommandPlayerName, "toggled glow on", "", CHAT_PREFIX);

		return;
	}

	// Leader command logic

	if (!g_bEnableLeader)
		return;

	if (!pPlayer->IsLeader())
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "You must be a Leader or an Admin to use this command.");
		return;
	}

	if (player->m_iTeamNum != CS_TEAM_CT && g_bLeaderActionsHumanOnly)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "You must be a human to use this command.");
		return;
	}

	if (!g_playerManager->CanTargetPlayers(player, args[1], iNumClients, pSlots, NO_DEAD | NO_MULTIPLE | NO_TERRORIST))
		return;

	CCSPlayerController* pTarget = CCSPlayerController::FromSlot(pSlots[0]);

	color = LeaderColorMap[pPlayer->GetLeaderIndex()].clColor;

	ZEPlayer *pPlayerTarget = pTarget->GetZEPlayer();

	if (!pPlayerTarget->GetGlowModel())
	{
		pPlayerTarget->StartGlow(color, iDuration);
		ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX "Leader %s enabled glow on %s.", player->GetPlayerName(), pTarget->GetPlayerName());
	}
	else
	{
		pPlayerTarget->EndGlow();
		ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX "Leader %s disabled glow on %s.", player->GetPlayerName(), pTarget->GetPlayerName());
	}
}

CON_COMMAND_CHAT(vl, "<name> - Vote for a player to become a leader")
{
	if (!g_bEnableLeader)
		return;

	if (!player)
	{
		ClientPrint(player, HUD_PRINTCONSOLE, CHAT_PREFIX "You cannot use this command from the server console.");
		return;
	}

	if (args.ArgC() < 2)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Usage: !vl <name>");
		return;
	}

	ZEPlayer* pPlayer = player->GetZEPlayer();

	if (pPlayer->GetLeaderVoteTime() + 30.0f > gpGlobals->curtime)
	{
		int iRemainingTime = (int)(pPlayer->GetLeaderVoteTime() + 30.0f - gpGlobals->curtime);
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Wait %i seconds before you can !vl again.", iRemainingTime);
		return;
	}

	int iNumClients = 0;
	int pSlots[MAXPLAYERS];

	if (!g_playerManager->CanTargetPlayers(player, args[1], iNumClients, pSlots, NO_MULTIPLE | NO_SELF | NO_BOT | NO_IMMUNITY))
		return;

	CCSPlayerController* pTarget = CCSPlayerController::FromSlot(pSlots[0]);
	ZEPlayer* pPlayerTarget = pTarget->GetZEPlayer();

	if (pPlayerTarget->IsLeader())
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "%s is already a Leader.", pTarget->GetPlayerName());
		return;
	}

	if (pPlayerTarget->HasPlayerVotedLeader(pPlayer))
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "You have already voted for %s to become a leader.", pTarget->GetPlayerName());
		return;
	}

	int iLeaderVoteCount = pPlayerTarget->GetLeaderVoteCount();
	int iNeededLeaderVoteCount = Leader_GetNeededLeaderVoteCount();

	pPlayer->SetLeaderVoteTime(gpGlobals->curtime);

	if (iLeaderVoteCount + 1 >= iNeededLeaderVoteCount)
	{
		pPlayerTarget->SetLeader(++g_iLeaderIndex);
		pPlayerTarget->PurgeLeaderVotes();
		pPlayerTarget->SetLeaderTracer(g_iLeaderIndex);
		g_vecLeaders.AddToTail(pPlayerTarget->GetHandle());

		if (pTarget->m_iTeamNum == CS_TEAM_CT)
		{
			CCSPlayerPawn *pPawn = (CCSPlayerPawn *)pTarget->GetPawn();
			Leader_ApplyLeaderVisuals(pPawn);
		}

		Message("%s was voted for Leader with %i vote(s). LeaderIndex = %i\n", pTarget->GetPlayerName(), iNeededLeaderVoteCount, g_iLeaderIndex);

		ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX "%s is now a Leader!", pTarget->GetPlayerName());
		
		ClientPrint(pTarget, HUD_PRINTTALK, CHAT_PREFIX "You became a leader! Use !leaderhelp and !leadercolors commands to list available leader commands and colors");

		// apply apparent leader perks (like leader model, glow(?)) here
		// also run a timer somewhere (per player or global) to reapply them

		return;
	}

	pPlayerTarget->AddLeaderVote(pPlayer);
	ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX "%s wants %s to become a Leader (%i/%i votes).",\
				player->GetPlayerName(), pTarget->GetPlayerName(), iLeaderVoteCount+1, iNeededLeaderVoteCount);
}

CON_COMMAND_CHAT(defend, "[name|duration] [duration] - Place a defend marker on you or target player")
{
	if (!g_bEnableLeader)
		return;

	if (!player)
	{
		ClientPrint(player, HUD_PRINTCONSOLE, CHAT_PREFIX "You cannot use this command from the server console.");
		return;
	}

	ZEPlayer* pPlayer = player->GetZEPlayer();

	if (!pPlayer->IsLeader())
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "You must be a leader to use this command.");
		return;
	}

	if (player->m_iTeamNum != CS_TEAM_CT && g_bLeaderActionsHumanOnly)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "You must be a human to use this command.");
		return;
	}

	// no arguments, place default duration marker on player
	if (args.ArgC() < 2)
	{
		if (player->m_iTeamNum != CS_TEAM_CT)
		{
			ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "You can only place defend marker on a human.");
			return;
		}

		if (Leader_CreateDefendMarker(pPlayer, LeaderColorMap[pPlayer->GetLeaderIndex()].clColor, 30))
			ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Placed defend marker on yourself lasting 30 seconds.");

		return;
	}

	int iNumClients = 0;
	int pSlots[MAXPLAYERS];
	ETargetType nType;

	if (!g_playerManager->CanTargetPlayers(player, args[1], iNumClients, pSlots, NO_MULTIPLE | NO_TERRORIST | NO_DEAD | NO_IMMUNITY, nType))
		return;

	// 1 argument, check if it's target or duration
	if (args.ArgC() == 2)
	{
		if (iNumClients) // valid target
		{
			CCSPlayerController *pTarget = CCSPlayerController::FromSlot(pSlots[0]);
			ZEPlayer* pTargetPlayer = pTarget->GetZEPlayer();

			if (Leader_CreateDefendMarker(pTargetPlayer, LeaderColorMap[pPlayer->GetLeaderIndex()].clColor, 30))
				ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Placed defend marker on %s lasting 30 seconds.", pTarget->GetPlayerName());

			return;
		}

		// Targetting self
		int iArg1 = V_StringToInt32(args[1], -1);
		if (iArg1 == -1) // target not found AND assume it's not a valid number
		{
			ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "%s", g_playerManager->GetErrorString(ETargetError::INVALID, (iNumClients == 0) ? 0 : pSlots[0]).c_str());
			return;
		}

		if (!g_playerManager->CanTargetPlayers(player, "@me", iNumClients, pSlots, NO_TERRORIST | NO_DEAD | NO_IMMUNITY, nType))
			return;

		if (iArg1 < 1)
		{
			if (Leader_CreateDefendMarker(pPlayer, LeaderColorMap[pPlayer->GetLeaderIndex()].clColor, 30))
				ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Placed defend marker on yourself lasting 30 seconds.");

			return;
		}
		iArg1 = MIN(iArg1, 60);

		if (Leader_CreateDefendMarker(pPlayer, LeaderColorMap[pPlayer->GetLeaderIndex()].clColor, iArg1))
			ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Placed defend marker on yourself lasting %i seconds.", iArg1);

		return;
	}

	CCSPlayerController *pTarget = CCSPlayerController::FromSlot(pSlots[0]);
	ZEPlayer* pTargetPlayer = pTarget->GetZEPlayer();

	int iArg2 = V_StringToInt32(args[2], -1);

	if (iArg2 < 1) // assume it's not a valid number
	{
		if (Leader_CreateDefendMarker(pTargetPlayer, LeaderColorMap[pPlayer->GetLeaderIndex()].clColor, 30))
			ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Placed defend marker on %s lasting 30 seconds.", pTarget->GetPlayerName());

		return;
	}

	iArg2 = MIN(iArg2, 60);

	if (Leader_CreateDefendMarker(pTargetPlayer, LeaderColorMap[pPlayer->GetLeaderIndex()].clColor, iArg2))
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Placed defend marker on %s lasting %i seconds.", pTarget->GetPlayerName(), iArg2);
}

CON_COMMAND_CHAT(tracer, "<name> [color] - Toggle projectile tracers on a player")
{
	if (!g_bEnableLeader)
		return;

	if (!player)
	{
		ClientPrint(player, HUD_PRINTCONSOLE, CHAT_PREFIX "You cannot use this command from the server console.");
		return;
	}

	int iPlayerSlot = player->GetPlayerSlot();

	ZEPlayer* pPlayer = g_playerManager->GetPlayer(iPlayerSlot);

	if (!pPlayer->IsLeader())
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "You must be a leader to use this command.");
		return;
	}

	if (player->m_iTeamNum != CS_TEAM_CT && g_bLeaderActionsHumanOnly)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "You must be a human to use this command.");
		return;
	}

	if (args.ArgC() < 2)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Usage: !tracer <name> [color]");
		return;
	}

	int iNumClients = 0;
	int pSlots[MAXPLAYERS];

	if (!g_playerManager->CanTargetPlayers(player, args[1], iNumClients, pSlots, NO_MULTIPLE | NO_TERRORIST))
		return;

	CCSPlayerController* pTarget = CCSPlayerController::FromSlot(pSlots[0]);
	ZEPlayer* pPlayerTarget = pTarget->GetZEPlayer();

	if (pPlayerTarget->GetLeaderTracer())
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Disabled tracers for player %s.", pTarget->GetPlayerName());
		pPlayerTarget->SetLeaderTracer(0);
		return;
	}

	int iTracerIndex = 0;
	if (args.ArgC() < 3)
		iTracerIndex = pPlayer->GetLeaderIndex();
	else
	{
		int iIndex = V_StringToInt32(args[2], -1);

		if (iIndex > -1)
			iTracerIndex = MIN(iIndex, g_nLeaderColorMapSize-1);
		else
		{
			for (int i = 0; i < g_nLeaderColorMapSize; i++)
			{
				if (!V_stricmp(args[2], LeaderColorMap[i].pszColorName))
				{
					iTracerIndex = i;
					break;
				}
			}
		}
	}

	ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Enabled tracers for player %s.", pTarget->GetPlayerName());
	pPlayerTarget->SetLeaderTracer(iTracerIndex);
}

CON_COMMAND_CHAT(beacon, "<name> [color] - Toggle beacon on a player")
{
	ZEPlayer* pPlayer = player ? player->GetZEPlayer() : nullptr;
	bool bIsAdmin = pPlayer ? pPlayer->IsAdminFlagSet(ADMFLAG_GENERIC) : true;

	if (args.ArgC() < 2 && (bIsAdmin || (g_bEnableLeader && pPlayer->IsLeader())))
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Usage: !beacon <name> [color]");
		return;
	}

	int iNumClients = 0;
	int pSlots[MAXPLAYERS];
	ETargetType nType;

	if (bIsAdmin) // Admin beacon logic
	{
		if (!g_playerManager->CanTargetPlayers(player, args[1], iNumClients, pSlots, NO_DEAD, nType))
			return;

		const char* pszCommandPlayerName = player ? player->GetPlayerName() : CONSOLE_NAME;

		Color color;
		if (args.ArgC() == 3)
			color = Leader_ColorFromString(args[2]);

		for (int i = 0; i < iNumClients; i++)
		{
			CCSPlayerController* pTarget = CCSPlayerController::FromSlot(pSlots[i]);

			if (!pTarget)
				continue;

			if (pTarget->m_iTeamNum < CS_TEAM_T)
				continue;

			// Exception - Use LeaderIndex color if Admin is also a Leader
			if (args.ArgC() == 2 && pPlayer && pPlayer->IsLeader())
				color = LeaderColorMap[pPlayer->GetLeaderIndex()].clColor;
			else if (args.ArgC() == 2)
				color = pTarget->m_iTeamNum == CS_TEAM_T ? LeaderColorMap[2].clColor/*orange*/ : LeaderColorMap[1].clColor/*blue*/;

			ZEPlayer *pPlayerTarget = pTarget->GetZEPlayer();

			if (!pPlayerTarget->GetBeaconParticle())
				pPlayerTarget->StartBeacon(color, pPlayer->GetHandle());
			else
				pPlayerTarget->EndBeacon();

			if (iNumClients == 1)
				PrintSingleAdminAction(pszCommandPlayerName, pTarget->GetPlayerName(), "toggled beacon on", "", CHAT_PREFIX);
		}

		if (iNumClients > 1)
			PrintMultiAdminAction(nType, pszCommandPlayerName, "toggled beacon on", "", CHAT_PREFIX);

		return;
	}

	// Leader beacon logic

	if (!g_bEnableLeader)
		return;

	if (!pPlayer->IsLeader())
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "You must be a Leader or an Admin to use this command.");
		return;
	}

	if (player->m_iTeamNum != CS_TEAM_CT && g_bLeaderActionsHumanOnly)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "You must be a human to use this command.");
		return;
	}

	if (!g_playerManager->CanTargetPlayers(player, args[1], iNumClients, pSlots, NO_DEAD | NO_MULTIPLE | NO_TERRORIST))
		return;

	CCSPlayerController* pTarget = CCSPlayerController::FromSlot(pSlots[0]);

	Color color;
	if (args.ArgC() == 3)
		color = Leader_ColorFromString(args[2]);
	else
		color = LeaderColorMap[pPlayer->GetLeaderIndex()].clColor;

	ZEPlayer *pPlayerTarget = pTarget->GetZEPlayer();

	if (!pPlayerTarget->GetBeaconParticle())
	{
		pPlayerTarget->StartBeacon(color, pPlayer->GetHandle());
		ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX "Leader %s enabled beacon on %s.", player->GetPlayerName(), pTarget->GetPlayerName());
	}
	else
	{
		pPlayerTarget->EndBeacon();
		ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX "Leader %s disabled beacon on %s.", player->GetPlayerName(), pTarget->GetPlayerName());
	}
}

CON_COMMAND_CHAT(leaders, "- List all current leaders")
{
	if (!g_bEnableLeader)
		return;

	int iDestination = player ? HUD_PRINTTALK : HUD_PRINTCONSOLE;

	if (Leader_NoLeaders()) // also wipes any invalid entries from g_vecLeaders
	{
		ClientPrint(player, iDestination, CHAT_PREFIX "There are currently no leaders.");
		return;
	}

	ClientPrint(player, iDestination, CHAT_PREFIX "List of current leaders:");

	FOR_EACH_VEC(g_vecLeaders, i)
	{
		ZEPlayer *pLeader = g_vecLeaders[i].Get();
		CCSPlayerController *pController = CCSPlayerController::FromSlot((CPlayerSlot) pLeader->GetPlayerSlot());

		ClientPrint(player, iDestination, CHAT_PREFIX "%s", pController->GetPlayerName());
	}
}

CON_COMMAND_CHAT(leaderhelp, "- List leader commands in chat")
{
	if (!g_bEnableLeader)
		return;

	int iDestination = player ? HUD_PRINTTALK : HUD_PRINTCONSOLE;

	ClientPrint(player, iDestination, CHAT_PREFIX "List of leader commands:");
	ClientPrint(player, iDestination, CHAT_PREFIX "!beacon <name> [color] - place a beacon on player");
	ClientPrint(player, iDestination, CHAT_PREFIX "!tracer <name> [color] - give player tracers");
	ClientPrint(player, iDestination, CHAT_PREFIX "!defend [name|duration] [duration] - place defend mark on player");
	ClientPrint(player, iDestination, CHAT_PREFIX "!glow <name> [duration] - toggle glow highlight on a player");
}

CON_COMMAND_CHAT(leadercolors, "- List leader colors in chat")
{
	if (!g_bEnableLeader)
		return;

	int iDestination = player ? HUD_PRINTTALK : HUD_PRINTCONSOLE;

	ClientPrint(player, iDestination, CHAT_PREFIX "List of leader colors:");
	for (int i = 0; i < g_nLeaderColorMapSize; i++)
	{
		ClientPrint(player, iDestination, CHAT_PREFIX "%i - %s", i, LeaderColorMap[i].pszColorName);
	}
}

CON_COMMAND_CHAT_FLAGS(forceld, "<name> [color] - Force leader status on a player", ADMFLAG_GENERIC)
{
	if (!g_bEnableLeader)
		return;

	if (args.ArgC() < 2)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Usage: !forceld <name> [index]");
		return;
	}

	int iNumClients = 0;
	int pSlots[MAXPLAYERS];

	if (!g_playerManager->CanTargetPlayers(player, args[1], iNumClients, pSlots, NO_MULTIPLE))
		return;

	const char* pszCommandPlayerName = player ? player->GetPlayerName() : CONSOLE_NAME;

	CCSPlayerController* pTarget = CCSPlayerController::FromSlot(pSlots[0]);
	ZEPlayer* pPlayerTarget = pTarget->GetZEPlayer();

	if (pPlayerTarget->IsLeader())
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "%s is already a leader.", pTarget->GetPlayerName());
		return;
	}

	if (args.ArgC() < 3)
	{
		pPlayerTarget->SetLeader(++g_iLeaderIndex);
		pPlayerTarget->SetLeaderTracer(g_iLeaderIndex);
	}
	else
	{
		int iColorIndex = V_StringToInt32(args[2], -1);

		if (iColorIndex > -1)
		{
			iColorIndex = MIN(iColorIndex, g_nLeaderColorMapSize-1);
		}
		else
		{
			for (int i = 0; i < g_nLeaderColorMapSize; i++)
			{
				if (!V_stricmp(args[2], LeaderColorMap[i].pszColorName))
				{
					iColorIndex = i;
					break;
				}
			}
		}

		if (iColorIndex > -1)
		{
			pPlayerTarget->SetLeader(iColorIndex);
			pPlayerTarget->SetLeaderTracer(iColorIndex);
		}
		else
		{
			pPlayerTarget->SetLeader(++g_iLeaderIndex);
			pPlayerTarget->SetLeaderTracer(g_iLeaderIndex);
		}
	}

	if (pTarget->m_iTeamNum == CS_TEAM_CT)
		Leader_ApplyLeaderVisuals((CCSPlayerPawn *)pTarget->GetPawn());

	pPlayerTarget->PurgeLeaderVotes();
	g_vecLeaders.AddToTail(pPlayerTarget->GetHandle());

	PrintSingleAdminAction(pszCommandPlayerName, pTarget->GetPlayerName(), "forced", " to be a Leader", CHAT_PREFIX);
}

CON_COMMAND_CHAT_FLAGS(stripld, "<name> - Strip leader status from a player", ADMFLAG_GENERIC)
{
	if (!g_bEnableLeader)
		return;

	if (args.ArgC() < 2)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Usage: !stripld <name>");
		return;
	}

	int iNumClients = 0;
	int pSlots[MAXPLAYERS];

	if (!g_playerManager->CanTargetPlayers(player, args[1], iNumClients, pSlots, NO_MULTIPLE))
		return;

	const char* pszCommandPlayerName = player ? player->GetPlayerName() : CONSOLE_NAME;

	CCSPlayerController* pTarget = CCSPlayerController::FromSlot(pSlots[0]);
	ZEPlayer* pPlayerTarget = pTarget->GetZEPlayer();

	if (!pPlayerTarget->IsLeader())
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "%s is not a leader.", pTarget->GetPlayerName());
		return;
	}

	pPlayerTarget->SetLeader(0);
	pPlayerTarget->SetLeaderTracer(0);
	FOR_EACH_VEC(g_vecLeaders, i)
	{
		if (g_vecLeaders[i] == pPlayerTarget)
		{
			g_vecLeaders.Remove(i);
			break;
		}
	}

	if (pTarget->m_iTeamNum == CS_TEAM_CT)
		Leader_RemoveLeaderVisuals((CCSPlayerPawn *)pTarget->GetPawn());

	PrintSingleAdminAction(pszCommandPlayerName, pTarget->GetPlayerName(), "stripped leader from ", "", CHAT_PREFIX);
}