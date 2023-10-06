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

#include "interface.h"
#include "cs2_interfaces.h"
#include "strtools.h"
#include "../../common.h"
#include "tier0/dbg.h"
#include "interfaces/interfaces.h"
#include "../../utils/module.h"
#include "../addresses.h"

#include "tier0/memdbgon.h"

void interfaces::Initialize()
{
	pGameResourceServiceServer = (CGameResourceService*)modules::engine->FindInterface(GAMERESOURCESERVICESERVER_INTERFACE_VERSION);
	g_pCVar = (ICvar*)modules::tier0->FindInterface(CVAR_INTERFACE_VERSION);
	g_pSource2GameEntities = (ISource2GameEntities*)modules::server->FindInterface(SOURCE2GAMEENTITIES_INTERFACE_VERSION);
	pSchemaSystem = (CSchemaSystem*)modules::schemasystem->FindInterface(SCHEMASYSTEM_INTERFACE_VERSION);
}
