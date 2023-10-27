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

#pragma once

#include "cbaseplayercontroller.h"
#include "services.h"
#include "../playermanager.h"

extern CEntitySystem* g_pEntitySystem;

class CCSPlayerController : public CBasePlayerController
{
public:
	DECLARE_SCHEMA_CLASS(CCSPlayerController);

	SCHEMA_FIELD(CCSPlayerController_InGameMoneyServices*, m_pInGameMoneyServices)
	SCHEMA_FIELD(CCSPlayerController_ActionTrackingServices*, m_pActionTrackingServices)
	SCHEMA_FIELD(bool, m_bPawnIsAlive);

	static CCSPlayerController* FromPawn(CCSPlayerPawn* pawn) {
		return (CCSPlayerController*)pawn->m_hController().Get();
	}

	static CCSPlayerController* FromSlot(CPlayerSlot slot)
	{
		return (CCSPlayerController*)g_pEntitySystem->GetBaseEntity(CEntityIndex(slot.Get() + 1));
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
};