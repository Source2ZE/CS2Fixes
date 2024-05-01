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

class InputData_t;
class CGamePlayerEquip;
class Z_CBaseEntity;
class CGameUI;

namespace CGamePlayerEquipHandler
{
void Use(CGamePlayerEquip* pEntity, InputData_t* pInput);
void TriggerForAllPlayers(CGamePlayerEquip* pEntity, InputData_t* pInput);
bool TriggerForActivatedPlayer(CGamePlayerEquip* pEntity, InputData_t* pInput);
} // namespace CGamePlayerEquipHandler

namespace CGameUIHandler
{
bool OnActivate(CGameUI* pEntity, Z_CBaseEntity* pActivator);
bool OnDeactivate(CGameUI* pEntity, Z_CBaseEntity* pActivator);
void RunThink(int tick);
} // namespace CGameUIHandler

void EntityHandler_OnGameFramePre(bool simulate, int tick);
void EntityHandler_OnGameFramePost(bool simulate, int tick);