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

#include "entity.h"

#include "../addresses.h"
#include "../common.h"
#include "../gameconfig.h"
#include "../utils/virtual.h"
#include "entitysystem.h"
#include "platform.h"
#include "entity/cgamerules.h"

#include "tier0/memdbgon.h"

extern CGameEntitySystem *g_pEntitySystem;
extern CGameConfig *g_GameConfig;
extern CCSGameRules *g_pGameRules;

class Z_CBaseEntity;

Z_CBaseEntity *UTIL_FindPickerEntity(CBasePlayerController *pPlayer)
{
	static int offset = g_GameConfig->GetOffset("CGameRules_FindPickerEntity");

	if (offset < 0)
	{
		Panic("Missing offset for CGameRules_FindPickerEntity!\n");
		return nullptr;
	}

	return CALL_VIRTUAL(Z_CBaseEntity *, offset, g_pGameRules, pPlayer);
}

Z_CBaseEntity *UTIL_FindEntityByClassname(CEntityInstance *pStartEntity, const char *szName)
{
	return addresses::CGameEntitySystem_FindEntityByClassName(g_pEntitySystem, pStartEntity, szName);
}

Z_CBaseEntity *UTIL_FindEntityByName(CEntityInstance *pStartEntity, const char *szName,
									   CEntityInstance *pSearchingEntity, CEntityInstance *pActivator, CEntityInstance *pCaller, IEntityFindFilter *pFilter)
{
	return addresses::CGameEntitySystem_FindEntityByName(g_pEntitySystem, pStartEntity, szName, pSearchingEntity, pActivator, pCaller, pFilter);
}

Z_CBaseEntity* CreateEntityByName(const char* className)
{
	return addresses::CreateEntityByName(className, -1);
}

void UTIL_AddEntityIOEvent(CEntityInstance *pTarget, const char *pszInput,
						   CEntityInstance *pActivator, CEntityInstance *pCaller, variant_t *value, float flDelay)
{
	addresses::CEntitySystem_AddEntityIOEvent(g_pEntitySystem, pTarget, pszInput, pActivator, pCaller, value, flDelay, 0);
}
