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

#include "cbaseplayerpawn.h"

enum CSPlayerState
{
	STATE_ACTIVE = 0x0,
	STATE_WELCOME = 0x1,
	STATE_PICKINGTEAM = 0x2,
	STATE_PICKINGCLASS = 0x3,
	STATE_DEATH_ANIM = 0x4,
	STATE_DEATH_WAIT_FOR_KEY = 0x5,
	STATE_OBSERVER_MODE = 0x6,
	STATE_GUNGAME_RESPAWN = 0x7,
	STATE_DORMANT = 0x8,
	NUM_PLAYER_STATES = 0x9,
};

class CCSPlayerPawnBase : public CBasePlayerPawn
{
public:
	DECLARE_SCHEMA_CLASS(CCSPlayerPawnBase);
	SCHEMA_FIELD(QAngle, m_angEyeAngles)
	SCHEMA_FIELD(CSPlayerState, m_iPlayerState)
	SCHEMA_FIELD(CHandle<CCSPlayerController>, m_hOriginalController)

	CCSPlayerController* GetOriginalController()
	{
		return m_hOriginalController().Get();
	}

	bool IsBot()
	{
		return m_fFlags() & FL_PAWN_FAKECLIENT;
	}
};

class CCSPlayerPawn : public CCSPlayerPawnBase
{
public:
	DECLARE_SCHEMA_CLASS(CCSPlayerPawn);

	SCHEMA_FIELD(float, m_flVelocityModifier)
	SCHEMA_FIELD(CCSPlayer_ActionTrackingServices*, m_pActionTrackingServices)

	[[nodiscard]] CCSPlayer_CameraServices* GetCameraService()
	{
		return reinterpret_cast<CCSPlayer_CameraServices*>(m_pCameraServices());
	}
};