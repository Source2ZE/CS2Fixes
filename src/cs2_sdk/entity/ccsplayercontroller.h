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

#pragma once

#include "../playermanager.h"
#include "../serversideclient.h"
#include "cbaseplayercontroller.h"
#include "services.h"

extern CServerSideClient* GetClientBySlot(CPlayerSlot slot);

extern CGameEntitySystem* g_pEntitySystem;

class CCSPlayerController : public CBasePlayerController
{
public:
	DECLARE_SCHEMA_CLASS(CCSPlayerController);

	SCHEMA_FIELD(CCSPlayerController_InGameMoneyServices*, m_pInGameMoneyServices)
	SCHEMA_FIELD(CCSPlayerController_ActionTrackingServices*, m_pActionTrackingServices)
	SCHEMA_FIELD(uint32_t, m_iPing)
	SCHEMA_FIELD(CUtlSymbolLarge, m_szClan)
	SCHEMA_FIELD_POINTER(char, m_szClanName) // char m_szClanName[32]
	SCHEMA_FIELD(bool, m_bEverFullyConnected)
	SCHEMA_FIELD(bool, m_bPawnIsAlive)
	SCHEMA_FIELD(int32_t, m_nDisconnectionTick)
	SCHEMA_FIELD(CHandle<CCSPlayerPawn>, m_hPlayerPawn)
	SCHEMA_FIELD(CHandle<CCSPlayerPawnBase>, m_hObserverPawn)
	SCHEMA_FIELD(CHandle<CCSPlayerController>, m_hOriginalControllerOfCurrentPawn)
	SCHEMA_FIELD(uint32_t, m_iPawnHealth)
	SCHEMA_FIELD(int32_t, m_iPawnArmor)
	SCHEMA_FIELD(int32_t, m_iScore)
	SCHEMA_FIELD(int32_t, m_iRoundScore)
	SCHEMA_FIELD(int32_t, m_iRoundsWon)
	SCHEMA_FIELD(int32_t, m_iMVPs)
	SCHEMA_FIELD(float, m_flSmoothedPing)

	static CCSPlayerController* FromPawn(CCSPlayerPawn* pawn)
	{
		return (CCSPlayerController*)pawn->m_hController().Get();
	}

	static CCSPlayerController* FromSlot(CPlayerSlot slot)
	{
		return (CCSPlayerController*)g_pEntitySystem->GetEntityInstance(CEntityIndex(slot.Get() + 1));
	}

	// Returns the actual player pawn
	CCSPlayerPawn* GetPlayerPawn()
	{
		return m_hPlayerPawn().Get();
	}

	ZEPlayer* GetZEPlayer()
	{
		return g_playerManager->GetPlayer(GetPlayerSlot());
	}

	CServerSideClient* GetServerSideClient()
	{
		return GetClientBySlot(GetPlayerSlot());
	}

	bool IsBot()
	{
		return m_fFlags() & FL_CONTROLLER_FAKECLIENT;
	}

	void ChangeTeam(int iTeam)
	{
		static int offset = g_GameConfig->GetOffset("CCSPlayerController_ChangeTeam");
		CALL_VIRTUAL(void, offset, this, iTeam);
	}

	void SwitchTeam(int iTeam)
	{
		if (!IsController())
			return;

		// disable hide particle on self for opposite team & enable hide particle on self for teammate
		ZEPlayer* pZEPlayer = GetZEPlayer();
		if (g_bTransparencyParticleReady && pZEPlayer)
		{
			if (iTeam == CS_TEAM_SPECTATOR)
				pZEPlayer->ResetTransparencyMask(true);
			else if (iTeam == CS_TEAM_CT || iTeam == CS_TEAM_T)
			{
				for (int i = 0; i < GetGlobals()->maxClients; i++)
				{
					if (i == GetPlayerSlot())
						continue;

					if (!g_playerManager->IsPlayerUsingTransparency(i))
						continue;

					CCSPlayerController* pPeerController = CCSPlayerController::FromSlot(i);

					if (!pPeerController || pPeerController->m_bIsHLTV)
						continue;
					ZEPlayer* pZEPeerPlayer = pPeerController->GetZEPlayer();
					if (pZEPeerPlayer)
						pZEPeerPlayer->QueuePeerTransparency(pPeerController->m_iTeamNum() == iTeam, GetPlayerSlot());
				}

				// enable hide particle on others for self
				if (g_playerManager->IsPlayerUsingTransparency(GetPlayerSlot()))
				{
					pZEPlayer->ResetTransparencyMask(true);
					pZEPlayer->SetTeamTransparency(true, iTeam);
				}
			}
		}

		if (iTeam == CS_TEAM_SPECTATOR)
			ChangeTeam(iTeam);
		else
			addresses::CCSPlayerController_SwitchTeam(this, iTeam);
	}

	void Respawn()
	{
		CCSPlayerPawn* pPawn = GetPlayerPawn();
		if (!pPawn || pPawn->IsAlive())
			return;

		SetPawn(pPawn);
		static int offset = g_GameConfig->GetOffset("CCSPlayerController_Respawn");
		CALL_VIRTUAL(void, offset, this);
	}

	CSPlayerState GetPawnState()
	{
		// All CS2 pawns are derived from this
		CCSPlayerPawnBase* pPawn = (CCSPlayerPawnBase*)GetPawn();

		// The player is still joining so their pawn doesn't exist yet, and STATE_WELCOME is what they start with
		if (!pPawn)
			return STATE_WELCOME;

		return pPawn->m_iPlayerState();
	}

	CSPlayerState GetPlayerPawnState()
	{
		CCSPlayerPawn* pPawn = GetPlayerPawn();

		// The player is still joining so their pawn doesn't exist yet, and STATE_WELCOME is what they start with
		if (!pPawn)
			return STATE_WELCOME;

		return pPawn->m_iPlayerState();
	}

	CBaseEntity* GetObserverTarget()
	{
		auto pPawn = GetPawn();

		if (!pPawn)
			return nullptr;

		return pPawn->m_pObserverServices->m_hObserverTarget().Get();
	}

	void AddScore(int iScore)
	{
		m_iScore() = m_iScore() + iScore;
	}
};