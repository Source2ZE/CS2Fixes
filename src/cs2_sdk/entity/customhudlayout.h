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

#include "ccsplayercontroller.h"

enum EHudPanelClassStatus_t : int
{
	k_eHudPanelClassStatus_Undefined = -1,
	k_eHudPanelClassStatus_DoesNotHaveClass = 0,
	k_eHudPanelClassStatus_HasClass = 1,
};

struct HUDPanelDialogVariableString_t
{
private:
	virtual void unk00() {}

public:
	HUDPanelDialogVariableString_t(uint16 nPanelIdIndex, uint16 nDialogVariableIndex, CUtlString sValue, bool bIsSet) :
		m_nPanelIdIndex(nPanelIdIndex), m_nDialogVariableIndex(nDialogVariableIndex), m_sValue(sValue), m_bIsSet(bIsSet)
	{
		// Since we're constructing a new object, the vtable pointer will be incorrect so fix it
		static const auto pVTable = modules::server->FindVirtualTable("HUDPanelDialogVariableString_t");
		((void**)this)[0] = pVTable;
	}

	bool operator==(const HUDPanelDialogVariableString_t& other) const
	{
		return m_nPanelIdIndex == other.m_nPanelIdIndex && m_nDialogVariableIndex == other.m_nDialogVariableIndex;
	}

	uint16 m_nPanelIdIndex;
	uint16 m_nDialogVariableIndex;
	CUtlString m_sValue;
	bool m_bIsSet;
};

struct HUDPanelHasClass_t
{
public:
	HUDPanelHasClass_t(uint16 nPanelIdIndex, uint16 nClassNameIndex, bool bHasClass) :
		m_nPanelIdIndex(nPanelIdIndex), m_nClassNameIndex(nClassNameIndex), m_eClassStatus((EHudPanelClassStatus_t)bHasClass)
	{
	}

	bool operator==(const HUDPanelHasClass_t& other) const
	{
		return m_nPanelIdIndex == other.m_nPanelIdIndex && m_nClassNameIndex == other.m_nClassNameIndex;
	}

	uint16 m_nPanelIdIndex;
	uint16 m_nClassNameIndex;
	EHudPanelClassStatus_t m_eClassStatus;
};

class CCSCustomHudLayoutState
{
public:
	DECLARE_SCHEMA_CLASS_INLINE(CCSCustomHudLayoutState)

	SCHEMA_FIELD(int, m_playerSlot)
	SCHEMA_FIELD(bool, m_bInputCaptureEnabled)
	SCHEMA_FIELD_POINTER(CUtlVector<HUDPanelHasClass_t>, m_vecHasClasses)
	SCHEMA_FIELD_POINTER(CUtlVector<HUDPanelDialogVariableString_t>, m_vecDialogVariableStrings)
};

class CCSCustomHudLayout : public CBaseEntity
{
public:
	DECLARE_SCHEMA_CLASS(CCSCustomHudLayout)

	SCHEMA_FIELD(CUtlSymbolLarge, m_strLayout);
	SCHEMA_FIELD_POINTER(CUtlVector<CCSCustomHudLayoutState>, m_vecPlayerLayoutStates);
	SCHEMA_FIELD_POINTER(CCSCustomHudLayoutState, m_globalLayoutState);
	SCHEMA_FIELD_POINTER(CUtlVector<CUtlString>, m_vecPanelIds);
	SCHEMA_FIELD_POINTER(CUtlVector<CUtlString>, m_vecClassNames);
	SCHEMA_FIELD_POINTER(CUtlVector<CUtlString>, m_vecDialogVariableNames);

	CCSCustomHudLayoutState& GetLayoutState(CCSPlayerController* pController = nullptr)
	{
		return pController ? m_vecPlayerLayoutStates->Element(pController->GetPlayerSlot()) : *m_globalLayoutState;
	}

	void SetHasClass(std::string sPanelId, std::string sClassName, bool bHasClass, CCSPlayerController* pController = nullptr)
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

	void SetDialogVariableString(std::string sPanelId, std::string sVariableName, std::string sValue, CCSPlayerController* pController = nullptr)
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

	void SetInputCaptureEnabled(bool bEnable, CCSPlayerController* pController)
	{
		GetLayoutState(pController).m_bInputCaptureEnabled = bEnable;
	}

	bool IsInputCaptureEnabled(CCSPlayerController* pController)
	{
		return GetLayoutState(pController).m_bInputCaptureEnabled;
	}
};