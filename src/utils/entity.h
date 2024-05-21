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

#pragma once

#include "platform.h"
#include "datamap.h"
#include "string_t.h"
#include "variant.h"

class CEntityInstance;
class CBaseEntity;
class CBasePlayerController;
class IEntityFindFilter;

CBaseEntity *UTIL_FindPickerEntity(CBasePlayerController *pPlayer);
CBaseEntity *UTIL_FindEntityByClassname(CEntityInstance *pStart, const char *name);
CBaseEntity *UTIL_FindEntityByName(CEntityInstance *pStartEntity, const char *szName,
									CEntityInstance *pSearchingEntity = nullptr, CEntityInstance *pActivator = nullptr,
									CEntityInstance *pCaller = nullptr, IEntityFindFilter *pFilter = nullptr);
CBaseEntity* CreateEntityByName(const char* className);

// Add an entity IO event to the event queue, just like a map would
// The queue is processed after all entities are simulated every frame
void UTIL_AddEntityIOEvent(CEntityInstance *pTarget, const char *pszInput,
							CEntityInstance *pActivator = nullptr, CEntityInstance *pCaller = nullptr,
						   variant_t value = variant_t(""), float flDelay = 0.0f);
