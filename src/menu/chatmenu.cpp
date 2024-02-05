/**
 * =============================================================================
 * CS2Fixes
 * Copyright (C) 2024 Source2ZE
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
#include "chatmenu.h"
#include "../commands.h"

bool ChatMenuInstance::Render(ZEPlayer* player)
{
	if (!BaseMenuInstance::Render(player))
	{
		player->m_pMenuInstance = nullptr;
		return false;
	}

	auto controller = CCSPlayerController::FromSlot(player->GetPlayerSlot());

	auto SendChatMessage = [controller](std::string text) -> void {
		ClientPrint(controller, HUD_PRINTTALK, text.c_str());
	};

	SendChatMessage(" " CHAT_COLOR_SILVER + m_pMenu->m_szTitle);

	int visualOffset = 1;
	for (int i = GetOffset(); i < MIN(GetOffset() + GetVisibleItemCount(), m_pMenu->m_vecItems.size()); i++)
	{
		auto& item = m_pMenu->m_vecItems[i];

		if(item.type == MenuItemDisplayType::Text)
			SendChatMessage(" " CHAT_COLOR_GOLD "!" + std::to_string(visualOffset++) + " " + item.name);
		else if (item.type == MenuItemDisplayType::Spacer)
		{
			SendChatMessage(" ");
			visualOffset++;
		}
		else if(item.type == MenuItemDisplayType::Disabled)
			SendChatMessage(" " CHAT_COLOR_SILVER "!" + std::to_string(visualOffset++) + " " + item.name + " (Disabled)");
	}

	if(HasPrevPage() || HasNextPage() || HasCloseButton())
		SendChatMessage(" ");

	if(HasPrevPage())
		SendChatMessage(" " CHAT_COLOR_GOLD "!7 Prev");
	if (HasNextPage())
		SendChatMessage(" " CHAT_COLOR_GOLD "!8 Next");
	if (HasCloseButton())
		SendChatMessage(" " CHAT_COLOR_GOLD "!9 Close");

	return true;
}

void ChatMenu::Send(ZEPlayer* player)
{
	player->m_pMenuInstance = std::make_unique<ChatMenuInstance>();
	player->m_pMenuInstance->m_pMenu = shared_from_this();

	player->m_pMenuInstance->Render(player);
}