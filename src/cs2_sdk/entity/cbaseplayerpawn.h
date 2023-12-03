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

#include "cbaseentity.h"
#include "cbasemodelentity.h"
#include "services.h"

class CBasePlayerPawn : public CBaseModelEntity
{
public:
	DECLARE_SCHEMA_CLASS(CBasePlayerPawn);

	SCHEMA_FIELD(CPlayer_MovementServices*, m_pMovementServices)
	SCHEMA_FIELD(CPlayer_WeaponServices*, m_pWeaponServices)
	SCHEMA_FIELD(CCSPlayer_ItemServices*, m_pItemServices)
	SCHEMA_FIELD(CHandle<CBasePlayerController>, m_hController)

	void TakeDamage(int iDamage)
	{
		if (m_iHealth() - iDamage <= 0)
			CommitSuicide(false, true);
		else
			Z_CBaseEntity::TakeDamage(iDamage);
	}

	void CommitSuicide(bool bExplode, bool bForce)
	{
		static int offset = g_GameConfig->GetOffset("CBasePlayerPawn_CommitSuicide");
		CALL_VIRTUAL(void, offset, this, bExplode, bForce);
	}

	CBasePlayerController *GetController() { return m_hController.Get(); }
};