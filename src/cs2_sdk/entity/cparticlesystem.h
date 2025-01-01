/**
 * =============================================================================
 * CS2Fixes
 * Copyright (C) 2023-2025 Source2ZE
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

#include "cbasemodelentity.h"

class CParticleSystem : public CBaseModelEntity
{
public:
	DECLARE_SCHEMA_CLASS(CParticleSystem);

	SCHEMA_FIELD(bool, m_bActive)
	SCHEMA_FIELD(bool, m_bStartActive)
	SCHEMA_FIELD(bool, m_bFrozen)
	SCHEMA_FIELD(CUtlSymbolLarge, m_iszEffectName)
	SCHEMA_FIELD(int, m_nTintCP)
	SCHEMA_FIELD_POINTER(Color, m_clrTint)
	SCHEMA_FIELD_POINTER(CHandle<CBaseEntity>, m_hControlPointEnts) // m_hControlPointEnts[64]
};

class CEnvParticleGlow : public CParticleSystem
{
public:
	DECLARE_SCHEMA_CLASS(CEnvParticleGlow);

	SCHEMA_FIELD(float, m_flAlphaScale)
	SCHEMA_FIELD(float, m_flRadiusScale)
	SCHEMA_FIELD(float, m_flSelfIllumScale)
	SCHEMA_FIELD_POINTER(Color, m_ColorTint)
};