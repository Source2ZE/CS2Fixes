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

#include "../schema.h"
#include "cbaseentity.h"

class CGamePlayerEquip : public CBaseEntity
{
	DECLARE_SCHEMA_CLASS(CGamePlayerEquip)
public:

    static constexpr int MAX_EQUIPMENTS_SIZE          = 32;

    static constexpr int SF_PLAYEREQUIP_USEONLY       = 0x0001;
    static constexpr int SF_PLAYEREQUIP_STRIPFIRST    = 0x0002;

    // TODO this flag copied from CSGO, and impl on FyS server. but CS2Fixes not support aws currently.
    // Add it in the future.
    static constexpr int SF_PLAYEREQUIP_ONLYSTRIPSAME = 0x0004; 
};