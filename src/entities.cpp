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

#include "entities.h"

#include "entity.h"
#include "entity/ccsplayerpawn.h"
#include "entity/cgameplayerequip.h"

class InputData_t
{
public:
    Z_CBaseEntity* pActivator;
    Z_CBaseEntity* pCaller;
    variant_t      value;
    int            nOutputID;
};

bool StripPlayer(CCSPlayerPawn* pPawn)
{
    const auto pItemServices   = pPawn->m_pItemServices();

    if (!pItemServices)
        return false;

    pItemServices->StripPlayerWeapons(true);

    return true;
}

namespace CGamePlayerEquipHandler
{

void Use(CGamePlayerEquip* pEntity, InputData_t* pInput)
{
    const auto pCaller = pInput->pActivator;

    if (!pCaller || !pCaller->IsPawn())
        return;

    const auto pPawn = reinterpret_cast<CCSPlayerPawn*>(pCaller);

    const auto flags = pEntity->m_spawnflags();

    if (flags & CGamePlayerEquip::SF_PLAYEREQUIP_STRIPFIRST)
    {
        if (!StripPlayer(pPawn))
            return;
    }
    else if (flags & ::CGamePlayerEquip::SF_PLAYEREQUIP_ONLYSTRIPSAME)
    {
        // TODO support strip same flags
    }
}

void TriggerForAllPlayers(CGamePlayerEquip* pEntity, InputData_t* pInput)
{
    const auto flags = pEntity->m_spawnflags();

    if (flags & CGamePlayerEquip::SF_PLAYEREQUIP_STRIPFIRST)
    {
        CCSPlayerPawn* pPawn = nullptr;
        while ((pPawn = reinterpret_cast<CCSPlayerPawn*>(UTIL_FindEntityByClassname(pPawn, "player"))) != nullptr)
        {
            if (pPawn->IsPawn() && pPawn->IsAlive())
                StripPlayer(pPawn);
        }
    }
    else if (flags & CGamePlayerEquip::SF_PLAYEREQUIP_ONLYSTRIPSAME)
    {
        // TODO support strip same flags
    }
}

void TriggerForActivatedPlayer(CGamePlayerEquip* pEntity, InputData_t* pInput)
{
    const auto pCaller   = pInput->pActivator;
    const auto pszWeapon = ((pInput->value.m_type == FIELD_CSTRING || pInput->value.m_type == FIELD_STRING) && pInput->value.m_pszString) ? pInput->value.m_pszString : nullptr;

    if (!pCaller || !pszWeapon || !pCaller->IsPawn())
        return;

    const auto pPawn = reinterpret_cast<CCSPlayerPawn*>(pCaller);
    const auto flags = pEntity->m_spawnflags();

    if (flags & CGamePlayerEquip::SF_PLAYEREQUIP_STRIPFIRST)
    {
        if (!StripPlayer(pPawn))
            return;
    }
    else if (flags & CGamePlayerEquip::SF_PLAYEREQUIP_ONLYSTRIPSAME)
    {
        // TODO
    }

    const auto pItemServices = pPawn->m_pItemServices();

    if (!pItemServices)
        return;

    pItemServices->GiveNamedItem(pszWeapon);
}
} // namespace CGamePlayerEquipHandler