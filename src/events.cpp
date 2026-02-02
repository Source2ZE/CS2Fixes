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

#include "KeyValues.h"
#include "commands.h"
#include "common.h"
#include "ctimer.h"
#include "entities.h"
#include "entity/cbaseplayercontroller.h"
#include "entity/cgamerules.h"
#include "entwatch.h"
#include "eventlistener.h"
#include "hud_manager.h"
#include "idlemanager.h"
#include "leader.h"
#include "map_votes.h"
#include "mapmigrations.h"
#include "panoramavote.h"
#include "recipientfilters.h"
#include "topdefender.h"
#include "votemanager.h"
#include "zombiereborn.h"

#include "tier0/memdbgon.h"

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

GAME_EVENT_F(round_prestart)
{
	RemoveTimers(TIMERFLAG_ROUND);

	EntityHandler_OnRoundRestart();

	CBaseEntity* pShake = nullptr;

	// Prevent shakes carrying over from previous rounds
	while ((pShake = UTIL_FindEntityByClassname(pShake, "env_shake")))
		pShake->AcceptInput("StopShake");

	if (g_cvarEnableZR.Get())
		ZR_OnRoundPrestart(pEvent);

	if (g_cvarEnableEntWatch.Get())
		EW_RoundPreStart();

	g_pMapMigrations->OnRoundPrestart();
}

CConVar<bool> g_cvarBlockTeamMessages("cs2f_block_team_messages", FCVAR_NONE, "Whether to block team join messages", false);

GAME_EVENT_F(player_team)
{
	// Remove chat message for team changes
	if (g_cvarBlockTeamMessages.Get())
		pEvent->SetBool("silent", true);
}

CConVar<bool> g_cvarNoblock("cs2f_noblock_enable", FCVAR_NONE, "Whether to use player noblock, which sets debris collision on every player", false);
CConVar<int> g_cvarFreeArmor("cs2f_free_armor", FCVAR_NONE, "Whether kevlar (1+) and/or helmet (2) are given automatically", 0, true, 0, true, 2);

GAME_EVENT_F(player_spawn)
{
	CCSPlayerController* pController = (CCSPlayerController*)pEvent->GetPlayerController("userid");

	if (!pController)
		return;

	ZEPlayer* pPlayer = pController->GetZEPlayer();

	// always reset when player spawns
	if (pPlayer)
		pPlayer->SetMaxSpeed(1.f);

	if (g_cvarEnableZR.Get())
		ZR_OnPlayerSpawn(pController);

	if (pController->IsConnected())
		pController->GetZEPlayer()->OnSpawn();

	CHandle<CCSPlayerController> hController = pController->GetHandle();

	// Gotta do this on the next frame...
	CTimer::Create(0.0f, TIMERFLAG_MAP | TIMERFLAG_ROUND, [hController]() {
		CCSPlayerController* pController = hController.Get();

		if (!pController)
			return -1.0f;

		if (const auto player = pController->GetZEPlayer())
			player->SetSteamIdAttribute();

		if (!pController->m_bPawnIsAlive())
			return -1.0f;

		CBasePlayerPawn* pPawn = pController->GetPawn();

		// Just in case somehow there's health but the player is, say, an observer
		if (!g_cvarNoblock.Get() || !pPawn || !pPawn->IsAlive())
			return -1.0f;

		pPawn->SetCollisionGroup(COLLISION_GROUP_DEBRIS);

		return -1.0f;
	});

	CCSPlayerPawn* pPawn = (CCSPlayerPawn*)pController->GetPawn();

	if (!pPawn)
		return;

	CCSPlayer_ItemServices* pItemServices = pPawn->m_pItemServices();

	if (!pItemServices)
		return;

	// Dumb workaround for mp_free_armor breaking kevlar rebuys in buy menu
	if (g_cvarFreeArmor.GetInt() == 1)
		pItemServices->GiveNamedItem("item_kevlar");
	else if (g_cvarFreeArmor.GetInt() == 2)
		pItemServices->GiveNamedItem("item_assaultsuit");
}

GAME_EVENT_F(player_hurt)
{
	if (g_cvarEnableTopDefender.Get())
		TD_OnPlayerHurt(pEvent);
}

GAME_EVENT_F(player_death)
{
	if (g_cvarEnableZR.Get())
		ZR_OnPlayerDeath(pEvent);

	if (g_cvarEnableEntWatch.Get())
		EW_PlayerDeath(pEvent);

	if (g_cvarEnableTopDefender.Get())
		TD_OnPlayerDeath(pEvent);
}

CConVar<bool> g_cvarFullAllTalk("cs2f_full_alltalk", FCVAR_NONE, "Whether to enforce sv_full_alltalk 1", false);

GAME_EVENT_F(round_start)
{
	g_pPanoramaVoteHandler->Init();

	if (g_cvarEnableZR.Get())
		ZR_OnRoundStart(pEvent);

	if (g_cvarEnableLeader.Get())
		Leader_OnRoundStart(pEvent);

	// Dumb workaround for CS2 always overriding sv_full_alltalk on state changes
	if (g_cvarFullAllTalk.Get())
		g_pEngineServer2->ServerCommand("sv_full_alltalk 1");

	// Ensure there's no warmup, because mp_warmup_online_enabled gets randomly ignored for some reason, this is a problem with cs2f_fix_hud_flashing
	if (g_cvarFixHudFlashing.Get() && g_pGameRules && g_pGameRules->m_bWarmupPeriod)
		g_pEngineServer2->ServerCommand("mp_warmup_end");

	if (g_cvarEnableTopDefender.Get())
		TD_OnRoundStart(pEvent);
}

GAME_EVENT_F(round_end)
{
	if (g_cvarFixHudFlashing.Get() && g_pGameRules)
		g_pGameRules->m_bGameRestart = false;

	if (g_cvarEnableTopDefender.Get())
		TD_OnRoundEnd(pEvent);
}

GAME_EVENT_F(round_freeze_end)
{
	if (g_cvarEnableZR.Get())
		ZR_OnRoundFreezeEnd(pEvent);
}

GAME_EVENT_F(round_time_warning)
{
	if (g_cvarEnableZR.Get())
		ZR_OnRoundTimeWarning(pEvent);
}

GAME_EVENT_F(bullet_impact)
{
	if (g_cvarEnableLeader.Get())
		Leader_BulletImpact(pEvent);
}

GAME_EVENT_F(vote_cast)
{
	g_pPanoramaVoteHandler->VoteCast(pEvent);
}

GAME_EVENT_F(cs_win_panel_match)
{
	g_pIdleSystem->PauseIdleChecks();

	if (!g_pMapVoteSystem->IsVoteOngoing())
		g_pMapVoteSystem->StartVote();
}