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

CUtlVector<CGameEventListener *> g_vecEventListeners;

void RegisterEventListeners()
{
	FOR_EACH_VEC(g_vecEventListeners, i)
	{
		g_gameEventManager->AddListener(g_vecEventListeners[i], g_vecEventListeners[i]->GetEventName(), true);
	}
}

void UnregisterEventListeners()
{
	FOR_EACH_VEC(g_vecEventListeners, i)
	{
		g_gameEventManager->RemoveListener(g_vecEventListeners[i]);
	}

	g_vecEventListeners.Purge();
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

	Message("EVENT FIRED: %s %s\n", pEvent->GetName(), pController->GetPlayerName());
}
