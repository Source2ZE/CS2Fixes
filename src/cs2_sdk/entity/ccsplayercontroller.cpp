/**
 * =============================================================================
 * CS2Fixes
 * Copyright (C) 2024 Source2ZE
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

#include "ccsplayercontroller.h"
#undef CreateEvent
#include <igameevents.h>
#include <src/eventlistener.h>

extern IGameEventManager2* g_gameEventManager;

void CCSPlayerController::ShowRespawnStatus(const char* str)
{
	IGameEvent* pEvent = g_gameEventManager->CreateEvent("show_survival_respawn_status");

	if (!pEvent)
		return;

	pEvent->SetPlayer("userid", GetPlayerSlot());
	pEvent->SetUint64("duration", 1);
	pEvent->SetString("loc_token", str);

	GetClientEventListener(GetPlayerSlot())->FireGameEvent(pEvent);
	delete pEvent;
}