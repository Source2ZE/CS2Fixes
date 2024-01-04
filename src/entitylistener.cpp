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

#include "entitylistener.h"
#include "common.h"
#include "cs2fixes.h"

void CEntityListener::OnEntitySpawned(CEntityInstance* pEntity)
{
#ifdef _DEBUG
	const char* pszClassName = pEntity->m_pEntity->m_designerName.String();
	Message("Entity spawned: %s\n", pszClassName);
#endif
}

void CEntityListener::OnEntityCreated(CEntityInstance* pEntity)
{
	CBaseEntity* pBaseEntity = g_pEntitySystem->GetBaseEntity(pEntity->GetEntityIndex());

	// This needs to be in cs2fixes.cpp, due to use of SourceHook macros
	g_CS2Fixes.Setup_Hook_GetHammerUniqueId(pBaseEntity);
}

void CEntityListener::OnEntityDeleted(CEntityInstance* pEntity)
{
}

void CEntityListener::OnEntityParentChanged(CEntityInstance* pEntity, CEntityInstance* pNewParent)
{
}