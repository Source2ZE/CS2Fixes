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

#include "commands.h"
#include "common.h"
#include "user_preferences.h"
#include "httpmanager.h"
#include "playermanager.h"
#include "strtools.h"
#include <string>
#undef snprintf
#include "vendor/nlohmann/json.hpp"

using json = nlohmann::json;


CUserPreferencesStorage* g_pUserPreferencesStorage = nullptr;
CUserPreferencesSystem* g_pUserPreferencesSystem = nullptr;

// CONVAR_TODO
CON_COMMAND_F(cs2f_user_prefs_api, "API for user preferences, currently a REST API.", FCVAR_LINKED_CONCOMMAND | FCVAR_SPONLY | FCVAR_PROTECTED)
{
	if (!g_pUserPreferencesSystem || !g_pUserPreferencesStorage) {
		Message("The user preferences subsystem is not enabled.");
		return;
	}

	CUserPreferencesREST* restStorageSystem = (CUserPreferencesREST*) g_pUserPreferencesStorage;
	if (args.ArgC() < 2)
		Message("Usage: %s <url>. Current value: %s\n", args[0], restStorageSystem->GetPreferencesAPIUrl());
	else {
		Message("Setting preferences URL to %s\n", args[1]);
		restStorageSystem->SetPreferencesAPIUrl(args[1]);
	}
}

CON_COMMAND_CHAT_FLAGS(pullprefs, "Pull preferences.", ADMFLAG_ROOT)
{
	ZEPlayer* pPlayer = player->GetZEPlayer();
	if (!pPlayer) return;

	g_pUserPreferencesSystem->PullPreferences(pPlayer->GetPlayerSlot().Get());
}

CON_COMMAND_CHAT_FLAGS(setpref, "Set a preference key for the player.", ADMFLAG_ROOT)
{
	if (args.ArgC() < 3)
		Message("Usage: %s <pref> <value>\n", args[0]);
	else {
		ZEPlayer* pPlayer = player->GetZEPlayer();
		if (!pPlayer) return;

		g_pUserPreferencesSystem->SetPreference(pPlayer->GetPlayerSlot().Get(), args[1], args[2]);
	}
}

CON_COMMAND_CHAT_FLAGS(getpref, "Get a preference key for the player.", ADMFLAG_ROOT)
{
	if (args.ArgC() < 2)
		Message("Usage: %s <pref>\n", args[0]);
	else {
		ZEPlayer* pPlayer = player->GetZEPlayer();
		if (!pPlayer) return;

		const char* sValue = g_pUserPreferencesSystem->GetPreference(pPlayer->GetPlayerSlot().Get(), args[1]);
		Message("The setting is set to: %s\n", sValue);
	}
}

CON_COMMAND_CHAT_FLAGS(pushprefs, "Push preferences.", ADMFLAG_ROOT)
{
	ZEPlayer* pPlayer = player->GetZEPlayer();
	if (!pPlayer) return;

	g_pUserPreferencesSystem->PushPreferences(pPlayer->GetPlayerSlot().Get());
}

void CUserPreferencesSystem::ClearPreferences(int iSlot) 
{
	m_mUserSteamIds[iSlot] = 0;
	m_mPreferencesLoaded[iSlot] = false;
	m_mPreferencesMaps[iSlot].Purge();
}

bool CUserPreferencesSystem::PutPreferences(int iSlot, uint64 iSteamId, CUtlMap<uint32, CPreferenceValue> &preferenceData)
{
	ZEPlayer* player = g_playerManager->GetPlayer(CPlayerSlot(iSlot));
	if (!player || !player->IsAuthenticated()) return false;
	if (iSteamId != player->GetSteamId64()) {
		return false;
	}
#ifdef _DEBUG
	Message("Putting data for %llu\n", iSteamId);
#endif
	m_mUserSteamIds[iSlot] = iSteamId;
	m_mPreferencesLoaded[iSlot] = true;
	FOR_EACH_MAP(preferenceData, i) {
		uint32 iKeyHash = preferenceData.Key(i);
		int iValueIdx = preferenceData.Find(iKeyHash);
		if (iValueIdx == preferenceData.InvalidIndex())
			continue;
		m_mPreferencesMaps[iSlot].InsertOrReplace(iKeyHash, preferenceData[iValueIdx]);
	}
	return true;
} 

void CUserPreferencesSystem::OnPutPreferences(int iSlot)
{
	int iHideDistance = GetPreferenceInt(iSlot, HIDE_DISTANCE_PREF_KEY_NAME, 0);
	int iSoundStatus = GetPreferenceInt(iSlot, SOUND_STATUS_PREF_KEY_NAME, 1);
	bool bStopSound = (bool) (iSoundStatus & 1);
	bool bSilenceSound = (bool) (iSoundStatus & 2);
	bool bHideDecals = (bool) GetPreferenceInt(iSlot, DECAL_PREF_KEY_NAME, 1);

	// Set the values that we just loaded --- the player is guaranteed available
	g_playerManager->SetPlayerStopSound(iSlot, bStopSound);
	g_playerManager->SetPlayerSilenceSound(iSlot, bSilenceSound);
	g_playerManager->SetPlayerStopDecals(iSlot, bHideDecals);

	ZEPlayer* player = g_playerManager->GetPlayer(CPlayerSlot(iSlot));
	player->SetHideDistance(iHideDistance);
}

void CUserPreferencesSystem::PullPreferences(int iSlot) 
{
	if (!g_pUserPreferencesStorage) return;

	// Ignore non-authenticated players and get the authenticated SteamID
	ZEPlayer* player = g_playerManager->GetPlayer(CPlayerSlot(iSlot));
	if (!player || !player->IsAuthenticated()) return;
	uint64 iSteamId = player->GetSteamId64();

	g_pUserPreferencesStorage->LoadPreferences(
		iSteamId,
		[iSlot](uint64 iSteamId, CUtlMap<uint32, CPreferenceValue>& preferenceData) {
			if (g_pUserPreferencesSystem->PutPreferences(iSlot, iSteamId, preferenceData)) {
				g_pUserPreferencesSystem->OnPutPreferences(iSlot);
			}
		}
	);
}

const char* CUserPreferencesSystem::GetPreference(int iSlot, const char* sKey, const char* sDefaultValue)
{
	uint32 iKeyHash = hash_32_fnv1a_const(sKey);
	int iKeyIdx = m_mPreferencesMaps[iSlot].Find(iKeyHash);
#ifdef _DEBUG
	Message("User at slot %d is reading from preference '%s' with hash %d at index %d.\n", iSlot, sKey, iKeyHash, iKeyIdx);
#endif
	if (iKeyIdx == m_mPreferencesMaps[iSlot].InvalidIndex()) return sDefaultValue;
	return (const char*) m_mPreferencesMaps[iSlot][iKeyIdx].GetValue();
}

int CUserPreferencesSystem::GetPreferenceInt(int iSlot, const char* sKey, int iDefaultValue)
{
	const char* pszPreferenceValue = GetPreference(iSlot, sKey, "");
	if (*pszPreferenceValue == '\0')
		return iDefaultValue;
	return V_StringToInt32(pszPreferenceValue, iDefaultValue);
}

float CUserPreferencesSystem::GetPreferenceFloat(int iSlot, const char* sKey, float fDefaultValue)
{
	const char* pszPreferenceValue = GetPreference(iSlot, sKey, "");
	if (*pszPreferenceValue == '\0')
		return fDefaultValue;
	return V_StringToFloat32(pszPreferenceValue, fDefaultValue);
}

void CUserPreferencesSystem::SetPreference(int iSlot, const char* sKey, const char* sValue)
{
	uint32 iKeyHash = hash_32_fnv1a_const(sKey);
#ifdef _DEBUG
	Message("User at slot %d is storing in preference '%s' with hash %d value '%s'.\n", iSlot, sKey, iKeyHash, sValue);
#endif

	// Create or populate the content of the preference value
	int iValueIdx = m_mPreferencesMaps[iSlot].Find(iKeyHash);
	CPreferenceValue* prefValue;
	if (iValueIdx == m_mPreferencesMaps[iSlot].InvalidIndex()) {
		prefValue = new CPreferenceValue(sKey, sValue);
	} else {
		prefValue = &m_mPreferencesMaps[iSlot][iValueIdx];
		prefValue->SetKeyValue(sKey, sValue);
	}

    // Override the key-value pair and insert
	m_mPreferencesMaps[iSlot].InsertOrReplace(iKeyHash, *prefValue);
}

void CUserPreferencesSystem::SetPreferenceInt(int iSlot, const char* sKey, int iValue)
{
	char sPreferenceString[MAX_PREFERENCE_LENGTH];
	V_snprintf(sPreferenceString, sizeof(sPreferenceString), "%d", iValue);
	SetPreference(iSlot, sKey, (const char*) sPreferenceString);
}

void CUserPreferencesSystem::SetPreferenceFloat(int iSlot, const char* sKey, float fValue)
{
	char sPreferenceString[MAX_PREFERENCE_LENGTH];
	V_snprintf(sPreferenceString, sizeof(sPreferenceString), "%f", fValue);
	SetPreference(iSlot, sKey, (const char*) sPreferenceString);
}

bool CUserPreferencesSystem::CheckPreferencesLoaded(int iSlot)
{
	ZEPlayer* player = g_playerManager->GetPlayer(CPlayerSlot(iSlot));
	if (!player || !player->IsAuthenticated()) return false;
	return m_mPreferencesLoaded[iSlot];
}

void CUserPreferencesSystem::PushPreferences(int iSlot)
{
	if (!g_pUserPreferencesStorage) return;

	// Fetch the slot and only 'push' if the player has already loaded
	if (!m_mPreferencesLoaded[iSlot]) return;
	uint64 iSteamId = m_mUserSteamIds[iSlot];

	g_pUserPreferencesStorage->StorePreferences(
		iSteamId,
		m_mPreferencesMaps[iSlot],
		[iSlot](uint64 iSteamId, CUtlMap<uint32, CPreferenceValue>& preferenceData) {
			if (g_pUserPreferencesSystem->PutPreferences(iSlot, iSteamId, preferenceData)) {
				g_pUserPreferencesSystem->OnPutPreferences(iSlot);
			}
		}
	);
}

void CUserPreferencesREST::JsonToPreferencesMap(json data, CUtlMap<uint32, CPreferenceValue> &preferencesMap)
{
	preferencesMap.SetLessFunc(DefLessFunc(uint32));
	for (auto it = data.begin(); it != data.end(); ++it) {
		std::string key = it.key();
		std::string value = it.value();

		// Prepare the key and value pair, and also the key name via hashes
#ifdef _DEBUG
	Message("- Storing KV-pair: %s, %s\n", key.c_str(), value.c_str());
#endif
		CPreferenceValue* prefValue = new CPreferenceValue(key.c_str(), value.c_str());

		// Store key, value, and key names
		uint32 iKeyHash = hash_32_fnv1a_const(prefValue->GetKey());
		preferencesMap.InsertOrReplace(iKeyHash, *prefValue);
	}
}

void CUserPreferencesREST::LoadPreferences(uint64 iSteamId, StorageCallback cb)
{
#ifdef _DEBUG
	Message("Loading data for %llu\n", iSteamId);
#endif
	if (m_pszUserPreferencesUrl[0] == '\0') return;

	// Submit the request to pull the user data
	char sUserPreferencesUrl[256];
	V_snprintf(sUserPreferencesUrl, sizeof(sUserPreferencesUrl), "%s%llu", m_pszUserPreferencesUrl, iSteamId);
	g_HTTPManager.GET(sUserPreferencesUrl, [iSteamId, cb](HTTPRequestHandle request, json data) {
#ifdef _DEBUG
	Message("Executing storage callback during load for %llu\n", iSteamId);
#endif
		CUtlMap<uint32, CPreferenceValue> preferencesMap;
		((CUserPreferencesREST*) g_pUserPreferencesStorage)->JsonToPreferencesMap(data, preferencesMap);
		cb(iSteamId, preferencesMap);
		preferencesMap.Purge();
	});
}

void CUserPreferencesREST::StorePreferences(uint64 iSteamId, CUtlMap<uint32, CPreferenceValue> &preferences, StorageCallback cb)
{
#ifdef _DEBUG
	Message("Storing data for %llu\n", iSteamId);
#endif
	if (m_pszUserPreferencesUrl[0] == '\0') return;

	// Create the JSON object with all key-value pairs
	json sJsonObject = json::object();
	FOR_EACH_MAP(preferences, i) {
		uint32 iKeyHash = preferences.Key(i);
		int iValueIdx = preferences.Find(iKeyHash);
		if (iValueIdx == preferences.InvalidIndex())
			continue;
		CPreferenceValue prefValue = preferences[iValueIdx];
		sJsonObject[prefValue.GetKey()] = prefValue.GetValue();
	}

	// Prepare the API URL to send the request to
	char sUserPreferencesUrl[256];
	V_snprintf(sUserPreferencesUrl, sizeof(sUserPreferencesUrl), "%s%llu", m_pszUserPreferencesUrl, iSteamId);
	
	// Dump the Json object and submit the POST request
	std::string sDumpedJson = sJsonObject.dump();
	g_HTTPManager.POST(sUserPreferencesUrl, sDumpedJson.c_str(), [iSteamId, cb](HTTPRequestHandle request, json data) {
#ifdef _DEBUG
	Message("Executing storage callback during store for %llu\n", iSteamId);
#endif
		CUtlMap<uint32, CPreferenceValue> preferencesMap;
		((CUserPreferencesREST*) g_pUserPreferencesStorage)->JsonToPreferencesMap(data, preferencesMap);
		cb(iSteamId, preferencesMap);
		preferencesMap.Purge();
	});
}