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

CON_COMMAND_F(exec_custom, "<cfgpath> - Execute a cfg through the custom cfg parser", FCVAR_SPONLY | FCVAR_LINKED_CONCOMMAND)
{
	if (args.ArgC() < 2)
	{
		Message("Usage: exec_custom <cfgpath>\n");
		return;
	}

	g_pCfgParser->ParseCfg(args[1]);
}

void CCfgParser::ApplyGameSettings(const char* pszMapName)
{
	// Run plugin cfg
	g_pCfgParser->ParseCfg("cs2fixes/cs2fixes");

	// Run custom server cfg
	g_pCfgParser->ParseCfg("cs2fixes/server");

	if (!V_strcmp(pszMapName, ""))
		return;

	// Run map cfg (if present)
	// We call ParseCfg indirectly through exec_custom, so any commands within the map cfg will be added to the command buffer after nested executes in previous configs
	char cmd[MAX_PATH];
	V_snprintf(cmd, sizeof(cmd), "exec_custom cs2fixes/maps/%s", pszMapName);
	g_pEngineServer2->ServerCommand(cmd);
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
		if (!strCommand.empty() && strCommand.back() == '\r')
			strCommand.pop_back();

		if (!strCommand.empty())
			g_pEngineServer2->ServerCommand(strCommand.c_str());
	}
}
