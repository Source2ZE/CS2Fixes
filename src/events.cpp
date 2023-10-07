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
#include "keyvalues.h"
#include "commands.h"
#include "eventlistener.h"
#include "entity/cbaseplayercontroller.h"

extern IGameEventManager2 *g_gameEventManager;

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
	CCSPlayerController *pController = (CCSPlayerController *)pEvent->GetPlayerController("userid");

	if (!pController)
		return;

	Message("EVENT FIRED: %s %s\n", pEvent->GetName(), pController->GetPlayerName());
}
