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

#include "KeyValues.h"
#include "commands.h"
#include "common.h"
#include "ctimer.h"
#include "entities.h"
#include "entity/cbaseplayercontroller.h"
#include "entity/cgamerules.h"
#include "eventlistener.h"
#include "leader.h"
#include "networkstringtabledefs.h"
#include "panoramavote.h"
#include "recipientfilters.h"
#include "votemanager.h"
#include "zombiereborn.h"

#include "tier0/memdbgon.h"

extern IGameEventManager2* g_gameEventManager;
extern IServerGameClients* g_pSource2GameClients;
extern CGameEntitySystem* g_pEntitySystem;
extern CGlobalVars* gpGlobals;
extern CCSGameRules* g_pGameRules;
extern IVEngineServer2* g_pEngineServer2;

extern int g_iRoundNum;

CUtlVector<CGameEventListener*> g_vecEventListeners;

void RegisterEventListeners()
{
	static bool bRegistered = false;

	if (bRegistered || !g_gameEventManager)
		return;

	FOR_EACH_VEC(g_vecEventListeners, i)
	{
		g_gameEventManager->AddListener(g_vecEventListeners[i], g_vecEventListeners[i]->GetEventName(), true);
	}

	bRegistered = true;
}

void UnregisterEventListeners()
{
	if (!g_gameEventManager)
		return;

	FOR_EACH_VEC(g_vecEventListeners, i)
	{
		g_gameEventManager->RemoveListener(g_vecEventListeners[i]);
	}

	g_vecEventListeners.Purge();
}

bool g_bPurgeEntityNames = false;
FAKE_BOOL_CVAR(cs2f_purge_entity_strings, "Whether to purge the EntityNames stringtable on new rounds", g_bPurgeEntityNames, false, false);

extern void FullUpdateAllClients();

GAME_EVENT_F(round_prestart)
{
	g_iRoundNum++;

	if (g_bPurgeEntityNames)
	{
		INetworkStringTable* pEntityNames = g_pNetworkStringTableServer->FindTable("EntityNames");

		if (pEntityNames)
		{
			int iStringCount = pEntityNames->GetNumStrings();
			addresses::CNetworkStringTable_DeleteAllStrings(pEntityNames);

			Message("Purged %i strings from EntityNames\n", iStringCount);

			// Vauff: Not fixing cubemap fog in my testing
			// This also breaks round start particle resets, so disabling for now
			// pEntityNames->SetTick(-1, nullptr);

			// FullUpdateAllClients();
		}
	}

	EntityHandler_OnRoundRestart();

	CBaseEntity* pShake = nullptr;

	// Prevent shakes carrying over from previous rounds
	while ((pShake = UTIL_FindEntityByClassname(pShake, "env_shake")))
		pShake->AcceptInput("StopShake");

	if (g_bEnableZR)
		ZR_OnRoundPrestart(pEvent);
}

static bool g_bBlockTeamMessages = false;

FAKE_BOOL_CVAR(cs2f_block_team_messages, "Whether to block team join messages", g_bBlockTeamMessages, false, false)

GAME_EVENT_F(player_team)
{
	// Remove chat message for team changes
	if (g_bBlockTeamMessages)
		pEvent->SetBool("silent", true);
}

static bool g_bNoblock = false;

FAKE_BOOL_CVAR(cs2f_noblock_enable, "Whether to use player noblock, which sets debris collision on every player", g_bNoblock, false, false)

GAME_EVENT_F(player_spawn)
{
	CCSPlayerController* pController = (CCSPlayerController*)pEvent->GetPlayerController("userid");

	if (!pController)
		return;

	ZEPlayer* pPlayer = pController->GetZEPlayer();

	// always reset when player spawns
	if (pPlayer)
		pPlayer->SetMaxSpeed(1.f);

	if (g_bEnableZR)
		ZR_OnPlayerSpawn(pController);

	if (pController->IsConnected())
		pController->GetZEPlayer()->OnSpawn();

	CHandle<CCSPlayerController> hController = pController->GetHandle();

	// Gotta do this on the next frame...
	new CTimer(0.0f, false, false, [hController]() {
		CCSPlayerController* pController = hController.Get();

		if (!pController)
			return -1.0f;

		if (const auto player = pController->GetZEPlayer())
			player->SetSteamIdAttribute();

		if (!pController->m_bPawnIsAlive())
			return -1.0f;

		CBasePlayerPawn* pPawn = pController->GetPawn();

		// Just in case somehow there's health but the player is, say, an observer
		if (!g_bNoblock || !pPawn || !pPawn->IsAlive())
			return -1.0f;

		pPawn->SetCollisionGroup(COLLISION_GROUP_DEBRIS);

		return -1.0f;
	});

	// And this needs even more delay..? Don't even know if this is enough, bug can't be reproduced
	new CTimer(0.1f, false, false, [hController]() {
		CCSPlayerController* pController = hController.Get();

		if (!pController)
			return -1.0f;

		CBasePlayerPawn* pPawn = pController->GetPawn();

		if (pPawn)
		{
			// Fix new haunted CS2 bug? https://www.reddit.com/r/cs2/comments/1glvg9s/thank_you_for_choosing_anubis_airlines/
			// We've seen this several times across different maps at this point
			pPawn->m_vecAbsVelocity = Vector(0, 0, 0);
		}

		return -1.0f;
	});
}

static bool g_bEnableTopDefender = false;

FAKE_BOOL_CVAR(cs2f_topdefender_enable, "Whether to use TopDefender", g_bEnableTopDefender, false, false)

GAME_EVENT_F(player_hurt)
{
	if (g_bEnableZR)
		ZR_OnPlayerHurt(pEvent);

	if (!g_bEnableTopDefender)
		return;

	CCSPlayerController* pAttacker = (CCSPlayerController*)pEvent->GetPlayerController("attacker");
	CCSPlayerController* pVictim = (CCSPlayerController*)pEvent->GetPlayerController("userid");

	// Ignore Ts/zombies and CTs hurting themselves
	if (!pAttacker || pAttacker->m_iTeamNum() != CS_TEAM_CT || pAttacker->m_iTeamNum() == pVictim->m_iTeamNum())
		return;

	ZEPlayer* pPlayer = pAttacker->GetZEPlayer();

	if (!pPlayer)
		return;

	pPlayer->SetTotalDamage(pPlayer->GetTotalDamage() + pEvent->GetInt("dmg_health"));
	pPlayer->SetTotalHits(pPlayer->GetTotalHits() + 1);
}

GAME_EVENT_F(player_death)
{
	if (g_bEnableZR)
		ZR_OnPlayerDeath(pEvent);

	if (!g_bEnableTopDefender)
		return;

	CCSPlayerController* pAttacker = (CCSPlayerController*)pEvent->GetPlayerController("attacker");
	CCSPlayerController* pVictim = (CCSPlayerController*)pEvent->GetPlayerController("userid");

	// Ignore Ts/zombie kills and ignore CT teamkilling or suicide
	if (!pAttacker || !pVictim || pAttacker->m_iTeamNum != CS_TEAM_CT || pAttacker->m_iTeamNum == pVictim->m_iTeamNum)
		return;

	ZEPlayer* pPlayer = pAttacker->GetZEPlayer();

	if (!pPlayer)
		return;

	pPlayer->SetTotalKills(pPlayer->GetTotalKills() + 1);
}

bool g_bFullAllTalk = false;
FAKE_BOOL_CVAR(cs2f_full_alltalk, "Whether to enforce sv_full_alltalk 1", g_bFullAllTalk, false, false);

GAME_EVENT_F(round_start)
{
	g_pPanoramaVoteHandler->Init();

	if (g_bEnableZR)
		ZR_OnRoundStart(pEvent);

	if (g_bEnableLeader)
		Leader_OnRoundStart(pEvent);

	// Dumb workaround for CS2 always overriding sv_full_alltalk on state changes
	if (g_bFullAllTalk)
		g_pEngineServer2->ServerCommand("sv_full_alltalk 1");

	if (!g_bEnableTopDefender)
		return;

	for (int i = 0; i < gpGlobals->maxClients; i++)
	{
		ZEPlayer* pPlayer = g_playerManager->GetPlayer(i);

		if (!pPlayer)
			continue;

		pPlayer->SetTotalDamage(0);
		pPlayer->SetTotalHits(0);
		pPlayer->SetTotalKills(0);
	}
}

GAME_EVENT_F(round_end)
{
	if (g_bVoteManagerEnable)
	{
		ConVar* cvar = g_pCVar->GetConVar(g_pCVar->FindConVar("mp_timelimit"));

		// CONVAR_TODO
		// HACK: values is actually the cvar value itself, hence this ugly cast.
		float flTimelimit = *(float*)&cvar->values;

		int iTimeleft = (int)((g_pGameRules->m_flGameStartTime + flTimelimit * 60.0f) - gpGlobals->curtime);

		// check for end of last round
		if (iTimeleft <= 0)
		{
			g_RTVState = ERTVState::POST_LAST_ROUND_END;
			g_ExtendState = EExtendState::POST_LAST_ROUND_END;
		}
	}

	if (!g_bEnableTopDefender)
		return;

	CUtlVector<ZEPlayer*> sortedPlayers;

	for (int i = 0; i < gpGlobals->maxClients; i++)
	{
		ZEPlayer* pPlayer = g_playerManager->GetPlayer(i);

		if (!pPlayer || pPlayer->GetTotalDamage() == 0)
			continue;

		CCSPlayerController* pController = CCSPlayerController::FromSlot(pPlayer->GetPlayerSlot());

		if (!pController)
			continue;

		sortedPlayers.AddToTail(pPlayer);
	}

	if (sortedPlayers.Count() == 0)
		return;

	sortedPlayers.Sort([](ZEPlayer* const* a, ZEPlayer* const* b) -> int {
		return (*a)->GetTotalDamage() < (*b)->GetTotalDamage();
	});

	ClientPrintAll(HUD_PRINTTALK, " \x09TOP DEFENDERS");

	char colorMap[] = {'\x10', '\x08', '\x09', '\x0B'};

	for (int i = 0; i < sortedPlayers.Count(); i++)
	{
		ZEPlayer* pPlayer = sortedPlayers[i];
		CCSPlayerController* pController = CCSPlayerController::FromSlot(pPlayer->GetPlayerSlot());

		if (i < 5)
			ClientPrintAll(HUD_PRINTTALK, " %c%i. %s \x01- \x07%i DMG \x05(%i HITS & %i KILLS)", colorMap[MIN(i, 3)], i + 1, pController->GetPlayerName(), pPlayer->GetTotalDamage(), pPlayer->GetTotalHits(), pPlayer->GetTotalKills());
		else
			ClientPrint(pController, HUD_PRINTTALK, " \x0C%i. %s \x01- \x07%i DMG \x05(%i HITS & %i KILLS)", i + 1, pController->GetPlayerName(), pPlayer->GetTotalDamage(), pPlayer->GetTotalHits(), pPlayer->GetTotalKills());

		pPlayer->SetTotalDamage(0);
		pPlayer->SetTotalHits(0);
		pPlayer->SetTotalKills(0);
	}
}

GAME_EVENT_F(round_freeze_end)
{
	if (g_bEnableZR)
		ZR_OnRoundFreezeEnd(pEvent);
}

GAME_EVENT_F(round_time_warning)
{
	if (g_bEnableZR)
		ZR_OnRoundTimeWarning(pEvent);
}

GAME_EVENT_F(bullet_impact)
{
	if (g_bEnableLeader)
		Leader_BulletImpact(pEvent);
}

GAME_EVENT_F(vote_cast)
{
	g_pPanoramaVoteHandler->VoteCast(pEvent);
}