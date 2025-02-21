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

#include "idlemanager.h"
#include "commands.h"
#include <vprof.h>

extern IVEngineServer2* g_pEngineServer2;
extern CGlobalVars* GetGlobals();
extern CPlayerManager* g_playerManager;

CIdleSystem* g_pIdleSystem = nullptr;

CConVar<float> g_cvarIdleKickTime("cs2f_idle_kick_time", FCVAR_NONE, "Amount of minutes before kicking idle players. 0 to disable afk kicking.", 0.0f, true, 0.0f, false, 0.0f);
CConVar<int> g_cvarMinClientsForIdleKicks("cs2f_idle_kick_min_players", FCVAR_NONE, "Minimum amount of connected clients to kick idle players.", 0, true, 0, true, 64);
CConVar<bool> g_cvarKickAdmins("cs2f_idle_kick_admins", FCVAR_NONE, "Whether to kick idle players with ADMFLAG_GENERIC", true);

void CIdleSystem::CheckForIdleClients()
{
	if (m_bPaused || g_cvarIdleKickTime.Get() <= 0.0f || !GetGlobals())
		return;

	int iClientNum = 0;
	for (int i = 0; i < GetGlobals()->maxClients; i++)
	{
		ZEPlayer* zPlayer = g_playerManager->GetPlayer(i);

		if (!zPlayer || zPlayer->IsFakeClient())
			continue;

		iClientNum++;
	}

	for (int i = 0; i < GetGlobals()->maxClients; i++)
	{
		ZEPlayer* zPlayer = g_playerManager->GetPlayer(i);

		if (!zPlayer || zPlayer->IsFakeClient())
			continue;

		if (zPlayer->IsAuthenticated() && !g_cvarKickAdmins.Get())
		{
			auto admin = g_pAdminSystem->FindAdmin(zPlayer->GetSteamId64());
			if (admin && admin->GetFlags() & ADMFLAG_GENERIC)
				continue;
		}

		time_t iIdleTimeLeft = (g_cvarIdleKickTime.Get() * 60) - (std::time(0) - zPlayer->GetLastInputTime());

		if (iIdleTimeLeft > 0)
		{
			if (iIdleTimeLeft < 60)
			{
				CCSPlayerController* pPlayer = CCSPlayerController::FromSlot(zPlayer->GetPlayerSlot());
				if (pPlayer)
					ClientPrint(pPlayer, HUD_PRINTTALK, CHAT_PREFIX "You will be flagged as idle if you do not move within\2 %i\1 seconds.", iIdleTimeLeft);
			}
			continue;
		}

		if (iClientNum < g_cvarMinClientsForIdleKicks.Get())
			continue;

		g_pEngineServer2->DisconnectClient(zPlayer->GetPlayerSlot(), NETWORK_DISCONNECT_KICKED_IDLE, "Auto kicked for being idle");
	}
}

// Logged inputs and time for the logged inputs are updated every time this function is run.
void CIdleSystem::UpdateIdleTimes()
{
	if (g_cvarIdleKickTime.Get() <= 0.0f || !GetGlobals())
		return;

	VPROF("CIdleSystem::UpdateIdleTimes");

	for (int i = 0; i < GetGlobals()->maxClients; i++)
	{
		ZEPlayer* pPlayer = g_playerManager->GetPlayer(i);

		if (!pPlayer)
			continue;

		CBasePlayerController* pTarget = (CBasePlayerController*)g_pEntitySystem->GetEntityInstance((CEntityIndex)(pPlayer->GetPlayerSlot().Get() + 1));
		if (!pTarget)
			continue;

		const auto pPawn = pTarget->m_hPawn();
		if (!pPawn)
			continue;

		const auto pMovement = pPawn->m_pMovementServices();
		uint64 iCurrentMovement = IN_NONE;
		if (pMovement)
		{
			const auto buttonStates = pMovement->m_nButtons().m_pButtonStates();
			iCurrentMovement = buttonStates[0];
		}
		const auto buttonsChanged = pPlayer->GetLastInputs() ^ iCurrentMovement;

		if (!buttonsChanged)
			continue;

		pPlayer->SetLastInputs(iCurrentMovement);
		pPlayer->UpdateLastInputTime();
	}
}

void CIdleSystem::Reset()
{
	m_bPaused = false;

	if (!GetGlobals())
		return;

	for (int i = 0; i < GetGlobals()->maxClients; i++)
	{
		ZEPlayer* pPlayer = g_playerManager->GetPlayer(i);

		if (!pPlayer || pPlayer->IsFakeClient())
			continue;

		pPlayer->SetLastInputs(IN_NONE);
		pPlayer->UpdateLastInputTime();
	}
}