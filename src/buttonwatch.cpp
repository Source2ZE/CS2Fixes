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

#include "buttonwatch.h"

#include "commands.h"
#include "cs2fixes.h"
#include "ctimer.h"
#include "detours.h"
#include "entity.h"
#include "entity/cbaseplayercontroller.h"
#include "entity/ccsplayercontroller.h"
#include "entity/ccsplayerpawn.h"
#include "entity/cgameplayerequip.h"
#include "entity/cgamerules.h"
#include "entity/clogiccase.h"
#include "entity/cpointviewcontrol.h"

CConVar<bool> g_cvarEnableButtonWatch("cs2f_enable_button_watch", FCVAR_NONE, "INCOMPATIBLE WITH CS#. Whether to enable button watch or not.", false,
									  [](CConVar<bool>* cvar, CSplitScreenSlot slot, const bool* new_val, const bool* old_val) {
										  if (!(*new_val) || !SetupFireOutputInternalDetour())
										  {
											  mapIOFunctions.erase("buttonwatch");
											  cvar->Set(false);
										  }
										  else if (!IsButtonWatchEnabled())
											  mapIOFunctions["buttonwatch"] = ButtonWatch;
									  });

CON_COMMAND_CHAT_FLAGS(bw, "- Toggle button watch display", ADMFLAG_GENERIC)
{
	if (!IsButtonWatchEnabled())
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Button watch is disabled on this server.");
		return;
	}

	if (!player)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "You cannot use this command from the server console.");
		return;
	}

	ZEPlayer* zpPlayer = player->GetZEPlayer();
	if (!zpPlayer)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Something went wrong, please wait a moment before trying this command again.");
		return;
	}

	zpPlayer->CycleButtonWatch();

	switch (zpPlayer->GetButtonWatchMode())
	{
		case 0:
			ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "You have\x02 disabled\1 button watch.");
			break;
		case 1:
			ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "You have\x04 enabled\1 button watch in chat.");
			break;
		case 2:
			ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "You have\x04 enabled\1 button watch in console.");
			break;
		case 3:
			ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "You have\x04 enabled\1 button watch in chat and console.");
			break;
	}
}

bool IsButtonWatchEnabled()
{
	return std::any_of(mapIOFunctions.begin(), mapIOFunctions.end(), [](const auto& p) {
		return p.first == "buttonwatch";
	});
}

std::map<int, bool> mapRecentEnts;
void ButtonWatch(const CEntityIOOutput* pThis, CEntityInstance* pActivator, CEntityInstance* pCaller, const CVariant* value, float flDelay)
{
	if (!IsButtonWatchEnabled() || !GetGlobals() || V_stricmp(pThis->m_pDesc->m_pName, "OnPressed") || !pActivator || !((CBaseEntity*)pActivator)->IsPawn() || !pCaller || mapRecentEnts.contains(pCaller->GetEntityIndex().Get()))
		return;

	CCSPlayerController* ccsPlayer = CCSPlayerController::FromPawn(static_cast<CCSPlayerPawn*>(pActivator));
	std::string strPlayerName = ccsPlayer->GetPlayerName();

	ZEPlayer* zpPlayer = ccsPlayer->GetZEPlayer();
	std::string strPlayerID = "";
	if (zpPlayer && !zpPlayer->IsFakeClient())
	{
		strPlayerID = std::to_string(zpPlayer->IsAuthenticated() ? zpPlayer->GetSteamId64() : zpPlayer->GetUnauthenticatedSteamId64());
		strPlayerID = "(" + strPlayerID + ")";
	}

	std::string strButton = std::to_string(pCaller->GetEntityIndex().Get()) + " " + std::string(((CBaseEntity*)pCaller)->GetName());

	for (int i = 0; i < GetGlobals()->maxClients; i++)
	{
		CCSPlayerController* ccsPlayer = CCSPlayerController::FromSlot(i);
		if (!ccsPlayer)
			continue;

		ZEPlayer* zpPlayer = ccsPlayer->GetZEPlayer();
		if (!zpPlayer)
			continue;

		if (zpPlayer->GetButtonWatchMode() % 2 == 1)
			ClientPrint(ccsPlayer, HUD_PRINTTALK, " \x02[BW]\x0C %s\1 pressed button \x0C%s\1", strPlayerName.c_str(), strButton.c_str());
		if (zpPlayer->GetButtonWatchMode() >= 2)
		{
			ClientPrint(ccsPlayer, HUD_PRINTCONSOLE, "------------------------------------ [ButtonWatch] ------------------------------------");
			ClientPrint(ccsPlayer, HUD_PRINTCONSOLE, "Player: %s %s", strPlayerName.c_str(), strPlayerID.c_str());
			ClientPrint(ccsPlayer, HUD_PRINTCONSOLE, "Button: %s", strButton.c_str());
			ClientPrint(ccsPlayer, HUD_PRINTCONSOLE, "---------------------------------------------------------------------------------------");
		}
	}

	// Limit each button to only printing out at most once every 5 seconds
	int iIndex = pCaller->GetEntityIndex().Get();
	mapRecentEnts[iIndex] = true;
	new CTimer(5.0f, true, true, [iIndex]() {
		mapRecentEnts.erase(iIndex);
		return -1.0f;
	});
}