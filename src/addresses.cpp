/**
 * =============================================================================
 * CS2Fixes
 * Copyright (C) 2023 Source2ZE
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

#include "addresses.h"
#include "utils/module.h"
#include <interfaces/cs2_interfaces.h>

#include "tier0/memdbgon.h"


#define RESOLVE_SIG(gameConfig, name, variable) \
	variable = (decltype(variable))gameConfig->ResolveSignature(name)

void addresses::Initialize(CGameConfig *g_GameConfig)
{
	modules::engine = new CModule(ROOTBIN, "engine2");
	modules::tier0 = new CModule(ROOTBIN, "tier0");
	modules::server = new CModule(GAMEBIN, "server");
	modules::schemasystem = new CModule(ROOTBIN, "schemasystem");
	modules::vscript = new CModule(ROOTBIN, "vscript");
	modules::client = nullptr;

	if (!CommandLine()->HasParm("-dedicated"))
		modules::client = new CModule(GAMEBIN, "client");

#ifdef _WIN32
	modules::hammer = nullptr;
	if (CommandLine()->HasParm("-tools"))
		modules::hammer = new CModule(ROOTBIN, "tools/hammer");
#endif

	RESOLVE_SIG(g_GameConfig, "NetworkStateChanged", addresses::NetworkStateChanged);
	RESOLVE_SIG(g_GameConfig, "StateChanged", addresses::StateChanged);
	RESOLVE_SIG(g_GameConfig, "UTIL_ClientPrintAll", addresses::UTIL_ClientPrintAll);
	RESOLVE_SIG(g_GameConfig, "ClientPrint", addresses::ClientPrint);
	RESOLVE_SIG(g_GameConfig, "SetGroundEntity", addresses::SetGroundEntity);
	RESOLVE_SIG(g_GameConfig, "CCSPlayerController_SwitchTeam", addresses::CCSPlayerController_SwitchTeam);
}
