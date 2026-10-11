/**
 * =============================================================================
 * CS2Fixes
 * Copyright (C) 2023-2026 Source2ZE
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

#include <string>
#include <vector>

class ZEPlayer;
class CCSPlayerController;
class CCommand;

struct EntwatchHudRow
{
	std::string name;
	std::string owner;
	std::string state;
	std::string cooldown;
	std::string ammo;

	bool operator==(const EntwatchHudRow&) const = default;
};

// Keep the item model independent of either HUD entity implementation.
void EW_RenderHud(ZEPlayer* player, const std::vector<EntwatchHudRow>& rows);
void EW_RenderWorldText(ZEPlayer* player, const std::vector<EntwatchHudRow>& rows, bool visible);
void EW_ForgetHudPlayer(int slot);
void EW_DestroyHud();
int EW_DefaultHudStyle();
