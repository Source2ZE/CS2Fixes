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
#include "entity.h"

/* logic_relay */

class CPointViewControl : public CBaseEntity
{
    DECLARE_SCHEMA_CLASS(CPointViewControl)

    // hello, don't port my garbage code to cssharp :)

    static constexpr int SF_POINT_VIEWCONTROL_FROZEN = 1 << 5;
    static constexpr int SF_POINT_VIEWCONTROL_FOV    = 1 << 6;
    static constexpr int SF_POINT_VIEWCONTROL_DISARM = 1 << 7;

public:
    [[nodiscard]] CBaseEntity* GetTargetCameraEntity()
    {
        const auto pTarget = UTIL_FindEntityByName(nullptr, m_target().String());
        return pTarget && pTarget->m_pCollision() ? pTarget : nullptr;
    }

    [[nodiscard]] bool HasTargetCameraEntity()
    {
        return m_target().IsValid() && strlen(m_target().String()) >= 2;
    }

    [[nodiscard]] bool HasFrozen()
    {
        return !!(m_spawnflags() & SF_POINT_VIEWCONTROL_FROZEN);
    }

    [[nodiscard]] bool HasFOV()
    {
        return !!(m_spawnflags() & SF_POINT_VIEWCONTROL_FOV);
    }

    [[nodiscard]] bool HasDisarm()
    {
        return !!(m_spawnflags() & SF_POINT_VIEWCONTROL_DISARM);
    }

    [[nodiscard]] uint GetFOV()
    {
        return clamp(static_cast<uint>(m_iHealth()), 16, 179);
    }
};
