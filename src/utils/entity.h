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

#pragma once

#include "platform.h"
#include "datamap.h"
#include "string_t.h"

class CEntityInstance;
class Z_CBaseEntity;
class CBasePlayerController;
class IEntityFindFilter;

// Quick and dirty definition to avoid having to include variant.h for now
struct variant_string_t
{
	variant_string_t(const char *pszValue)
	{
		m_value = MAKE_STRING(pszValue);
	}

	string_t m_value;
	uint16 m_type = FIELD_STRING;
	uint16 m_flags = 0;
};

Z_CBaseEntity *UTIL_FindPickerEntity(CBasePlayerController *pPlayer);
Z_CBaseEntity *UTIL_FindEntityByClassname(CEntityInstance *pStart, const char *name);
Z_CBaseEntity *UTIL_FindEntityByName(CEntityInstance *pStartEntity, const char *szName,
									CEntityInstance *pSearchingEntity = nullptr, CEntityInstance *pActivator = nullptr,
									CEntityInstance *pCaller = nullptr, IEntityFindFilter *pFilter = nullptr);

void UTIL_AddEntityIOEvent(CEntityInstance *pTarget, const char *pszInput,
							CEntityInstance *pActivator = nullptr, CEntityInstance *pCaller = nullptr,
							variant_string_t *value = nullptr, float flDelay = 0.0f);
