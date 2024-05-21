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

#include "cbaseentity.h"
#include "globaltypes.h"

class CBaseModelEntity : public CBaseEntity
{
public:
	DECLARE_SCHEMA_CLASS(CBaseModelEntity);

	SCHEMA_FIELD(CCollisionProperty , m_Collision)
	SCHEMA_FIELD(CGlowProperty, m_Glow)
	SCHEMA_FIELD(Color, m_clrRender)
	SCHEMA_FIELD(RenderMode_t, m_nRenderMode)
	SCHEMA_FIELD(float, m_flDissolveStartTime)
	
	void SetModel(const char *szModel)
	{
		addresses::CBaseModelEntity_SetModel(this, szModel);
	}
	
	const char* GetModelName()
	{
		return ((CSkeletonInstance*)m_CBodyComponent->m_pSceneNode.Get())->m_modelState().m_ModelName.Get().String();
	}
};