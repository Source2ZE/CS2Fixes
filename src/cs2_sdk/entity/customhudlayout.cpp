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

std::unordered_map<int, CustomHudLayoutCallbacks_t> CCSCustomHudLayout::sm_mapCustomLayoutCallbacks;

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
	pLayout->SetDisconnectCallback(&DefaultOnDisconnect);

	return pLayout;
}

void CCSCustomHudLayout::ClearCallbacks()
{
	sm_mapCustomLayoutCallbacks.clear();
}

void CCSCustomHudLayout::OnClick(CCSPlayerController* pController, const std::string& sButtonId)
{
	if (auto it = sm_mapCustomLayoutCallbacks.find(GetHandle().ToInt()); it != sm_mapCustomLayoutCallbacks.end())
		it->second.m_OnClick(pController, this, sButtonId);
}

void CCSCustomHudLayout::OnClientDisconnect(int slot)
{
	auto iterator = sm_mapCustomLayoutCallbacks.begin();

	while (iterator != sm_mapCustomLayoutCallbacks.end())
	{
		auto pLayout = CHandle<CCSCustomHudLayout>(iterator->first).Get();

		if (!pLayout)
		{
			iterator = sm_mapCustomLayoutCallbacks.erase(iterator);
			continue;
		}

		iterator->second.m_OnDisconnect(pLayout, slot);
		iterator++;
	}
}

void CCSCustomHudLayout::DefaultOnDisconnect(CCSCustomHudLayout* pLayout, int slot)
{
	pLayout->ClearClasses(slot);
	pLayout->ClearDialogVariables(slot);
	pLayout->SetInputCaptureEnabled(false, slot);
}

void CCSCustomHudLayout::OnEntityDeleted()
{
	sm_mapCustomLayoutCallbacks.erase(GetHandle().ToInt());
}

CCSCustomHudLayoutState& CCSCustomHudLayout::GetLayoutState(int nSlot)
{
	if (nSlot < 0 || nSlot >= 64)
		return *m_globalLayoutState;

	return *(CCSCustomHudLayoutState*)m_vecPlayerLayoutStates.GetManipulator()(SCHEMA_COLLECTION_MANIPULATOR_ACTION_GET_ELEMENT, m_vecPlayerLayoutStates, nSlot, 0);
}

CCSCustomHudLayoutState& CCSCustomHudLayout::GetLayoutState(CCSPlayerController* pController)
{
	return pController ? GetLayoutState(pController->GetPlayerSlot()) : *m_globalLayoutState;
}

void CCSCustomHudLayout::SetHasClass(std::string sPanelId, std::string sClassName, bool bHasClass, int nSlot)
{
	auto panelIndex = m_vecPanelIds->Find(sPanelId.c_str());

	if (panelIndex == -1)
		panelIndex = m_vecPanelIds->AddToTail(sPanelId.c_str());

	auto classIndex = m_vecClassNames->Find(sClassName.c_str());

	if (classIndex == -1)
		classIndex = m_vecClassNames->AddToTail(sClassName.c_str());

	auto& layoutState = GetLayoutState(nSlot);

	HUDPanelHasClass_t hasClass(panelIndex, classIndex, bHasClass);

	auto hasClassIndex = layoutState.m_vecHasClasses->Find(hasClass);

	if (hasClassIndex == -1)
		layoutState.m_vecHasClasses->AddToTail(hasClass);
	else
		layoutState.m_vecHasClasses->Element(hasClassIndex).m_eClassStatus = hasClass.m_eClassStatus;
}

void CCSCustomHudLayout::SetHasClass(std::string sPanelId, std::string sClassName, bool bHasClass, CCSPlayerController* pController)
{
	SetHasClass(sPanelId, sClassName, bHasClass, pController ? pController->GetPlayerSlot() : -1);
}

void CCSCustomHudLayout::SetDialogVariableString(std::string sPanelId, std::string sVariableName, std::string sValue, int nSlot)
{
	auto panelIndex = m_vecPanelIds->Find(sPanelId.c_str());

	if (panelIndex == -1)
		panelIndex = m_vecPanelIds->AddToTail(sPanelId.c_str());

	auto variableIndex = m_vecDialogVariableNames->Find(sVariableName.c_str());

	if (variableIndex == -1)
		variableIndex = m_vecDialogVariableNames->AddToTail(sVariableName.c_str());

	auto& layoutState = GetLayoutState(nSlot);

	HUDPanelDialogVariableString_t dialogVariable(panelIndex, variableIndex, sValue.c_str(), true);

	auto dialogVariableIndex = layoutState.m_vecDialogVariableStrings->Find(dialogVariable);

	if (dialogVariableIndex == -1)
	{
		layoutState.m_vecDialogVariableStrings->AddToTail(dialogVariable);
	}
	else
	{
		layoutState.m_vecDialogVariableStrings->Element(dialogVariableIndex).m_sValue = sValue.c_str();
		layoutState.m_vecDialogVariableStrings->Element(dialogVariableIndex).m_bIsSet = true;
	}
}

void CCSCustomHudLayout::SetDialogVariableString(std::string sPanelId, std::string sVariableName, std::string sValue, CCSPlayerController* pController)
{
	SetDialogVariableString(sPanelId, sVariableName, sValue, pController ? pController->GetPlayerSlot() : -1);
}

void CCSCustomHudLayout::ClearClasses(int nSlot)
{
	auto& layoutState = GetLayoutState(nSlot);

	for (int i = 0; i < layoutState.m_vecHasClasses->Count(); i++)
	{
		if (layoutState.m_vecHasClasses->Element(i).m_eClassStatus == k_eHudPanelClassStatus_Undefined)
			continue;

		auto pClass = (HUDPanelHasClass_t*)layoutState.m_vecHasClasses.GetManipulator()(SCHEMA_COLLECTION_MANIPULATOR_ACTION_GET_ELEMENT, layoutState.m_vecHasClasses, i, 0);
		pClass->m_eClassStatus = k_eHudPanelClassStatus_Undefined;
	}
}

void CCSCustomHudLayout::ClearDialogVariables(int nSlot)
{
	auto& layoutState = GetLayoutState(nSlot);

	for (int i = 0; i < layoutState.m_vecDialogVariableStrings->Count(); i++)
	{
		if (!layoutState.m_vecDialogVariableStrings->Element(i).m_bIsSet)
			continue;

		auto pString = (HUDPanelDialogVariableString_t*)layoutState.m_vecDialogVariableStrings.GetManipulator()(SCHEMA_COLLECTION_MANIPULATOR_ACTION_GET_ELEMENT, layoutState.m_vecDialogVariableStrings, i, 0);
		pString->m_bIsSet = false;
		pString->m_sValue = "";
	}
}

void CCSCustomHudLayout::SetInputCaptureEnabled(bool bEnable, int nSlot)
{
	GetLayoutState(nSlot).m_bInputCaptureEnabled = bEnable;
}

void CCSCustomHudLayout::SetInputCaptureEnabled(bool bEnable, CCSPlayerController* pController)
{
	SetInputCaptureEnabled(bEnable, pController ? pController->GetPlayerSlot() : -1);
}

bool CCSCustomHudLayout::IsInputCaptureEnabled(int nSlot)
{
	return GetLayoutState(nSlot).m_bInputCaptureEnabled;
}

bool CCSCustomHudLayout::IsInputCaptureEnabled(CCSPlayerController* pController)
{
	return GetLayoutState(pController).m_bInputCaptureEnabled;
}

void CCSCustomHudLayout::SetClickCallback(CustomHudClickCallback_t callback)
{
	sm_mapCustomLayoutCallbacks[GetHandle().ToInt()].m_OnClick = callback;
}

void CCSCustomHudLayout::SetDisconnectCallback(CustomHudDisconnectCallback_t callback)
{
	sm_mapCustomLayoutCallbacks[GetHandle().ToInt()].m_OnDisconnect = callback;
}
