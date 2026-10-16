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

#include "tier0/convar.h"
#include "tier0/utlsymboltable.h"
#undef snprintf
#include "vendor/nlohmann/json_fwd.hpp"

using json = nlohmann::json;

extern CConVar<bool> g_cvarConVarWhitelistEnable;

struct WhitelistValue
{
	std::string strConVar;
	bool bEnabled;
};

// Workaround to access CUtlSymbolTable::m_bInsensitive, which is protected
class CUtlSymbolTableAccessor : public CUtlSymbolTable
{
public:
	void SetInsensitive(bool bInsensitive) { m_bInsensitive = bInsensitive; }
};

class CConVarWhitelist
{
public:
	CConVarWhitelist()
	{
		LoadConfig();
	}

	bool LoadConfig();
	void ApplyToGameTable(const char* pszMapName);
	bool IsConfigLoaded() { return m_bConfigLoaded; };

private:
	bool m_bConfigLoaded = false;
	std::vector<WhitelistValue> m_vecGlobalWhitelist;
	std::map<std::string, std::vector<WhitelistValue>> m_mapOverrides;
};

extern CConVarWhitelist* g_pConvarWhitelist;