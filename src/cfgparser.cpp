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

#include "cfgparser.h"
#include "commands.h"

#include <fstream>

CCfgParser* g_pCfgParser = nullptr;

void CCfgParser::PreLevelLoad(const char* pszMapName)
{
	// Make sure convars are set before map load, currently used by map migrations
	ExecuteConfigs(pszMapName);
}

void CCfgParser::ApplyGameSettings(const char* pszMapName)
{
	// Execute again for good measure, because this is when configs would normally execute
	ExecuteConfigs(pszMapName);
}

void CCfgParser::ExecuteConfigs(const char* pszMapName)
{
	// Run plugin cfg
	g_pCfgParser->ParseCfg("cs2fixes/cs2fixes");

	// Run custom server cfg
	g_pCfgParser->ParseCfg("cs2fixes/server");

	if (!V_strcmp(pszMapName, ""))
		return;

	// Run map cfg (if present)
	char szCfgPath[MAX_PATH];
	V_snprintf(szCfgPath, sizeof(szCfgPath), "cs2fixes/maps/%s", pszMapName);
	ParseCfg(szCfgPath);
}

void CCfgParser::ParseCfg(const char* pszCfgPath)
{
	char szPath[MAX_PATH];
	V_snprintf(szPath, sizeof(szPath), "%s/csgo/cfg/%s.cfg", Plat_GetGameDirectory(), pszCfgPath);
	std::ifstream cfgFile(szPath);

	if (!cfgFile.is_open())
	{
		Message("Unable to open & execute custom cfg file \"%s\"\n", pszCfgPath);
		return;
	}

	Message("Executing custom cfg file \"%s\"\n", pszCfgPath);

	std::string strCommand;

	while (std::getline(cfgFile, strCommand))
	{
		CCommand args;

		if (!args.Tokenize(strCommand.c_str()) || !args.ArgC())
			continue;

		if (!V_strcasecmp(args[0], "exec_custom"))
		{
			if (args.ArgC() < 2)
				Message("Usage: exec_custom <cfgpath>\n");
			else
				ParseCfg(args[1]);

			continue;
		}

		ConCommandRef command(args[0], true);
		ConVarRefAbstract convar(args[0], true);

		if (command.IsValidRef())
		{
			CCommandContext context(CT_FIRST_SPLITSCREEN_CLIENT, -1);
			command.Dispatch(context, args);
			continue;
		}

		if (convar.IsValidRef())
		{
			// Hold on to ConVarRefAbstract's so they don't get destroyed on next iteration loop, this could cause memory issues with deferred FCVAR_PERFORMING_CALLBACKS cvar's
			ConVarRefAbstract& cachedConvar = m_mapConVars.insert_or_assign(convar.GetAccessIndex(), convar).first->second;

			if (args.ArgC() > 1 && !cachedConvar.SetString(args[1]))
				Message("Failed to execute \"%s %s\"\n", args[0], args[1]);

			continue;
		}

		Message("Unknown command \"%s\"\n", args[0]);
	}
}
