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

class CEntityIdentity;
class CEntityInstance;
class CBaseEntity;
class CCSPlayerPawn;
enum DamageTypes_t : unsigned int;

bool CustomIO_HandleInput(CEntityInstance* pEntityInstance,
						  const char* pParams,
						  CEntityInstance* pActivator,
						  CEntityInstance* pCaller);

bool IgnitePawn(CCSPlayerPawn* pEntity,
				float flDuration,
				CBaseEntity* pInflictor = nullptr,
				CBaseEntity* pAttacker = nullptr,
				CBaseEntity* pAbility = nullptr,
				DamageTypes_t nDamageType = DamageTypes_t(8)); // DMG_BURN
