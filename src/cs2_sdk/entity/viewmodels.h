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

class CBaseAnimGraph : public CBaseModelEntity
{
public:
	DECLARE_SCHEMA_CLASS(CBaseAnimGraph)
};

class CBaseViewModel : public CBaseAnimGraph
{
public:
	DECLARE_SCHEMA_CLASS(CBaseViewModel)
	SCHEMA_FIELD(uint32_t, m_nViewModelIndex)
};

class CPredictedViewModel : public CBaseViewModel
{
public:
	DECLARE_SCHEMA_CLASS(CPredictedViewModel)
};

class CCSGOViewModel : public CPredictedViewModel
{
public:
	DECLARE_SCHEMA_CLASS(CCSGOViewModel)
	SCHEMA_FIELD(bool, m_bShouldIgnoreOffsetAndAccuracy)
};
