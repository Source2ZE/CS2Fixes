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

#include "entitylistener.h"
#include "common.h"
#include "cs2fixes.h"
#include "gameconfig.h"
#include "cs2_sdk/entity/cbaseentity.h"
#include "plat.h"

extern CGameConfig *g_GameConfig;

void Patch_GetHammerUniqueId(CEntityInstance *pEntity)
{
	static int offset = g_GameConfig->GetOffset("GetHammerUniqueId");
	void **vtable = *(void ***)pEntity;

	// xor al, al -> mov al, 1
	// so it always returns true and allows hammerid to be copied into the schema prop
	Plat_WriteMemory(vtable[offset], (uint8_t*)"\xB0\x01", 2);
}

void CEntityListener::OnEntitySpawned(CEntityInstance* pEntity)
{
#ifdef _DEBUG
	const char* pszClassName = pEntity->m_pEntity->m_designerName.String();
	Message("Entity spawned: %s %s\n", pszClassName, ((CBaseEntity*)pEntity)->m_sUniqueHammerID().Get());
#endif
}

void CEntityListener::OnEntityCreated(CEntityInstance* pEntity)
{
	ExecuteOnce(Patch_GetHammerUniqueId(pEntity));
}

void CEntityListener::OnEntityDeleted(CEntityInstance* pEntity)
{
}

void CEntityListener::OnEntityParentChanged(CEntityInstance* pEntity, CEntityInstance* pNewParent)
{
}