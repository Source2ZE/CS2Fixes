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
#include "../common.h"
#include "entitysystem.h"

extern CEntitySystem *g_pEntitySystem;

CEntityInstance* UTIL_FindEntityByClassname(CEntityInstance* pStart, const char* name)
{
	CEntityIdentity* pEntity = pStart ? pStart->m_pEntity->m_pNext : g_pEntitySystem->m_EntityList.m_pFirstActiveEntity;

	for (; pEntity; pEntity = pEntity->m_pNext)
	{
		if (!V_strnicmp(pEntity->m_designerName.String(), name, V_strlen(name)))
			return pEntity->m_pInstance;
	};

	return nullptr;
}
