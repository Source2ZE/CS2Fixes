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

#include "entwatch_hud.h"
#include "commands.h"
#include "cs2fixes.h"
#include "entity/customhudlayout.h"
#include "entwatch.h"
#include "filesystem.h"
#include "playermanager.h"
#include "user_preferences.h"
#include "utils/entity.h"
#include <array>
#include <cerrno>
#include <cmath>
#include <cstdlib>

CConVar<int> g_cvarEntwatchHudStyle("entwatch_hud_style", FCVAR_NONE, "Default HUD renderer: 0 = world text, 1 = Panorama", 1, true, 0, true, 1);
CConVar<bool> g_cvarEntwatchPanoramaDebug("cs2f_entwatch_panorama_debug", FCVAR_NONE, "Log EntWatch HUD creation and Panorama resource diagnostics", false);

namespace
{
	constexpr size_t MaxRows = 64;
	CHandle<CCSCustomHudLayout> s_layout;
	std::array<std::string, 64> s_previous;
	std::array<bool, 64> s_initialized{};
	std::array<int, 64> s_positionX{};
	std::array<int, 64> s_positionY{};
	std::array<int, 64> s_size{};
	std::array<std::vector<EntwatchHudRow>, MAXPLAYERS> s_rows;
	std::array<int, MAXPLAYERS> s_mode{};
	double s_flNextCreateAttempt = 0.0;

	CCSCustomHudLayout* GetPanoramaHud(bool create)
	{
		if (s_layout.Get() || !create)
			return s_layout.Get();
		if (g_flUniversalTime < s_flNextCreateAttempt)
			return nullptr;
		s_flNextCreateAttempt = g_flUniversalTime + 30.0;
		if (!g_pFullFileSystem || !g_pFullFileSystem->FileExists("panorama/layout/custom_game/hud.vxml_c", "GAME") || !g_pFullFileSystem->FileExists("panorama/styles/entwatch/hud.vcss_c", "GAME"))
		{
			Warning("[EntWatch HUD] Missing compiled Panorama resources; using world text. Install the packaged panorama directory on server and clients. Retrying in 30 seconds.\n");
			return nullptr;
		}
		auto layout = CCSCustomHudLayout::Create("hud", "entwatchHud");
		if (!layout)
		{
			Warning("[EntWatch HUD] Failed to create custom_hud_layout; using world text. Retrying in 30 seconds.\n");
			return nullptr;
		}
		s_layout = layout->GetHandle();
		s_initialized.fill(false);
		for (auto& vecRows : s_rows) vecRows.clear();
		layout->Activate(true);
		if (g_cvarEntwatchPanoramaDebug.Get())
			Message("[EntWatch Panorama] Created shared entity=%d layout=%s server resource=%d\n", layout->entindex(), layout->m_strLayout().String(),
					g_pFullFileSystem->FileExists("panorama/layout/custom_game/hud.vxml_c", "GAME"));
		return layout;
	}

	bool RenderPanorama(ZEPlayer* player, const std::vector<EntwatchHudRow>& rows, bool visible)
	{
		auto layout = GetPanoramaHud(visible);
		if (!layout)
			return !visible;
		const int slot = player->GetPlayerSlot().Get();
		if (slot < 0 || slot >= MAXPLAYERS) return false;
		std::string signature = visible ? "on" : "off";
		signature += std::to_string(player->GetEntwatchHudMode());
		const int x = player->GetEntwatchPanoramaHudX();
		const int y = player->GetEntwatchPanoramaHudY();
		const int size = player->GetEntwatchPanoramaHudSize();
		signature += ":" + std::to_string(x) + ":" + std::to_string(y) + ":" + std::to_string(size) + ":";
		if (visible)
		{
			signature += std::to_string(rows.size()) + ":";
			for (size_t i = 0; i < std::min(rows.size(), MaxRows); ++i)
				for (const auto& value : {rows[i].name, rows[i].owner, rows[i].state, rows[i].cooldown, rows[i].ammo})
					signature += std::to_string(value.size()) + ":" + value;
		}
		if (s_initialized[slot] && s_previous[slot] == signature) return true;
		// Remove the previous per-player position before applying the new CSS classes.
		if (s_initialized[slot])
		{
			if (s_positionX[slot] != x)
				layout->SetHasClass("entwatch", "PosX" + std::to_string(s_positionX[slot]), false, slot);
			if (s_positionY[slot] != y)
				layout->SetHasClass("entwatch", "PosY" + std::to_string(s_positionY[slot]), false, slot);
			if (s_size[slot] != size)
				layout->SetHasClass("entwatch", "Size" + std::to_string(s_size[slot]), false, slot);
		}
		if (!s_initialized[slot] || s_positionX[slot] != x)
			layout->SetHasClass("entwatch", "PosX" + std::to_string(x), true, slot);
		if (!s_initialized[slot] || s_positionY[slot] != y)
			layout->SetHasClass("entwatch", "PosY" + std::to_string(y), true, slot);
		if (!s_initialized[slot] || s_size[slot] != size)
			layout->SetHasClass("entwatch", "Size" + std::to_string(size), true, slot);
		s_positionX[slot] = x;
		s_positionY[slot] = y;
		s_size[slot] = size;
		s_initialized[slot] = true;
		s_previous[slot] = signature;
		layout->SetHasClass("entwatch", "Hidden", !visible, slot);
		for (size_t i = 0; i < MaxRows; ++i)
		{
			const std::string id = "item_" + std::to_string(i);
			const bool active = visible && i < rows.size();
			if (!active && i >= s_rows[slot].size()) continue;
			if (active && i < s_rows[slot].size() && rows[i] == s_rows[slot][i] && s_mode[slot] == player->GetEntwatchHudMode()) continue;
			layout->SetHasClass(id, "Hidden", !active, slot);
			if (!active) continue;
			const auto& row = rows[i];
			layout->SetDialogVariableString(id, "item", row.name, slot);
			layout->SetDialogVariableString(id, "owner", row.owner, slot);
			layout->SetDialogVariableString(id, "state", row.state, slot);
			layout->SetDialogVariableString(id, "cooldown", row.cooldown, slot);
			layout->SetDialogVariableString(id, "ammo", row.ammo, slot);
			layout->SetHasClass(id, "NamesOnly", player->GetEntwatchHudMode() == Hud_ItemOnly, slot);
			layout->SetHasClass(id, "Reloading", !row.cooldown.empty(), slot);
			layout->SetHasClass(id, "HasAmmo", !row.ammo.empty(), slot);
		}
		layout->SetDialogVariableString("overflow", "count", std::to_string(rows.size() > MaxRows ? rows.size() - MaxRows : 0), slot);
		layout->SetHasClass("overflow", "Hidden", !visible || rows.size() <= MaxRows, slot);
		s_rows[slot].assign(rows.begin(), rows.begin() + (visible ? std::min(rows.size(), MaxRows) : 0));
		s_mode[slot] = player->GetEntwatchHudMode();
		return true;
	}
} // namespace

int EW_DefaultHudStyle()
{ return g_cvarEntwatchHudStyle.Get(); }

bool EW_ParseHudFloat(const char* pszValue, float& flValue)
{
	if (!pszValue || !*pszValue)
		return false;
	char* pszEnd;
	errno = 0;
	flValue = std::strtof(pszValue, &pszEnd);
	return pszEnd != pszValue && !*pszEnd && errno != ERANGE && std::isfinite(flValue);
}

void ZEPlayer::SetEntwatchPanoramaHudPos(int x, int y)
{
	m_iEntwatchPanoramaX = x >= 0 && x <= 100 ? x : EW_PANORAMA_X_DEFAULT;
	m_iEntwatchPanoramaY = y >= 0 && y <= 100 ? y : EW_PANORAMA_Y_DEFAULT;
	g_pUserPreferencesSystem->SetPreferenceInt(m_slot.Get(), EW_PREF_PANORAMA_X, m_iEntwatchPanoramaX);
	g_pUserPreferencesSystem->SetPreferenceInt(m_slot.Get(), EW_PREF_PANORAMA_Y, m_iEntwatchPanoramaY);
}

void EW_ConfigurePanoramaHudPos(CCSPlayerController* player, const CCommand& args)
{
	if (!player) return;
	auto zePlayer = player->GetZEPlayer();
	if (!zePlayer) return;
	int x = zePlayer->GetEntwatchPanoramaHudX();
	int y = zePlayer->GetEntwatchPanoramaHudY();
	if (args.ArgC() == 2 && !V_stricmp(args[1], "reset"))
	{
		x = EW_PANORAMA_X_DEFAULT;
		y = EW_PANORAMA_Y_DEFAULT;
	}
	else if (args.ArgC() == 3)
	{
		char* endX;
		char* endY;
		const long parsedX = std::strtol(args[1], &endX, 10);
		const long parsedY = std::strtol(args[2], &endY, 10);
		if (!*args[1] || !*args[2] || *endX || *endY || parsedX < 0 || parsedX > 100 || parsedY < 0 || parsedY > 100)
		{
			ClientPrint(player, HUD_PRINTTALK, EW_PREFIX "Use whole screen percentages from 0 to 100: !hudpos <x> <y>");
			return;
		}
		x = static_cast<int>(parsedX);
		y = static_cast<int>(parsedY);
	}
	else if (args.ArgC() != 1)
	{
		ClientPrint(player, HUD_PRINTTALK, EW_PREFIX "Usage: !hudpos [x y|reset]");
		return;
	}
	if (args.ArgC() > 1)
	{
		zePlayer->SetEntwatchPanoramaHudPos(x, y);
		EW_UpdateHud();
	}
	ClientPrint(player, HUD_PRINTTALK, EW_PREFIX "Panorama HUD position: %d%% from left, %d%% from top.", x, y);
}

CON_COMMAND_CHAT(hudpos2, "[x y|reset] - Alias to configure Panorama position regardless of active style")
{
	EW_ConfigurePanoramaHudPos(player, args);
}

void ZEPlayer::SetEntwatchPanoramaHudSize(int size)
{
	m_iEntwatchPanoramaSize = size >= EW_PANORAMA_SIZE_MIN && size <= EW_PANORAMA_SIZE_MAX ? size : EW_PANORAMA_SIZE_DEFAULT;
	g_pUserPreferencesSystem->SetPreferenceInt(m_slot.Get(), EW_PREF_PANORAMA_SIZE, m_iEntwatchPanoramaSize);
}

void EW_ConfigurePanoramaHudSize(CCSPlayerController* player, const CCommand& args)
{
	if (!player) return;
	auto zePlayer = player->GetZEPlayer();
	if (!zePlayer) return;
	int size = zePlayer->GetEntwatchPanoramaHudSize();
	if (args.ArgC() == 2)
	{
		if (!V_stricmp(args[1], "reset"))
			size = EW_PANORAMA_SIZE_DEFAULT;
		else
		{
			char* end;
			const long parsed = std::strtol(args[1], &end, 10);
			if (!*args[1] || *end || parsed < EW_PANORAMA_SIZE_MIN || parsed > EW_PANORAMA_SIZE_MAX)
			{
				ClientPrint(player, HUD_PRINTTALK, EW_PREFIX "Panorama size must be a whole percentage from 50 to 200. Default: 100.");
				return;
			}
			size = static_cast<int>(parsed);
		}
		zePlayer->SetEntwatchPanoramaHudSize(size);
		EW_UpdateHud();
	}
	else if (args.ArgC() != 1)
	{
		ClientPrint(player, HUD_PRINTTALK, EW_PREFIX "Usage: !hudsize [50-200|reset]");
		return;
	}
	ClientPrint(player, HUD_PRINTTALK, EW_PREFIX "Panorama HUD size: %d%%.", size);
}

void EW_DestroyHud()
{
	if (auto layout = s_layout.Get()) layout->Remove();
	s_layout = nullptr;
	s_flNextCreateAttempt = 0.0;
	s_initialized.fill(false);
	for (auto& previous : s_previous) previous.clear();
	for (auto& vecRows : s_rows) vecRows.clear();
}

void EW_ForgetHudPlayer(int slot)
{
	if (slot >= 0 && slot < 64)
	{
		s_initialized[slot] = false;
		s_previous[slot].clear();
		s_rows[slot].clear();
		auto pPlayer = g_playerManager ? g_playerManager->GetPlayer(CPlayerSlot(slot)) : nullptr;
		if (pPlayer)
		{
			if (auto pText = pPlayer->GetEntwatchHud())
				pText->Remove();
			pPlayer->SetEntwatchHud(nullptr);
		}
	}
}

void EW_RenderHud(ZEPlayer* player, const std::vector<EntwatchHudRow>& rows)
{
	if (!player || player->IsFakeClient() || player->GetPlayerSlot().Get() < 0 || player->GetPlayerSlot().Get() >= MAXPLAYERS)
		return;
	const bool visible = g_cvarEnableEntWatch.Get() && g_cvarEnableEntwatchHud.Get() && player->GetEntwatchHudMode() != Hud_None && !rows.empty();
	const bool panorama = player->GetEntwatchHudStyle() == 1;
	const bool bPanoramaReady = RenderPanorama(player, rows, visible && panorama);
	EW_RenderWorldText(player, rows, visible && (!panorama || !bPanoramaReady));
}

CON_COMMAND_CHAT(hudstyle, "[old|new|default] - Switch EntWatch world-text/Panorama renderer")
{
	if (!player)
	{
		ClientPrint(player, HUD_PRINTCONSOLE, "Use entwatch_hud_style 0/1 for the server default.");
		return;
	}
	auto zePlayer = player->GetZEPlayer();
	if (!zePlayer) return;
	int style = zePlayer->GetEntwatchHudStyle() == 0 ? 1 : 0;
	if (args.ArgC() > 2)
	{
		ClientPrint(player, HUD_PRINTTALK, EW_PREFIX "Usage: !hudstyle [old|new|default]");
		return;
	}
	if (args.ArgC() > 1)
	{
		if (!V_stricmp(args[1], "old"))
			style = 0;
		else if (!V_stricmp(args[1], "new"))
			style = 1;
		else if (!V_stricmp(args[1], "default"))
			style = -1;
		else
		{
			ClientPrint(player, HUD_PRINTTALK, EW_PREFIX "Usage: !hudstyle [old|new|default]");
			return;
		}
	}
	zePlayer->SetEntwatchHudStyle(style);
	EW_UpdateHud();
	ClientPrint(player, HUD_PRINTTALK, EW_PREFIX "HUD renderer: %s. Use !hud to toggle visibility.", zePlayer->GetEntwatchHudStyle() ? "Panorama" : "world text");
}
