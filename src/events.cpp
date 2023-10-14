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

#include "common.h"
#include "KeyValues.h"
#include "commands.h"
#include "ctimer.h"
#include "eventlistener.h"
#include "entity/cbaseplayercontroller.h"

#include "tier0/memdbgon.h"

extern IGameEventManager2 *g_gameEventManager;
extern IServerGameClients *g_pSource2GameClients;
extern CEntitySystem *g_pEntitySystem;

CUtlVector<CGameEventListener *> g_vecEventListeners;

void RegisterEventListeners()
{
	if (!g_gameEventManager)
		return;

	FOR_EACH_VEC(g_vecEventListeners, i)
	{
		g_gameEventManager->AddListener(g_vecEventListeners[i], g_vecEventListeners[i]->GetEventName(), true);
	}
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

bool g_bForceCT = false;

CON_COMMAND_F(c_force_ct, "toggle forcing CTs on every round", FCVAR_SPONLY | FCVAR_LINKED_CONCOMMAND)
{
	g_bForceCT = !g_bForceCT;

	Message("Forcing CTs on every round is now %s.\n", g_bForceCT ? "ON" : "OFF");
}

GAME_EVENT_F(round_prestart)
{
	if (!g_bForceCT)
		return;

	for (int i = 1; i <= MAXPLAYERS; i++)
	{
		CCSPlayerController *pController = (CCSPlayerController *)g_pEntitySystem->GetBaseEntity(CEntityIndex(i));

		// Only do this for Ts, ignore CTs and specs
		if (!pController || pController->m_iTeamNum() != CS_TEAM_T)
			continue;

		addresses::CCSPlayerController_SwitchTeam(pController, CS_TEAM_CT);
	}
}

bool g_bBlockTeamMessages = true;

CON_COMMAND_F(c_toggle_team_messages, "toggle team messages", FCVAR_SPONLY | FCVAR_LINKED_CONCOMMAND)
{
	g_bBlockTeamMessages = !g_bBlockTeamMessages;
}
GAME_EVENT_F(player_team)
{
	// Remove chat message for team changes
	if (g_bBlockTeamMessages)
		pEvent->SetBool("silent", true);
}

GAME_EVENT_F(player_spawn)
{
	CBasePlayerController *pController = (CBasePlayerController*)pEvent->GetPlayerController("userid");

	if (!pController)
		return;

	CEntityHandle hController = pController->GetHandle();

	// Gotta do this on the next frame...
	new CTimer(0.0f, false, false, [hController]()
	{
		CBasePlayerController *pController = (CBasePlayerController*)Z_CBaseEntity::EntityFromHandle(hController);

		if (!pController)
			return;

		CBasePlayerPawn *pPawn = pController->GetPawn();

		// Just in case somehow there's health but the player is, say, an observer
		if (!pPawn || pPawn->m_iHealth() <= 0 || !g_pSource2GameClients->IsPlayerAlive(pController->GetPlayerSlot()))
			return;

		pPawn->m_pCollision->m_collisionAttribute().m_nCollisionGroup = COLLISION_GROUP_DEBRIS;
		pPawn->m_pCollision->m_CollisionGroup = COLLISION_GROUP_DEBRIS;
		pPawn->CollisionRulesChanged();
	});
}
