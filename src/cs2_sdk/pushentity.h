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

#pragma once

#include "mathlib/vector.h"
#include "tier0/utlvector.h"

class CEntityInstance;

// Workaround to access CUtlVector::m_Size, which is protected, without constructing or destructing any elements
template <class T>
class CUtlVectorAccessor : public CUtlVector<T>
{
public:
	void SetCountUnsafe(int nCount) { this->m_Size = nCount; }
};

struct PhysicsPusherInfo_t
{
	CEntityInstance* m_pEntity;
	Vector m_vecStartAbsOrigin;
	bool m_bCollisionCleared;
};
static_assert(sizeof(PhysicsPusherInfo_t) == 0x18);

class CPhysicsPushedEntities
{
public:
	void* m_pVTable;
	CUtlVector<PhysicsPusherInfo_t> m_rgPusher;
};
static_assert(offsetof(CPhysicsPushedEntities, m_rgPusher) == 0x8);
