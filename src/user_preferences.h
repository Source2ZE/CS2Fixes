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
#include "common.h"
#include "utlstring.h"
#include <functional>
#undef snprintf
#include "vendor/nlohmann/json_fwd.hpp"

#define MAX_PREFERENCE_LENGTH 128

class CPreferenceValue;

using json = nlohmann::json;
using UserPrefsMap_t = std::map<uint32, std::shared_ptr<CPreferenceValue>>;
using StorageCallback_t = std::function<void(uint64, UserPrefsMap_t&)>;

class CPreferenceValue
{
public:
	CPreferenceValue(std::string strKey = "", std::string strValue = "")
	{
		m_strKey = strKey;
		m_strValue = strValue;
	}
	const char* GetKey() { return m_strKey.c_str(); };
	const char* GetValue() { return m_strValue.c_str(); };
	void SetKeyValue(std::string strKey, std::string strValue)
	{
		m_strKey = strKey;
		m_strValue = strValue;
	}

private:
	std::string m_strKey;
	std::string m_strValue;
};

class CUserPreferencesStorage
{
public:
	virtual void LoadPreferences(uint64 iSteamId, StorageCallback_t cb) = 0;
	virtual void StorePreferences(uint64 iSteamId, UserPrefsMap_t& preferences, StorageCallback_t cb) = 0;
};

extern CUserPreferencesStorage* g_pUserPreferencesStorage;

class CUserPreferencesREST : public CUserPreferencesStorage
{
public:
	void LoadPreferences(uint64 iSteamId, StorageCallback_t cb);
	void StorePreferences(uint64 iSteamId, UserPrefsMap_t& preferences, StorageCallback_t cb);
	void JsonToPreferencesMap(json data, UserPrefsMap_t& preferences);
};

class CUserPreferencesSystem
{
public:
	CUserPreferencesSystem()
	{
		for (int i = 0; i < MAXPLAYERS; i++)
		{
			m_mUserSteamIds[i] = 0;
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
	bool PutPreferences(int iSlot, uint64 iSteamId, UserPrefsMap_t& preferenceData);
	void OnPutPreferences(int iSlot);
	void PushPreferences(int iSlot);

private:
	UserPrefsMap_t m_mPreferencesMaps[MAXPLAYERS];
	uint64 m_mUserSteamIds[MAXPLAYERS];
	bool m_mPreferencesLoaded[MAXPLAYERS];
};

extern CUserPreferencesSystem* g_pUserPreferencesSystem;