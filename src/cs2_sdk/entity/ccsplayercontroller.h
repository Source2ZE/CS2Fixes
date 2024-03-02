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

#pragma once

#include "cbaseplayercontroller.h"
#include "services.h"
#include "../playermanager.h"

extern CGameEntitySystem* g_pEntitySystem;

class CCSPlayerController : public CBasePlayerController
{
public:
	DECLARE_SCHEMA_CLASS(CCSPlayerController);

	SCHEMA_FIELD(CCSPlayerController_InGameMoneyServices*, m_pInGameMoneyServices)
	SCHEMA_FIELD(CCSPlayerController_ActionTrackingServices*, m_pActionTrackingServices)
	SCHEMA_FIELD(bool, m_bPawnIsAlive);
	SCHEMA_FIELD(CHandle<CCSPlayerPawn>, m_hPlayerPawn);

	static CCSPlayerController* FromPawn(CCSPlayerPawn* pawn)
	{
		return (CCSPlayerController*)pawn->m_hController().Get();
	}

	static CCSPlayerController* FromSlot(CPlayerSlot slot)
	{
		return (CCSPlayerController*)g_pEntitySystem->GetBaseEntity(CEntityIndex(slot.Get() + 1));
	}

	// Returns the actual player pawn
	CCSPlayerPawn *GetPlayerPawn()
	{
		return m_hPlayerPawn().Get();
	}

	ZEPlayer* GetZEPlayer()
	{
		return g_playerManager->GetPlayer(GetPlayerSlot());
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

		if (iTeam == CS_TEAM_SPECTATOR)
		{
			ChangeTeam(iTeam);
		}
		else
		{
			addresses::CCSPlayerController_SwitchTeam(this, iTeam);
		}
	}

	void Respawn()
	{
		CCSPlayerPawn *pPawn = GetPlayerPawn();
		if (!pPawn || pPawn->IsAlive())
			return;

		SetPawn(pPawn);
		static int offset = g_GameConfig->GetOffset("CCSPlayerController_Respawn");
		CALL_VIRTUAL(void, offset, this);
	}

	CSPlayerState GetPlayerState()
	{
		// All CS2 pawns are derived from this
		CCSPlayerPawnBase *pPawn = (CCSPlayerPawnBase*)GetPawn();

		if (!pPawn)
			return STATE_NONE;

		return pPawn->m_iPlayerState();
	}

	Z_CBaseEntity *GetObserverTarget()
	{
		auto pPawn = GetPawn();

		if (!pPawn)
			return nullptr;

		return pPawn->m_pObserverServices->m_hObserverTarget().Get();
	}
};