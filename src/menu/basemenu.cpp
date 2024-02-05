/**
 * =============================================================================
 * CS2Fixes
 * Copyright (C) 2023 Source2ZE
 * Pagination logic courtesy of CounterStrikeSharp
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

#include "../playermanager.h"
#include "basemenu.h"
#include <iostream>
#include <string>

void BaseMenu::AddItem(std::string name, MenuItemDisplayType type, MenuItemCallback callback, ...)
{
    va_list args;
    va_start(args, callback);

    // realistically menu items will be very short
    // AddItem could be called a lot so lets not waste time on calling vsnprintf twice for every item
    char szBuffer[64];
    vsnprintf(szBuffer, sizeof(szBuffer), name.c_str(), args);
    va_end(args);

    MenuItem item = {
        .name = std::string(szBuffer),
        .callback = callback,
        .type = type
    };

    m_vecItems.push_back(item);
}

bool BaseMenuInstance::CheckCondition(ZEPlayer* player)
{
    if (m_pMenu->m_funcCondition) {
        return m_pMenu->m_funcCondition(player);
    }

    return true;
}


bool BaseMenuInstance::Render(ZEPlayer* player)
{
    //ConMsg("BaseMenuInstance::Render\n");

    return CheckCondition(player);
}

void BaseMenuInstance::NextPage(ZEPlayer* player)
{
    m_iPage++;
    m_stackOffsets.push(m_iOffset);
    m_iOffset += GetVisibleItemCount();
    Render(player);
}

void BaseMenuInstance::PrevPage(ZEPlayer* player)
{
    m_iPage--;
    m_iOffset = m_stackOffsets.top();
    m_stackOffsets.pop();
    Render(player);
}

void BaseMenuInstance::HandleInput(ZEPlayer* player, int iInput)
{
    if (!CheckCondition(player))
    {
        player->m_pMenuInstance = nullptr;
        return;
    }

    if (iInput == 7 && HasPrevPage())
        PrevPage(player);
    else if (iInput == 8 && HasNextPage())
        NextPage(player);
    else if (iInput == 9 && HasCloseButton())
        player->m_pMenuInstance = nullptr;
    else if (iInput >= 1 && iInput <= GetVisibleItemCount())
    {
        int iIndex = iInput - 1;
        int iRealIndex = iIndex + m_iOffset;
        if (iRealIndex < m_pMenu->m_vecItems.size())
        {
            auto& item = m_pMenu->m_vecItems[iRealIndex];
            if (item.callback)
                item.callback(player);
 
            // menu might not exist anymore after callback which might have deleted it, by e.g. opening a new menu. Do not access m_pMenu below this point.
            // if the player's menu instance is still the same as this instance, m_pMenu must still exist.
            if (this == player->m_pMenuInstance.get() && m_pMenu->m_bAutoClose)
                player->m_pMenuInstance = nullptr;
        }
    }   
}