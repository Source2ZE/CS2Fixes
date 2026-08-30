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

#include "customhudlayout.h"
#include "entity.h"

std::map<CHandle<CCSCustomHudLayout>, CustomHudClickCallback_t> g_mapClickCallbacks;

CCSCustomHudLayout* CCSCustomHudLayout::Create(std::string sLayout, std::string sTargetName)
{
	CCSCustomHudLayout* pLayout = CreateEntityByName<CCSCustomHudLayout>("custom_hud_layout");

	if (!pLayout)
		return nullptr;

	CEntityKeyValues* pKeyValues = new CEntityKeyValues();
	std::string sLayoutPath = "panorama/layout/custom_game/" + std::string(sLayout) + ".vxml_c";
	pKeyValues->SetString("layout", sLayoutPath.c_str());

	if (!sTargetName.empty())
		pKeyValues->SetString("targetname", sTargetName.c_str());

	pLayout->DispatchSpawn(pKeyValues);
	return pLayout;
}

void CCSCustomHudLayout::HandleClickCallback(CCSPlayerController* pController, CCSUsrMsg_CustomHudClicked message)
{
	CHandle<CCSCustomHudLayout> handle = CBaseHandle::FromPackedInt(message.custom_hud_layout());

	if (!handle.Get())
		return;

	auto iterator = g_mapClickCallbacks.begin();

	while (iterator != g_mapClickCallbacks.end())
	{
		// Clean up stale callbacks while we're at it
		if (!iterator->first.Get())
		{
			iterator = g_mapClickCallbacks.erase(iterator);
		}
		else
		{
			if (iterator->first == handle)
			{
				iterator->second(pController, handle.Get(), message.button_id());
				break;
			}

			iterator++;
		}
	}
}

CCSCustomHudLayoutState& CCSCustomHudLayout::GetLayoutState(CCSPlayerController* pController)
{
	return pController ? m_vecPlayerLayoutStates->Element(pController->GetPlayerSlot()) : *m_globalLayoutState;
}

void CCSCustomHudLayout::SetHasClass(std::string sPanelId, std::string sClassName, bool bHasClass, CCSPlayerController* pController)
{
	auto panelIndex = m_vecPanelIds->Find(sPanelId.c_str());

	if (panelIndex == -1)
		panelIndex = m_vecPanelIds->AddToTail(sPanelId.c_str());

	auto classIndex = m_vecClassNames->Find(sClassName.c_str());

	if (classIndex == -1)
		classIndex = m_vecClassNames->AddToTail(sClassName.c_str());

	auto& layoutState = GetLayoutState(pController);

	HUDPanelHasClass_t hasClass(panelIndex, classIndex, bHasClass);

	auto hasClassIndex = layoutState.m_vecHasClasses->Find(hasClass);

	if (hasClassIndex == -1)
		layoutState.m_vecHasClasses->AddToTail(hasClass);
	else
		layoutState.m_vecHasClasses->Element(hasClassIndex).m_eClassStatus = hasClass.m_eClassStatus;

	layoutState.m_vecHasClasses.NetworkStateChanged();
}

void CCSCustomHudLayout::SetDialogVariableString(std::string sPanelId, std::string sVariableName, std::string sValue, CCSPlayerController* pController)
{
	auto panelIndex = m_vecPanelIds->Find(sPanelId.c_str());

	if (panelIndex == -1)
		panelIndex = m_vecPanelIds->AddToTail(sPanelId.c_str());

	auto variableIndex = m_vecDialogVariableNames->Find(sVariableName.c_str());

	if (variableIndex == -1)
		variableIndex = m_vecDialogVariableNames->AddToTail(sVariableName.c_str());

	auto& layoutState = GetLayoutState(pController);

	HUDPanelDialogVariableString_t dialogVariable(panelIndex, variableIndex, sValue.c_str(), true);

	auto dialogVariableIndex = layoutState.m_vecDialogVariableStrings->Find(dialogVariable);

	if (dialogVariableIndex == -1)
		layoutState.m_vecDialogVariableStrings->AddToTail(dialogVariable);
	else
		layoutState.m_vecDialogVariableStrings->Element(dialogVariableIndex).m_sValue = sValue.c_str();

	layoutState.m_vecDialogVariableStrings.NetworkStateChanged();
}

void CCSCustomHudLayout::SetInputCaptureEnabled(bool bEnable, CCSPlayerController* pController)
{
	GetLayoutState(pController).m_bInputCaptureEnabled = bEnable;
}

bool CCSCustomHudLayout::IsInputCaptureEnabled(CCSPlayerController* pController)
{
	return GetLayoutState(pController).m_bInputCaptureEnabled;
}

void CCSCustomHudLayout::AddClickCallback(CustomHudClickCallback_t callback)
{
	g_mapClickCallbacks.insert({GetHandle(), callback});
}