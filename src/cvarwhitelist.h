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

#pragma once

#include "convar.h"
#undef snprintf
#include "vendor/nlohmann/json_fwd.hpp"

using json = nlohmann::json;

extern CConVar<bool> g_cvarConVarWhitelistEnable;

struct WhitelistValue
{
	std::string strConVar;
	bool bEnabled;
};

class CConVarWhitelist
{
public:
	CConVarWhitelist()
	{
		LoadConfig();
	}

	bool LoadConfig();
	bool IsWhitelisted(std::string strName);
	bool RunWhitelistCheck(std::string strName);
	bool IsMapExecuting() { return m_bMapExecuting; }
	void SetMapExecuting(bool bMapExecuting) { m_bMapExecuting = bMapExecuting; }

private:
	bool m_bConfigLoaded = false;
	bool m_bMapExecuting = false;
	std::vector<WhitelistValue> m_vecGlobalWhitelist;
	std::map<std::string, std::vector<WhitelistValue>> m_mapOverrides;
};

extern CConVarWhitelist* g_pConvarWhitelist;