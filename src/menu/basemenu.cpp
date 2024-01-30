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

void BaseMenu::AddItem(std::string name, MenuItemDisplayType type, MenuItemCallback callback)
{
    MenuItem item = {
        .name = name,
        .callback = callback,
        .type = type
    };

    m_vecItems.push_back(item);
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
    if (iInput == 7 && HasPrevPage())
    {
		PrevPage(player);
	}
    else if (iInput == 8 && HasNextPage())
    {
		NextPage(player);
	}
    else if (iInput == 9 && HasCloseButton())
    {
		player->m_pMenuInstance = nullptr;
	}
    else if (iInput >= 1 && iInput <= GetVisibleItemCount())
    {
		int iIndex = iInput - 1;
		int iRealIndex = iIndex + m_iOffset;
        if (iRealIndex < m_pMenu->m_vecItems.size())
        {
			auto& item = m_pMenu->m_vecItems[iRealIndex];
            if (item.callback)
            {
				item.callback();
			}
		}
	}   
}