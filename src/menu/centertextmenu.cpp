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
#include "../playermanager.h"
#include "centertextmenu.h"
#include "../commands.h"


bool CenterTextMenuInstance::Render(ZEPlayer* player)
{
	if (!BaseMenuInstance::Render(player))
	{
		player->m_pMenuInstance = nullptr;
		return false;
	}

	auto controller = CCSPlayerController::FromSlot(player->GetPlayerSlot());
	std::string output;

	auto AppendCenterMessage = [&output](std::string text) -> void {
		output.append(text + "<br>");
	};

	int visualOffset = 1;
	for (int i = GetOffset(); i < MIN(GetOffset() + GetVisibleItemCount(), m_pMenu->m_vecItems.size()); i++)
	{
		auto& item = m_pMenu->m_vecItems[i];

		if (item.type == MenuItemDisplayType::Text)
			AppendCenterMessage("<font color=\"#FFA500\">" + std::to_string(visualOffset++) + ". " + item.name + "</font>");
		else if (item.type == MenuItemDisplayType::Spacer)
		{
			AppendCenterMessage("");
			visualOffset++;
		}
		else if (item.type == MenuItemDisplayType::Disabled)
			AppendCenterMessage("<font color=\"#888888\">" + std::to_string(visualOffset++) + ". " + item.name + "</font>");
	}

	if (HasPrevPage() || HasNextPage() || HasCloseButton())
		AppendCenterMessage("");

	if (HasPrevPage())
		AppendCenterMessage("<font color=\"#FFA500\">7. Prev</font>");
	if (HasNextPage())
		AppendCenterMessage("<font color=\"#FFA500\">8. Next</font>");
	if (HasCloseButton())
		AppendCenterMessage("<font color=\"#FFA500\">9. Close</font>");

	controller->ShowRespawnStatus(output.c_str());
	return true;
}

void CenterTextMenu::Send(ZEPlayer* player)
{
	player->m_pMenuInstance = std::make_unique<CenterTextMenuInstance>();
	player->m_pMenuInstance->m_pMenu = shared_from_this();

	player->m_pMenuInstance->Render(player);
}