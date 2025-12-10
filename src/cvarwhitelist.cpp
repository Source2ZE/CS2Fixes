/**
 * =============================================================================
 * CS2Fixes
 * Copyright (C) 2023-2025 Source2ZE
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
#include "commands.h"
#include "utils.h"
#undef snprintf
#include "vendor/nlohmann/json.hpp"
#include <fstream>

CConVarWhitelist* g_pConvarWhitelist = nullptr;

CConVar<bool> g_cvarConVarWhitelistEnable("cs2f_cvarwhitelist_enable", FCVAR_NONE, "Whether to enable the ConVar whitelist for maps", false);

CON_COMMAND_CHAT_FLAGS(cvarwhitelist_reload, "- Reload the ConVar whitelist config file", ADMFLAG_ROOT)
{
	if (!g_cvarConVarWhitelistEnable.Get())
		return;

	if (g_pConvarWhitelist->LoadConfig())
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "ConVar whitelist config reloaded!");
	else
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Failed to reload ConVar whitelist config!");
}

bool CConVarWhitelist::LoadConfig()
{
	m_bConfigLoaded = false;

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

	if (jsonWhitelist.is_discarded())
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

bool CConVarWhitelist::IsWhitelisted(std::string strName)
{
	auto globalIt = std::find_if(m_vecGlobalWhitelist.begin(), m_vecGlobalWhitelist.end(), [strName](const auto& wv) {
		return wv.strConVar == StringToLower(strName);
	});

	// Cannot load map overrides, only check global whitelist
	if (!GetGlobals())
		return globalIt != m_vecGlobalWhitelist.end() && globalIt->bEnabled;

	auto mapVector = m_mapOverrides[GetGlobals()->mapname.ToCStr()];
	auto mapIt = std::find_if(mapVector.begin(), mapVector.end(), [strName](const auto& wv) {
		return wv.strConVar == StringToLower(strName);
	});

	// If a map override is present, it takes priority
	if (mapIt != mapVector.end())
	{
		if (mapIt->bEnabled)
			return true;
		else
			return false;
	}

	// Or fall back to global whitelist
	if (globalIt != m_vecGlobalWhitelist.end() && globalIt->bEnabled)
		return true;

	// Not whitelisted
	return false;
}

bool CConVarWhitelist::RunWhitelistCheck(std::string strName)
{
	// Listen servers may introduce weird behaviour in this context, so we're just not going to support this feature there
	if (!g_cvarConVarWhitelistEnable.Get() || !m_bConfigLoaded || !g_pEngineServer2->IsDedicatedServer() || !IsMapExecuting())
		return true;

	bool whitelisted = IsWhitelisted(strName);

	if (!whitelisted)
		Message("Blocked %s from executing \"%s\" due to ConVar whitelist violation\n", GetGlobals() ? GetGlobals()->mapname.ToCStr() : "", strName.c_str());

	return whitelisted;
}