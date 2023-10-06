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
#include <platform.h>

#include "../schema.h"

class CBaseEntity;

struct CSPerRoundStats_t
{
public:
	DECLARE_SCHEMA_CLASS_INLINE(CSPerRoundStats_t)

	SCHEMA_FIELD(int, m_iKills)
	SCHEMA_FIELD(int, m_iDeaths)
	SCHEMA_FIELD(int, m_iAssists)
	SCHEMA_FIELD(int, m_iDamage)
};

struct CSMatchStats_t : public CSPerRoundStats_t
{
public:
	DECLARE_SCHEMA_CLASS_INLINE(CSMatchStats_t)
};

class CCSPlayerController_ActionTrackingServices
{
public:
	DECLARE_SCHEMA_CLASS(CCSPlayerController_ActionTrackingServices)

	SCHEMA_FIELD(CSMatchStats_t, m_matchStats)
};

class CPlayer_MovementServices
{
public:
	DECLARE_SCHEMA_CLASS(CPlayer_MovementServices);
};

class CCSPlayerController_InGameMoneyServices
{
public:
	DECLARE_SCHEMA_CLASS(CCSPlayerController_InGameMoneyServices);

    SCHEMA_FIELD(int, m_iAccount)
};

class CCSPlayer_ItemServices
{
public:
	DECLARE_SCHEMA_CLASS(CCSPlayer_ItemServices);
	
	virtual ~CCSPlayer_ItemServices() = 0;
private:
	virtual void unk_01() = 0;
	virtual void unk_02() = 0;
	virtual void unk_03() = 0;
	virtual void unk_04() = 0;
	virtual void unk_05() = 0;
	virtual void unk_06() = 0;
	virtual void unk_07() = 0;
	virtual void unk_08() = 0;
	virtual void unk_09() = 0;
	virtual void unk_10() = 0;
	virtual void unk_11() = 0;
	virtual void unk_12() = 0;
	virtual void unk_13() = 0;
	virtual void unk_14() = 0;
	virtual CBaseEntity* _GiveNamedItem(const char* pchName) = 0;
public:
	virtual bool GiveNamedItemBool(const char* pchName) = 0;
	virtual CBaseEntity* GiveNamedItem(const char* pchName) = 0;
};