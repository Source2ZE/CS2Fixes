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

#include "cvarwhitelist.h"
#include "addresses.h"
#include "commands.h"
#include "utils.h"
#undef snprintf
#include "vendor/nlohmann/json.hpp"
#include <fstream>
#include <set>

CConVarWhitelist* g_pConvarWhitelist = nullptr;

CConVar<bool> g_cvarConVarWhitelistEnable("cs2f_cvarwhitelist_enable", FCVAR_NONE, "Whether to enable the custom ConVar whitelist for maps", false);

CON_COMMAND_CHAT_FLAGS(cvarwhitelist_reload, "- Reload the ConVar whitelist config file", ADMFLAG_ROOT)
{
	if (!g_cvarConVarWhitelistEnable.Get())
		return;

	if (g_pConvarWhitelist->LoadConfig() && GetGlobals())
	{
		g_pConvarWhitelist->ApplyToGameTable(GetGlobals()->mapname.ToCStr());
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "ConVar whitelist config reloaded!");
	}
	else
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Failed to reload ConVar whitelist config!");
	}
}

bool CConVarWhitelist::LoadConfig()
{
	m_bConfigLoaded = false;
	m_vecGlobalWhitelist.clear();
	m_mapOverrides.clear();

	const char* pszCvarWhitelistPath = "addons/cs2fixes/configs/cvar_whitelist.jsonc";
	char szPath[MAX_PATH];
	V_snprintf(szPath, sizeof(szPath), "%s%s%s", Plat_GetGameDirectory(), "/csgo/", pszCvarWhitelistPath);
	std::ifstream cvarWhitelistFile(szPath);

	if (!cvarWhitelistFile.is_open())
	{
		Panic("Failed to open %s, ConVar whitelist not loaded!\n", pszCvarWhitelistPath);
		return false;
	}

	json jsonWhitelist = json::parse(cvarWhitelistFile, nullptr, false, true);

	if (jsonWhitelist.is_discarded() || !jsonWhitelist.is_object())
	{
		Panic("Failed parsing JSON from %s, ConVar whitelist not loaded!\n", pszCvarWhitelistPath);
		return false;
	}

	for (auto& [strKey, jsonValue] : jsonWhitelist.items())
	{
		if (jsonValue.is_boolean())
		{
			m_vecGlobalWhitelist.push_back({StringToLower(strKey), jsonValue.get<bool>()});
		}
		else if (jsonValue.is_object())
		{
			std::vector<WhitelistValue> vecWhitelist;

			for (auto& [strConvar, jsonInnerValue] : jsonValue.items())
			{
				if (!jsonInnerValue.is_boolean())
				{
					Panic("Found invalid value in %s, ConVar whitelist not loaded!\n", pszCvarWhitelistPath);
					return false;
				}

				vecWhitelist.push_back({StringToLower(strConvar), jsonInnerValue.get<bool>()});
			}

			m_mapOverrides[strKey] = vecWhitelist;
		}
		else
		{
			Panic("Found invalid value in %s, ConVar whitelist not loaded!\n", pszCvarWhitelistPath);
			return false;
		}
	}

	m_bConfigLoaded = true;
	return true;
}

void CConVarWhitelist::ApplyToGameTable(const char* pszMapName)
{
	if (!g_cvarConVarWhitelistEnable.Get() || !IsConfigLoaded())
		return;

	std::set<std::string> setWhitelist;

	for (const auto& wv : m_vecGlobalWhitelist)
		if (wv.bEnabled)
			setWhitelist.insert(wv.strConVar);

	auto it = m_mapOverrides.find(pszMapName);

	// If a map override is present, it takes priority
	if (it != m_mapOverrides.end())
	{
		for (const auto& wv : it->second)
			if (wv.bEnabled)
				setWhitelist.insert(wv.strConVar);
			else
				setWhitelist.erase(wv.strConVar);
	}

	CUtlSymbolTable* pWhitelistTable = addresses::CCSAddonManager_CommandWhitelist;
	pWhitelistTable->RemoveAll();

	// Modify the games table to be case-insensitive before we add our entries
	static_cast<CUtlSymbolTableAccessor*>(pWhitelistTable)->SetInsensitive(true);

	for (const auto& strConVar : setWhitelist)
		pWhitelistTable->AddString(strConVar.c_str());
}