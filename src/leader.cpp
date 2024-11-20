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

extern IVEngineServer2* g_pEngineServer2;
extern CGameEntitySystem* g_pEntitySystem;
extern CGlobalVars* gpGlobals;
extern IGameEventManager2* g_gameEventManager;

// All colors MUST have 255 alpha
std::map<std::string, ColorPreset> mapColorPresets = {
	{"darkred",    ColorPreset("\x02", Color(255,   0,   0, 255), true)},
	{"red",        ColorPreset("\x07", Color(255,  64,  64, 255), true)},
	{"lightred",   ColorPreset("\x0F", Color(235,  75,  75, 255), true)},
	{"orange",     ColorPreset("\x10", Color(227, 175,  57, 255), true)}, // Default T color
	{"yellow",     ColorPreset("\x09", Color(236, 227, 122, 255), true)},
	{"lime",       ColorPreset("\x05", Color(190, 253, 146, 255), true)},
	{"lightgreen", ColorPreset("\x06", Color(190, 253, 146, 255), true)},
	{"green",      ColorPreset("\x04", Color( 64, 255,  61, 255), true)},
	{"lightblue",  ColorPreset("\x0B", Color( 94, 152, 216, 255), true)},
	{"blue",       ColorPreset("\x0C", Color( 76, 106, 255, 255), true)}, // Default if color finding func doesn't match any other color
	{"purple",     ColorPreset("\x0E", Color(211,  45, 231, 255), true)},
	{"gray",       ColorPreset("\x0A", Color(176, 194, 216, 255), true)},
	{"grey",       ColorPreset("\x08", Color(196, 201, 207, 255), true)},
	{"white",      ColorPreset("\x01", Color(255, 255, 255, 255), true)},
	{"pink",       ColorPreset(    "", Color(255, 192, 203, 255), false)}
};

CUtlVector<ZEPlayerHandle> g_vecLeaders;

static int g_iMarkerCount = 0;
static bool g_bPingWithLeader = true;

// 'CONVARS'
bool g_bEnableLeader = false;
static float g_flLeaderVoteRatio = 0.15;
bool g_bLeaderActionsHumanOnly = true;
bool g_bLeaderMarkerHumanOnly = true;
static bool g_bMuteNonLeaderPings = true;
static std::string g_strLeaderModelPath = "";
static std::string g_strDefendParticlePath = "particles/cs2fixes/leader_defend_mark.vpcf";
std::string g_strMarkParticlePath = "particles/cs2fixes/leader_defend_mark.vpcf";
static bool g_bLeaderCanTargetPlayers = false;
static bool g_bLeaderVoteMultiple = true;

FAKE_BOOL_CVAR(cs2f_leader_enable, "Whether to enable Leader features", g_bEnableLeader, false, false)
FAKE_FLOAT_CVAR(cs2f_leader_vote_ratio, "Vote ratio needed for player to become a leader", g_flLeaderVoteRatio, 0.2f, false)
FAKE_BOOL_CVAR(cs2f_leader_actions_ct_only, "Whether to allow leader actions (like !beacon) only from human team", g_bLeaderActionsHumanOnly, true, false)
FAKE_BOOL_CVAR(cs2f_leader_marker_ct_only, "Whether to have zombie leaders' player_pings spawn in particle markers or not", g_bLeaderMarkerHumanOnly, true, false)
FAKE_BOOL_CVAR(cs2f_leader_mute_player_pings, "Whether to mute player pings made by non-leaders", g_bMuteNonLeaderPings, true, false)
FAKE_STRING_CVAR(cs2f_leader_model_path, "Path to player model to be used for leaders", g_strLeaderModelPath, false)
FAKE_STRING_CVAR(cs2f_leader_defend_particle, "Path to defend particle to be used with c_defend", g_strDefendParticlePath, false)
FAKE_STRING_CVAR(cs2f_leader_mark_particle, "Path to particle to be used when a ct leader using player_ping", g_strMarkParticlePath, false)
FAKE_BOOL_CVAR(cs2f_leader_can_target_players, "Whether a leader can target other players with leader commands (not including c_leader)", g_bLeaderCanTargetPlayers, false, false)
FAKE_BOOL_CVAR(cs2f_leader_vote_multiple , "If true, players can vote up to cs2f_max_leaders leaders. If false, they may vote for a single leader", g_bLeaderVoteMultiple, true, false)

static int g_iMaxLeaders = 3;
static int g_iMaxMarkers = 6;
static int g_iMaxGlows = 3;
static int g_iMaxTracers = 3;
static int g_iMaxBeacons = 3;

FAKE_INT_CVAR(cs2f_leader_max_leaders, "Max amount of leaders set via c_vl or a leader using c_leader (doesn't impact admins)", g_iMaxLeaders, 3, false)
FAKE_INT_CVAR(cs2f_leader_max_markers, "Max amount of markers set by leaders (doesn't impact admins)", g_iMaxMarkers, 6, false)
FAKE_INT_CVAR(cs2f_leader_max_glows, "Max amount of glows set by leaders (doesn't impact admins)", g_iMaxGlows, 3, false)
FAKE_INT_CVAR(cs2f_leader_max_tracers, "Max amount of tracers set by leaders (doesn't impact admins)", g_iMaxTracers, 3, false)
FAKE_INT_CVAR(cs2f_leader_max_beacons, "Max amount of beacons set by leaders (doesn't impact admins)", g_iMaxBeacons, 3, false)

bool Leader_SetNewLeader(ZEPlayer* zpLeader, std::string strColor = "")
{
	CCSPlayerController* pLeader = CCSPlayerController::FromSlot(zpLeader->GetPlayerSlot());
	CCSPlayerPawn* pawnLeader = (CCSPlayerPawn*)pLeader->GetPawn();
	
	if (zpLeader->IsLeader())
		return false;

	pLeader->m_iScore() = pLeader->m_iScore() + 20000;
	zpLeader->SetLeader(true);
	Color color = Color(0, 0, 0, 0);

	if (pawnLeader && pawnLeader->m_iHealth() > 0 && pLeader->m_iTeamNum == CS_TEAM_CT && (Color)(pawnLeader->m_clrRender) != Color(255, 255, 255, 255))
		color = pawnLeader->m_clrRender;

	if (strColor.length() > 0)
	{
		std::transform(strColor.begin(), strColor.end(), strColor.begin(), [](unsigned char c) { return std::tolower(c); });

		if (strColor.length() > 0 && mapColorPresets.contains(strColor))
			color = mapColorPresets.at(strColor).color;
	}

	if (color.a() < 255 || std::sqrt(std::pow(color.r(), 2) + std::pow(color.g(), 2) + std::pow(color.b(), 2)) < 150)
	{
		// Dark color or no color set. Use a random color from mapColorPresets instead
		auto it = mapColorPresets.begin();
		std::advance(it, rand() % mapColorPresets.size());
		color = it->second.color;
	}

	zpLeader->SetLeaderColor(color);
	zpLeader->SetBeaconColor(color);

	if (pawnLeader)
		Leader_ApplyLeaderVisuals(pawnLeader);

	zpLeader->PurgeLeaderVotes();
	g_vecLeaders.AddToTail(zpLeader->GetHandle());
	return true;
}

Color Leader_GetColor(std::string strColor, ZEPlayer* zpUser = nullptr, CCSPlayerController* pTarget = nullptr)
{
	std::transform(strColor.begin(), strColor.end(), strColor.begin(), [](unsigned char c) { return std::tolower(c); });

	if (strColor.length() > 0 && mapColorPresets.contains(strColor))
		return mapColorPresets.at(strColor).color;
	else if (zpUser && zpUser->IsLeader())
		return zpUser->GetLeaderColor();
	else if (pTarget && pTarget->m_iTeamNum == CS_TEAM_T)
		return mapColorPresets["orange"].color;
	
	return mapColorPresets["blue"].color;
}

// This also wipes any invalid entries from g_vecLeaders
std::pair<int, std::string> GetLeaders()
{
	int iLeaders = 0;
	std::string strLeaders = "";
	FOR_EACH_VEC_BACK(g_vecLeaders, i)
	{
		ZEPlayer* pLeader = g_vecLeaders[i].Get();
		if (!pLeader)
		{
			g_vecLeaders.Remove(i);
			continue;
		}
		CCSPlayerController* pController = CCSPlayerController::FromSlot((CPlayerSlot) pLeader->GetPlayerSlot());
		if (!pController)
			continue;
		
		iLeaders++;
		strLeaders.append(pController->GetPlayerName());
		strLeaders.append(", ");
	}

	if (iLeaders == 0)
		return std::make_pair(0, "");
	else
		return std::make_pair(iLeaders, strLeaders.substr(0, strLeaders.length() - 2));
}

std::pair<int, std::string> GetCount(int iType)
{
	int iCount = 0;
	std::string strPlayerNames = "";

	for (int i = 0; i < gpGlobals->maxClients; i++)
	{
		CCSPlayerController* pPlayer = CCSPlayerController::FromSlot(CPlayerSlot(i));
		if (!pPlayer)
			continue;

		ZEPlayer* zpPlayer = pPlayer->GetZEPlayer();
		if (!zpPlayer)
			continue;

		switch (iType)
		{
			case 1:
				if (!zpPlayer->GetGlowModel())
					continue;
				break;
			case 2:
				if (zpPlayer->GetTracerColor().a() < 255)
					continue;
				break;
			case 3:
				if (!zpPlayer->GetBeaconParticle())
					continue;
				break;
		}

		iCount++;
		strPlayerNames.append(pPlayer->GetPlayerName());
		strPlayerNames.append(", ");
	}

	if (iCount == 0)
		return std::make_pair(0, "");
	else
		return std::make_pair(iCount, strPlayerNames.substr(0, strPlayerNames.length() - 2));
}

void Leader_ApplyLeaderVisuals(CCSPlayerPawn* pPawn)
{
	CCSPlayerController* pLeader = CCSPlayerController::FromPawn(pPawn);
	ZEPlayer* zpLeader = pLeader->GetZEPlayer();

	if (!zpLeader || !zpLeader->IsLeader() || pLeader->m_iTeamNum != CS_TEAM_CT)
		return;

	if (!g_strLeaderModelPath.empty())
	{
		pPawn->SetModel(g_strLeaderModelPath.c_str());
		pPawn->AcceptInput("Skin", 0);
	}

	pPawn->m_clrRender = zpLeader->GetLeaderColor();
	if (zpLeader->GetBeaconColor().a() == 255)
	{
		Color colorBeacon = zpLeader->GetBeaconColor();
		if (zpLeader->GetBeaconParticle())
			zpLeader->EndBeacon();
		zpLeader->StartBeacon(colorBeacon, zpLeader->GetHandle());
	}

	if (zpLeader->GetGlowColor().a() == 255)
	{
		Color colorGlow = zpLeader->GetGlowColor();
		if (zpLeader->GetGlowModel())
			zpLeader->EndGlow();
		zpLeader->StartGlow(colorGlow, 0);
	}
}

void Leader_RemoveLeaderVisuals(CCSPlayerPawn* pPawn)
{
	g_pZRPlayerClassManager->ApplyPreferredOrDefaultHumanClassVisuals(pPawn);

	ZEPlayer* zpLeader = CCSPlayerController::FromPawn(pPawn)->GetZEPlayer();

	if (!zpLeader)
		return;

	zpLeader->SetTracerColor(Color(0, 0, 0, 0));
	if (zpLeader->GetBeaconParticle())
		zpLeader->EndBeacon();
	if (zpLeader->GetGlowModel())
		zpLeader->EndGlow();
}

bool Leader_CreateDefendMarker(ZEPlayer* pPlayer, Color clrTint, int iDuration)
{
	CCSPlayerController* pController = CCSPlayerController::FromSlot(pPlayer->GetPlayerSlot());
	CCSPlayerPawn* pPawn = (CCSPlayerPawn*)pController->GetPawn();

	if (g_iMarkerCount >= g_iMaxMarkers)
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

	CParticleSystem* pMarker = CreateEntityByName<CParticleSystem>("info_particle_system");

	Vector vecOrigin = pPawn->GetAbsOrigin();
	vecOrigin.z += 10;

	CEntityKeyValues* pKeyValues = new CEntityKeyValues();

	pKeyValues->SetString("effect_name", g_strDefendParticlePath.c_str());
	pKeyValues->SetInt("tint_cp", 1);
	pKeyValues->SetColor("tint_cp_color", clrTint);
	pKeyValues->SetVector("origin", vecOrigin);
	pKeyValues->SetBool("start_active", true);

	pMarker->DispatchSpawn(pKeyValues);

	UTIL_AddEntityIOEvent(pMarker, "DestroyImmediately", nullptr, nullptr, "", iDuration);
	UTIL_AddEntityIOEvent(pMarker, "Kill", nullptr, nullptr, "", iDuration + 0.02f);

	return true;
}

void Leader_PostEventAbstract_Source1LegacyGameEvent(const uint64* clients, const CNetMessage* pData)
{
	if (!g_bEnableLeader)
		return;

	auto pPBData = pData->ToPB<CMsgSource1LegacyGameEvent>();
	
	static int player_ping_id = g_gameEventManager->LookupEventId("player_ping");

	if (pPBData->eventid() != player_ping_id)
		return;

	bool bNoHumanLeaders = true;
	FOR_EACH_VEC_BACK(g_vecLeaders, i)
	{
		if (g_vecLeaders[i].IsValid())
		{
			CCSPlayerController* pLeader = CCSPlayerController::FromSlot(g_vecLeaders[i].Get()->GetPlayerSlot());
			if (pLeader && pLeader->m_iTeamNum == CS_TEAM_CT)
			{
				bNoHumanLeaders = false;
				break;
			}
		}
	}

	if (bNoHumanLeaders)
	{
		if (g_bMuteNonLeaderPings)
			*(uint64*)clients = 0;

		return;
	}

	IGameEvent* pEvent = g_gameEventManager->UnserializeEvent(*pPBData);

	ZEPlayer* pPlayer = g_playerManager->GetPlayer(pEvent->GetPlayerSlot("userid"));
	CCSPlayerController* pController = CCSPlayerController::FromSlot(pEvent->GetPlayerSlot("userid"));
	CBaseEntity* pEntity = (CBaseEntity*)g_pEntitySystem->GetEntityInstance(pEvent->GetEntityIndex("entityid"));

	g_gameEventManager->FreeEvent(pEvent);

	// Add a mark particle to CT leader pings
	if (pPlayer->IsLeader() && (pController->m_iTeamNum == CS_TEAM_CT || !g_bLeaderMarkerHumanOnly))
	{
		Vector vecOrigin = pEntity->GetAbsOrigin();
		vecOrigin.z += 10;

		pPlayer->CreateMark(15, vecOrigin); // 6.1 seconds is time of default ping if you want it to match
		return;
	}

	if (pController->m_iTeamNum == CS_TEAM_T || g_bPingWithLeader)
	{
		if (g_bMuteNonLeaderPings)
			*(uint64*)clients = 0;

		return;
	}

	// Remove entity responsible for visual part of the ping
	pEntity->Remove();

	// Block clients from playing the ping sound
	*(uint64*)clients = 0;
}

void Leader_OnRoundStart(IGameEvent* pEvent)
{
	for (int i = 0; i < gpGlobals->maxClients; i++)
	{
		CCSPlayerController* pLeader = CCSPlayerController::FromSlot((CPlayerSlot)i);
		if (!pLeader)
			continue;

		CCSPlayerPawn* pawnLeader = (CCSPlayerPawn*)pLeader->GetPawn();

		if (!pawnLeader)
			continue;

		ZEPlayer* zpLeader = g_playerManager->GetPlayer((CPlayerSlot)i);

		if (zpLeader && !zpLeader->IsLeader())
			Leader_RemoveLeaderVisuals(pawnLeader);
		else
			Leader_ApplyLeaderVisuals(pawnLeader);
	}

	g_bPingWithLeader = true;
	g_iMarkerCount = 0;
}

// revisit this later with a TempEnt implementation
void Leader_BulletImpact(IGameEvent* pEvent)
{
	ZEPlayer* pPlayer = g_playerManager->GetPlayer(pEvent->GetPlayerSlot("userid"));

	if (!pPlayer || pPlayer->GetTracerColor().a() < 255)
		return;

	CCSPlayerPawn* pPawn = (CCSPlayerPawn*)pEvent->GetPlayerPawn("userid");
	CBasePlayerWeapon* pWeapon = pPawn->m_pWeaponServices->m_hActiveWeapon.Get();

	CParticleSystem* particle = CreateEntityByName<CParticleSystem>("info_particle_system");

	// Teleport particle to muzzle_flash attachment of player's weapon
	particle->AcceptInput("SetParent", "!activator", pWeapon, nullptr);
	particle->AcceptInput("SetParentAttachment", "muzzle_flash");

	CEntityKeyValues* pKeyValues = new CEntityKeyValues();

	// Event contains other end of the particle
	Vector vecData = Vector(pEvent->GetFloat("x"), pEvent->GetFloat("y"), pEvent->GetFloat("z"));

	pKeyValues->SetString("effect_name", "particles/cs2fixes/leader_tracer.vpcf");
	pKeyValues->SetInt("data_cp", 1);
	pKeyValues->SetVector("data_cp_value", vecData);
	pKeyValues->SetInt("tint_cp", 2);
	pKeyValues->SetColor("tint_cp_color", pPlayer->GetTracerColor());
	pKeyValues->SetBool("start_active", true);

	particle->DispatchSpawn(pKeyValues);

	UTIL_AddEntityIOEvent(particle, "DestroyImmediately", nullptr, nullptr, "", 0.1f);
	UTIL_AddEntityIOEvent(particle, "Kill", nullptr, nullptr, "", 0.12f);
}

void Leader_Precache(IEntityResourceManifest* pResourceManifest)
{
	if (!g_strLeaderModelPath.empty())
		pResourceManifest->AddResource(g_strLeaderModelPath.c_str());
	pResourceManifest->AddResource("particles/cs2fixes/leader_tracer.vpcf");
	pResourceManifest->AddResource(g_strDefendParticlePath.c_str());
	pResourceManifest->AddResource(g_strMarkParticlePath.c_str());
}

CON_COMMAND_CHAT_LEADER(glow, "[name] [color] - Toggle glow highlight on a player")
{
	ZEPlayer* pPlayer = player ? player->GetZEPlayer() : nullptr;
	bool bIsAdmin = pPlayer ? pPlayer->IsAdminFlagSet(FLAG_LEADER) : true;
	const char* pszCommandPlayerName = player ? player->GetPlayerName() : CONSOLE_NAME;

	int iNumClients = 0;
	int pSlots[MAXPLAYERS];
	ETargetType nType;
	uint64 iTargetFlags = NO_DEAD;
	if (!bIsAdmin)
		iTargetFlags |= NO_MULTIPLE | NO_TERRORIST;
	const char* pszTarget = "@me";
	if (args.ArgC() >= 2 && (bIsAdmin || g_bLeaderCanTargetPlayers))
		pszTarget = args[1];

	if (!g_playerManager->CanTargetPlayers(player, pszTarget, iNumClients, pSlots, iTargetFlags, nType))
		return;

	for (int i = 0; i < iNumClients; i++)
	{
		CCSPlayerController* pTarget = CCSPlayerController::FromSlot(pSlots[i]);

		ZEPlayer* pPlayerTarget = pTarget->GetZEPlayer();

		if (iNumClients == 1 && player == pTarget)
		{
			ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "%s glow on yourself.",
						pPlayerTarget->GetGlowModel() ? "Disabled" : "Enabled");
		}
		else if (iNumClients == 1)
		{
			ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX "%s %s %s glow on %s.",
						   bIsAdmin ? "Admin" : "Leader",
						   pszCommandPlayerName,
						   pPlayerTarget->GetGlowModel() ? "disabled" : "enabled",
						   pTarget->GetPlayerName());
		}

		if (!pPlayerTarget->GetGlowModel())
		{
			Color color = Leader_GetColor(args.ArgC() < 3 ? "" : args[2], pPlayer, pTarget);
			pPlayerTarget->StartGlow(color, 0);
		}
		else
			pPlayerTarget->EndGlow();
	}

	if (iNumClients > 1) // Can only hit this if bIsAdmin due to target flags
		PrintMultiAdminAction(nType, pszCommandPlayerName, "toggled glow on", "", CHAT_PREFIX);
}

CON_COMMAND_CHAT(glows, "- List all active player glows")
{
	std::pair<int, std::string> glows = GetCount(1);

	if (glows.first == 0)
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "There are no active glows.");
	else
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "%i active glows: %s", glows.first, glows.second.c_str());
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

	if (gpGlobals->curtime < 60.0f)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Leader voting is not open yet.");
		return;
	}
	
	if (GetLeaders().first > 0 && !g_bLeaderVoteMultiple)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "There is already an active leader.");
		return;
	}

	if (GetLeaders().first >= g_iMaxLeaders)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "The max amount of leaders has already been reached.");
		return;
	}

	ZEPlayer* pPlayer = player->GetZEPlayer();
	if (!pPlayer)
		return;

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
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "%s is already a leader.", pTarget->GetPlayerName());
		return;
	}

	if (pPlayerTarget->HasPlayerVotedLeader(pPlayer))
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "You have already voted for %s to become a leader.", pTarget->GetPlayerName());
		return;
	}

	int iLeaderVoteCount = pPlayerTarget->GetLeaderVoteCount();
	int iNeededLeaderVoteCount = (int)(g_playerManager->GetOnlinePlayerCount(false) * g_flLeaderVoteRatio) + 1;;

	pPlayer->SetLeaderVoteTime(gpGlobals->curtime);

	if (iLeaderVoteCount + 1 >= iNeededLeaderVoteCount)
	{
		Leader_SetNewLeader(pPlayerTarget);
		Message("%s was voted for Leader with %i vote(s).\n", pTarget->GetPlayerName(), iNeededLeaderVoteCount);
		ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX "%s has been voted as a leader!", pTarget->GetPlayerName());
		ClientPrint(pTarget, HUD_PRINTTALK, CHAT_PREFIX "You became a leader! Use !leaderhelp and !leadercolors commands to list available leader commands and colors.");
		return;
	}

	pPlayerTarget->AddLeaderVote(pPlayer);
	ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX "%s wants %s to become a Leader (%i/%i votes).",\
				player->GetPlayerName(), pTarget->GetPlayerName(), iLeaderVoteCount+1, iNeededLeaderVoteCount);
}

CON_COMMAND_CHAT_LEADER(defend, "[name|duration] [duration] - Place a defend marker on the target player")
{
	ZEPlayer* pPlayer = player ? player->GetZEPlayer() : nullptr;
	bool bIsAdmin = pPlayer ? pPlayer->IsAdminFlagSet(FLAG_LEADER) : true;
	int iDuration = args.ArgC() != 2 ? -1 : V_StringToInt32(args[1], -1);
	const char* pszCommandPlayerName = player ? player->GetPlayerName() : CONSOLE_NAME;

	int iNumClients = 0;
	int pSlots[MAXPLAYERS];
	ETargetType nType;
	const char* pszTarget = "@me";
	if ((args.ArgC() >= 2 || (args.ArgC() == 2 && iDuration == -1)) && (bIsAdmin || g_bLeaderCanTargetPlayers))
		pszTarget = args[1];
	iDuration = (iDuration < 1) ? 30 : MIN(iDuration, 60);
	
	if (!g_playerManager->CanTargetPlayers(player, pszTarget, iNumClients, pSlots, NO_MULTIPLE | NO_DEAD | NO_TERRORIST | NO_IMMUNITY, nType))
		return;

	CCSPlayerController* pTarget = CCSPlayerController::FromSlot(pSlots[0]);
	ZEPlayer* pTargetPlayer = pTarget->GetZEPlayer();

	if (Leader_CreateDefendMarker(pTargetPlayer, Leader_GetColor("", pPlayer), iDuration))
	{
		if (player == pTarget)
			ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Placed a defend marker on your position lasting %i seconds.", iDuration);
		else
		{
			ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX "%s %s placed a defend marker on %s's position lasting %i seconds.",
						   bIsAdmin ? "Admin" : "Leader", pszCommandPlayerName,
						   pTarget->GetPlayerName(), iDuration);
		}
	}
}

CON_COMMAND_CHAT_LEADER(tracer, "[name] [color] - Toggle projectile tracers on a player")
{
	ZEPlayer* pPlayer = player ? player->GetZEPlayer() : nullptr;
	bool bIsAdmin = pPlayer ? pPlayer->IsAdminFlagSet(FLAG_LEADER) : true;
	const char* pszCommandPlayerName = player ? player->GetPlayerName() : CONSOLE_NAME;

	int iNumClients = 0;
	int pSlots[MAXPLAYERS];
	ETargetType nType;
	const char* pszTarget = "@me";
	if (args.ArgC() >= 2 && (bIsAdmin || g_bLeaderCanTargetPlayers))
		pszTarget = args[1];

	if (!g_playerManager->CanTargetPlayers(player, pszTarget, iNumClients, pSlots, NO_DEAD | NO_TERRORIST | NO_MULTIPLE, nType))
		return;

	CCSPlayerController* pTarget = CCSPlayerController::FromSlot(pSlots[0]);
	ZEPlayer* pPlayerTarget = pTarget->GetZEPlayer();

	if (pPlayerTarget->GetTracerColor().a() == 255)
	{
		if (pTarget == player)
			ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Disabled tracers for yourself.", pTarget->GetPlayerName());
		else
		{
			ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX "%s %s disabled tracers for %s.",
						   bIsAdmin ? "Admin" : "Leader", pszCommandPlayerName, pTarget->GetPlayerName());
		}
		pPlayerTarget->SetTracerColor(Color(0, 0, 0, 0));
		return;
	}

	Color color = Leader_GetColor(args.ArgC() < 3 ? "" : args[2], pPlayer);
	pPlayerTarget->SetTracerColor(color);

	if (pTarget == player)
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Enabled tracers for yourself.", pTarget->GetPlayerName());
	else
	{
		ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX "%s %s enabled tracers for %s.",
					   bIsAdmin ? "Admin" : "Leader", pszCommandPlayerName, pTarget->GetPlayerName());
	}
}

CON_COMMAND_CHAT(tracers, "- List all active player tracers")
{
	std::pair<int, std::string> tracers = GetCount(2);

	if (tracers.first == 0)
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "There are no active tracers.");
	else
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "%i active tracers: %s", tracers.first, tracers.second.c_str());
}

CON_COMMAND_CHAT_LEADER(beacon, "[name] [color] - Toggle beacon on a player")
{
	ZEPlayer* pPlayer = player ? player->GetZEPlayer() : nullptr;
	bool bIsAdmin = pPlayer ? pPlayer->IsAdminFlagSet(FLAG_LEADER) : true;
	const char* pszCommandPlayerName = player ? player->GetPlayerName() : CONSOLE_NAME;

	int iNumClients = 0;
	int pSlots[MAXPLAYERS];
	ETargetType nType;
	uint64 iTargetFlags = NO_DEAD;
	if (!bIsAdmin)
		iTargetFlags |= NO_TERRORIST | NO_MULTIPLE;
	const char* pszTarget = "@me";
	if (args.ArgC() >= 2 && (bIsAdmin || g_bLeaderCanTargetPlayers))
		pszTarget = args[1];

	if (!g_playerManager->CanTargetPlayers(player, pszTarget, iNumClients, pSlots, iTargetFlags, nType))
		return;

	for (int i = 0; i < iNumClients; i++)
	{
		CCSPlayerController* pTarget = CCSPlayerController::FromSlot(pSlots[i]);
		ZEPlayer* pPlayerTarget = pTarget->GetZEPlayer();

		if (iNumClients == 1 && player == pTarget)
		{
			ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "%s beacon on yourself.",
						pPlayerTarget->GetBeaconParticle() ? "Disabled" : "Enabled");
		}
		else if (iNumClients == 1)
		{
			ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX "%s %s %s beacon on %s.",
						   bIsAdmin ? "Admin" : "Leader",
						   pszCommandPlayerName,
						   pPlayerTarget->GetBeaconParticle() ? "disabled" : "enabled",
						   pTarget->GetPlayerName());
		}

		if (!pPlayerTarget->GetBeaconParticle())
		{
			Color color = Leader_GetColor(args.ArgC() < 3 ? "" : args[2], pPlayer, pTarget);
			pPlayerTarget->StartBeacon(color, pPlayer ? pPlayer->GetHandle() : 0);
		}
		else
			pPlayerTarget->EndBeacon();
	}

	if (iNumClients > 1) // Can only hit this if bIsAdmin due to target flags
		PrintMultiAdminAction(nType, pszCommandPlayerName, "toggled beacon on", "", CHAT_PREFIX);
}

CON_COMMAND_CHAT(beacons, "- List all active player beacons")
{
	std::pair<int, std::string> beacons = GetCount(3);

	if (beacons.first == 0)
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "There are no active beacons.");
	else
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "%i active beacons: %s", beacons.first, beacons.second.c_str());
}

CON_COMMAND_CHAT_LEADER(enablepings, "- Enable non-leaders pings")
{
	g_bPingWithLeader = true;
	ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "\4Enabled\x01 pings for non-leaders.");
}

CON_COMMAND_CHAT_LEADER(disablepings, "- Disable non-leaders pings")
{
	g_bPingWithLeader = false;
	ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "\2Disabled\x01 pings for non-leaders.");
}

CON_COMMAND_CHAT(leaders, "- List all current leaders")
{
	if (!g_bEnableLeader)
		return;

	std::pair<int, std::string> leaders = GetLeaders();

	if (leaders.first == 0)
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "There are currently no leaders.");
	else
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "%i leaders: %s", leaders.first, leaders.second.c_str());
}

CON_COMMAND_CHAT(leaderhelp, "- List leader commands in chat")
{
	if (!g_bEnableLeader)
		return;

	ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "List of leader commands:");
	ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "!leader [name] [color] - Give another player leader status");
	if (g_bLeaderCanTargetPlayers)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "!beacon <name> [color] - Toggle beacon on a player");
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "!tracer <name> [color] - Toggle projectile tracers on a player");
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "!defend [name|duration] [duration] - Place a defend marker on the target player");
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "!glow <name> [color] - Toggle glow highlight on a player");
	}
	else
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "!beacon - Toggle beacon on yourself");
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "!tracer - Toggle projectile tracers on yourself");
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "!defend [duration] - Place a defend marker on yourself");
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "!glow - Toggle glow highlight on yourself");
	}
	ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "!disablepings - Disable non-leaders pings");
	ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "!leadercolor [color] - List leader colors in chat or change your active leader color");
	ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "!resign - Remove leader status from yourself");
}

CON_COMMAND_CHAT(leadercolor, "[color] - List leader colors in chat or change your leader color")
{
	if (!g_bEnableLeader)
		return;

	ZEPlayer* zpPlayer = player ? player->GetZEPlayer() : nullptr;
	if (zpPlayer && zpPlayer->IsLeader() && args.ArgC() >= 2)
	{
		std::string strColor = args[1];
		std::transform(strColor.begin(), strColor.end(), strColor.begin(), [](unsigned char c) { return std::tolower(c); });
		if (strColor.length() > 0 && mapColorPresets.contains(strColor))
		{
			auto const& colorPreset = mapColorPresets.at(strColor);
			Color color = colorPreset.color;
			zpPlayer->SetLeaderColor(color);

			// Override current leader visuals with the new color they manually picked
			if (zpPlayer->GetGlowModel())
			{
				zpPlayer->EndGlow();
				zpPlayer->StartGlow(color, 0);
			}

			if (zpPlayer->GetTracerColor().a() == 255)
				zpPlayer->SetTracerColor(color);

			if (zpPlayer->GetBeaconParticle())
			{
				zpPlayer->EndBeacon();
				zpPlayer->StartBeacon(color, zpPlayer ? zpPlayer->GetHandle() : 0);
			}

			CCSPlayerPawn* pawnPlayer = (CCSPlayerPawn*)player->GetPawn();
			if (pawnPlayer && pawnPlayer->m_iHealth() > 0 && player->m_iTeamNum == CS_TEAM_CT)
				pawnPlayer->m_clrRender = color;

			ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Set your leader color to %s%s\x01.", colorPreset.strChatColor.c_str(), strColor.c_str());
			return;
		}
	}

	ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "List of leader colors:");

	std::string strColors = "";
	for (auto const& [strColorName, colorPreset] : mapColorPresets)
	{
		strColors.append(colorPreset.strChatColor + strColorName + "\x01, ");
	}
	if (strColors.length() > 2)
		strColors = strColors.substr(0, strColors.length() - 2);

	ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "%s", strColors.c_str());
}

CON_COMMAND_CHAT_LEADER(leader, "[name] [color] - Force leader status on a player")
{
	ZEPlayer* pPlayer = player ? player->GetZEPlayer() : nullptr;
	bool bIsAdmin = pPlayer ? pPlayer->IsAdminFlagSet(FLAG_LEADER) : true;

	if (!bIsAdmin && GetLeaders().first >= g_iMaxLeaders)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "The max amount of leaders has already been reached.");
		return;
	}

	const char* pszCommandPlayerName = player ? player->GetPlayerName() : CONSOLE_NAME;

	int iNumClients = 0;
	int pSlots[MAXPLAYERS];
	ETargetType nType;
	const char* pszTarget = "@me";
	if (args.ArgC() >= 2)
		pszTarget = args[1];

	if (!g_playerManager->CanTargetPlayers(player, pszTarget, iNumClients, pSlots, NO_MULTIPLE | NO_BOT, nType))
		return;

	CCSPlayerController* pTarget = CCSPlayerController::FromSlot(pSlots[0]);
	ZEPlayer* pPlayerTarget = pTarget->GetZEPlayer();

	if (!Leader_SetNewLeader(pPlayerTarget, args.ArgC() < 3 ? "" : args[2]))
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "%s is already a leader.", pTarget->GetPlayerName());
	else
	{
		ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX "%s %s set %s as a leader.",
					   bIsAdmin ? "Admin" : "Leader", pszCommandPlayerName, pTarget->GetPlayerName());
	}
}

CON_COMMAND_CHAT_FLAGS(removeleader, "[name] - Remove leader status from a player", ADMFLAG_GENERIC)
{
	if (!g_bEnableLeader)
		return;

	if (args.ArgC() < 2)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Usage: !removeleader <name>");
		return;
	}

	ZEPlayer* pPlayer = player ? player->GetZEPlayer() : nullptr;
	const char* pszCommandPlayerName = player ? player->GetPlayerName() : CONSOLE_NAME;

	int iNumClients = 0;
	int pSlots[MAXPLAYERS];
	ETargetType nType;

	if (!g_playerManager->CanTargetPlayers(player, args[1], iNumClients, pSlots, NO_MULTIPLE | NO_BOT, nType))
		return;

	CCSPlayerController* pTarget = CCSPlayerController::FromSlot(pSlots[0]);
	ZEPlayer* pPlayerTarget = pTarget->GetZEPlayer();

	if (!pPlayerTarget->IsLeader())
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "%s is not a leader. Use !leaders to list all current leaders.", pTarget->GetPlayerName());
		return;
	}

	pTarget->m_iScore() = pTarget->m_iScore() - 20000;
	pPlayerTarget->SetLeader(false);
	pPlayerTarget->SetLeaderColor(Color(0, 0, 0, 0));
	pPlayerTarget->SetTracerColor(Color(0, 0, 0, 0));
	pPlayerTarget->SetBeaconColor(Color(0, 0, 0, 0));
	pPlayerTarget->SetGlowColor(Color(0, 0, 0, 0));
	FOR_EACH_VEC_BACK(g_vecLeaders, i)
	{
		if (g_vecLeaders[i] == pPlayerTarget)
		{
			g_vecLeaders.Remove(i);
			break;
		}
	}

	CCSPlayerPawn* pPawn = (pTarget->m_iTeamNum != CS_TEAM_CT || !pTarget->IsAlive()) ? nullptr : (CCSPlayerPawn*)pTarget->GetPawn();
	if (pPawn)
		Leader_RemoveLeaderVisuals(pPawn);

	if (player == pTarget)
		ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX "%s resigned from being a leader.", player->GetPlayerName());
	else
		PrintSingleAdminAction(pszCommandPlayerName, pTarget->GetPlayerName(), "removed leader from ", "", CHAT_PREFIX);
}

CON_COMMAND_CHAT(resign, "- Remove leader status from yourself")
{
	if (!g_bEnableLeader)
		return;

	ZEPlayer* pPlayer = player ? player->GetZEPlayer() : nullptr;
	// Only players can use this command at all
	if (!player || !pPlayer)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "You cannot use this command from the server console.");
		return;
	}

	// Check for this inside of command instead of using CON_COMMAND_CHAT_LEADER so that leaders can resign
	// even when they are not CTs and cs2f_leader_actions_ct_only is set to true
	if (!pPlayer->IsLeader())
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "You must be a leader to use this command.");
		return;
	}

	player->m_iScore() = player->m_iScore() - 20000;
	pPlayer->SetLeader(false);
	pPlayer->SetLeaderColor(Color(0, 0, 0, 0));
	pPlayer->SetTracerColor(Color(0, 0, 0, 0));
	pPlayer->SetBeaconColor(Color(0, 0, 0, 0));
	pPlayer->SetGlowColor(Color(0, 0, 0, 0));
	FOR_EACH_VEC_BACK(g_vecLeaders, i)
	{
		if (g_vecLeaders[i] == pPlayer)
		{
			g_vecLeaders.Remove(i);
			break;
		}
	}

	CCSPlayerPawn* pPawn = (player->m_iTeamNum != CS_TEAM_CT || !player->IsAlive()) ? nullptr : (CCSPlayerPawn*)player->GetPawn();
	if (pPawn)
		Leader_RemoveLeaderVisuals(pPawn);

	ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX "%s resigned from being a leader.", player->GetPlayerName());
}