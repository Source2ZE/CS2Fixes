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

#include "ccsplayerpawn.h"
#include "../ctimer.h"

// Silly workaround for an animation bug that's been happening since 2024-11-06 CS2 update
// Clients need to see the new playermodel with zero velocity for at least (two?) ticks to properly render animations
void CCSPlayerPawn::FixPlayerModelAnimations()
{
	if (m_nActualMoveType() < MOVETYPE_WALK)
		return;

	CHandle<CCSPlayerPawn> hPawn = GetHandle();
	Vector originalVelocity = m_vecAbsVelocity;

	Teleport(nullptr, nullptr, &vec3_origin);
	SetMoveType(MOVETYPE_OBSOLETE);

	CTimer::Create(0.02f, TIMERFLAG_MAP | TIMERFLAG_ROUND, [hPawn, originalVelocity]() {
		CCSPlayerPawn* pPawn = hPawn.Get();

		if (!pPawn || !pPawn->IsAlive())
			return -1.0f;

		pPawn->SetMoveType(MOVETYPE_WALK);
		pPawn->Teleport(nullptr, nullptr, &originalVelocity);

		return -1.0f;
	});
}