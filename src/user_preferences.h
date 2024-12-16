/**
 * =============================================================================
 * CS2Fixes
 * Copyright (C) 2023-2024 Source2ZE
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
#include "common.h"
#include "utlmap.h"
#include "utlstring.h"
#undef snprintf
#include "vendor/nlohmann/json_fwd.hpp"

using json = nlohmann::json;

#include <functional>
#define StorageCallback std::function<void(uint64, CUtlMap<uint32, CPreferenceValue>&)>

#define MAX_PREFERENCE_LENGTH 128

class CPreferenceValue
{
public:
	CPreferenceValue(const char* sKey = "", const char* sValue = "")
	{
		V_strcpy(m_sKey, sKey);
		V_strcpy(m_sValue, sValue);
	}
	char* GetKey() { return m_sKey; };
	char* GetValue() { return m_sValue; };
	void SetKeyValue(const char* sKey, const char* sValue)
	{
		V_strcpy(m_sKey, sKey);
		V_strcpy(m_sValue, sValue);
	}

private:
	char m_sKey[MAX_PREFERENCE_LENGTH];
	char m_sValue[MAX_PREFERENCE_LENGTH];
};

class CUserPreferencesStorage
{
public:
	virtual void LoadPreferences(uint64 iSteamId, StorageCallback cb) = 0;
	virtual void StorePreferences(uint64 iSteamId, CUtlMap<uint32, CPreferenceValue>& preferences, StorageCallback cb) = 0;
};

class CUserPreferencesREST : public CUserPreferencesStorage
{
public:
	void LoadPreferences(uint64 iSteamId, StorageCallback cb);
	void StorePreferences(uint64 iSteamId, CUtlMap<uint32, CPreferenceValue>& preferences, StorageCallback cb);
	void SetPreferencesAPIUrl(const char* sUserPreferencesUrl) { V_strcpy(m_pszUserPreferencesUrl, sUserPreferencesUrl); };
	const char* GetPreferencesAPIUrl() { return (const char*)m_pszUserPreferencesUrl; };
	void JsonToPreferencesMap(json data, CUtlMap<uint32, CPreferenceValue>& preferences);

private:
	char m_pszUserPreferencesUrl[256] = "";
};

class CUserPreferencesSystem
{
public:
	CUserPreferencesSystem()
	{
		for (int i = 0; i < MAXPLAYERS; i++)
		{
			m_mPreferencesMaps[i].SetLessFunc(DefLessFunc(uint32));
			m_mPreferencesLoaded[i] = false;
		}
	}

	void ClearPreferences(int iSlot);
	void PullPreferences(int iSlot);
	const char* GetPreference(int iSlot, const char* sKey, const char* sDefaultValue = "");
	int GetPreferenceInt(int iSlot, const char* sKey, int iDefaultValue = 0);
	float GetPreferenceFloat(int iSlot, const char* sKey, float fDefaultValue = 0.0f);
	void SetPreference(int iSlot, const char* sKey, const char* sValue);
	void SetPreferenceInt(int iSlot, const char* sKey, int iValue);
	void SetPreferenceFloat(int iSlot, const char* sKey, float fValue);
	bool CheckPreferencesLoaded(int iSlot);
	bool PutPreferences(int iSlot, uint64 iSteamId, CUtlMap<uint32, CPreferenceValue>& preferenceData);
	void OnPutPreferences(int iSlot);
	void PushPreferences(int iSlot);

private:
	CUtlMap<uint32, CPreferenceValue> m_mPreferencesMaps[MAXPLAYERS];
	uint64 m_mUserSteamIds[MAXPLAYERS];
	bool m_mPreferencesLoaded[MAXPLAYERS];
};

extern CUserPreferencesStorage* g_pUserPreferencesStorage;
extern CUserPreferencesSystem* g_pUserPreferencesSystem;