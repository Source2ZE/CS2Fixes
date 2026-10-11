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

#include "commands.h"
#include "entwatch.h"
#include "entwatch_hud.h"
#include "playermanager.h"
#include "utils/entity.h"

extern CConVar<bool> g_cvarEntwatchPanoramaDebug;

void EW_RenderWorldText(ZEPlayer* player, const std::vector<EntwatchHudRow>& rows, bool visible)
{
	if (!player)
		return;
	auto text = player->GetEntwatchHud();
	if (!text && visible)
	{
		player->CreateEntwatchWorldHud();
		text = player->GetEntwatchHud();
	}
	if (!text)
		return;
	std::string message;
	if (visible)
		for (const auto& row : rows)
		{
			if (!message.empty()) message += "\n";
			message += "[" + row.state + "]" + row.name;
			if (player->GetEntwatchHudMode() == Hud_On) message += ": " + row.owner;
		}
	text->AcceptInput("SetMessage", message.c_str());
}

void ZEPlayer::CreateEntwatchWorldHud()
{
	const bool bDebug = g_cvarEntwatchPanoramaDebug.Get();
	CCSPlayerController* pController = CCSPlayerController::FromSlot(GetPlayerSlot());
	if (!pController)
		return;

	CCSPlayerPawn* pPawn = pController->GetPlayerPawn();
	if (!pPawn)
		return;

	CPointOrient* pOrient = GetPointOrient();
	if (!pOrient)
		return;

	CPointWorldText* pText = GetEntwatchHud();
	if (pText)
	{
		pText->Remove();
		SetEntwatchHud(nullptr);
		pText = nullptr;
	}

	pText = CreateEntityByName<CPointWorldText>("point_worldtext");
	if (!pText)
	{
		static double flNextWarning = 0.0;
		if (g_flUniversalTime >= flNextWarning)
		{
			Warning("[EntWatch HUD] Failed to create point_worldtext for slot %d.\n", GetPlayerSlot().Get());
			flNextWarning = g_flUniversalTime + 30.0;
		}
		return;
	}
	pText->m_bEnabled(true);
	pText->m_bFullbright(true);
	pText->m_flFontSize(GetEntwatchHudSize());

	Color col;
	int a = m_colorEntwatchHud.a();
	if (a == 255) // 254 allows it to be visible through some items
		col.SetColor(m_colorEntwatchHud.r(), m_colorEntwatchHud.g(), m_colorEntwatchHud.b(), 254);
	else
		col.SetColor(m_colorEntwatchHud.r(), m_colorEntwatchHud.g(), m_colorEntwatchHud.b(), a);

	pText->m_Color = col;

	pText->m_flWorldUnitsPerPx(0.005f);
	pText->m_nJustifyVertical(PointWorldTextJustifyVertical_t::POINT_WORLD_TEXT_JUSTIFY_VERTICAL_TOP);

	V_strncpy(pText->m_FontName, "Verdana Bold", 64);

	pText->SetMessage("");

	pText->DispatchSpawn();
	SetEntwatchHud(pText);
	if (bDebug)
		Message("[EntWatch HUD] Created world-text HUD for slot %d.\n", GetPlayerSlot().Get());

	pText->AcceptInput("SetParent", "!activator", pOrient);

	Vector origin = pOrient->GetAbsOrigin();
	QAngle vmangles = pOrient->GetAbsRotation();

	Vector forward;
	Vector right;
	Vector up;
	AngleVectors(vmangles, &forward, &right, &up);

	origin += (forward * 7.0f);

	// -x = move left,  +x = move right
	origin += (right * GetEntwatchHudX());

	// -y = move up,   +y = move down
	origin -= (up * GetEntwatchHudY());

	QAngle angles;
	angles.x = 0.0f;
	angles.y = AngleNormalize(vmangles.y - 90.0f);
	angles.z = AngleNormalize(-vmangles.x + 90.0f);

	pText->Teleport(&origin, &angles, nullptr);
}
