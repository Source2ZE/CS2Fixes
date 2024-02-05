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

#pragma once
#include "basemenu.h"

class CenterTextMenuInstance;

class CenterTextMenu : public BaseMenu
{
	using BaseMenu::BaseMenu;
public:
	void Send(ZEPlayer* player);
	MenuType GetMenuType() override { return MenuType::CenterText; };
};

class CenterTextMenuInstance : public BaseMenuInstance
{
public:
	bool Render(ZEPlayer* player) override;
	void OnMenuClosed(ZEPlayer* player, bool intentional) override;
private:
	int GetNumItems() const override { return 5; };
};