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

extern CGlobalVars* gpGlobals;

enum gear_slot_t : uint32_t
{
	GEAR_SLOT_INVALID = 0xffffffff,
	GEAR_SLOT_RIFLE = 0x0,
	GEAR_SLOT_PISTOL = 0x1,
	GEAR_SLOT_KNIFE = 0x2,
	GEAR_SLOT_GRENADES = 0x3,
	GEAR_SLOT_C4 = 0x4,
	GEAR_SLOT_RESERVED_SLOT6 = 0x5,
	GEAR_SLOT_RESERVED_SLOT7 = 0x6,
	GEAR_SLOT_RESERVED_SLOT8 = 0x7,
	GEAR_SLOT_RESERVED_SLOT9 = 0x8,
	GEAR_SLOT_RESERVED_SLOT10 = 0x9,
	GEAR_SLOT_RESERVED_SLOT11 = 0xa,
	GEAR_SLOT_BOOSTS = 0xb,
	GEAR_SLOT_UTILITY = 0xc,
	GEAR_SLOT_COUNT = 0xd,
	GEAR_SLOT_FIRST = 0x0,
	GEAR_SLOT_LAST = 0xc,
};

class CEconItemView
{
public:
	DECLARE_SCHEMA_CLASS_INLINE(CEconItemView);

	SCHEMA_FIELD(uint16_t, m_iItemDefinitionIndex)
	SCHEMA_FIELD(bool, m_bInitialized)
};

class CAttributeContainer
{
public:
	DECLARE_SCHEMA_CLASS_INLINE(CAttributeContainer);

	SCHEMA_FIELD(CEconItemView, m_Item)
};

class CEconEntity : public CBaseEntity
{
public:
	DECLARE_SCHEMA_CLASS(CEconEntity)

	SCHEMA_FIELD(CAttributeContainer, m_AttributeManager)
};

class CBasePlayerWeaponVData : public CEntitySubclassVDataBase
{
public:
	DECLARE_SCHEMA_CLASS(CBasePlayerWeaponVData)
	SCHEMA_FIELD(int, m_iMaxClip1)
};

class CCSWeaponBaseVData : public CBasePlayerWeaponVData
{
public:
	DECLARE_SCHEMA_CLASS(CCSWeaponBaseVData)

	SCHEMA_FIELD(gear_slot_t, m_GearSlot)
	SCHEMA_FIELD(int, m_nPrice)
	SCHEMA_FIELD(int, m_nPrimaryReserveAmmoMax);
};

class CBasePlayerWeapon : public CEconEntity
{
public:
	DECLARE_SCHEMA_CLASS(CBasePlayerWeapon)
	SCHEMA_FIELD(int, m_nNextPrimaryAttackTick)
	SCHEMA_FIELD(int, m_nNextSecondaryAttackTick)

	CCSWeaponBaseVData* GetWeaponVData() { return (CCSWeaponBaseVData*)GetVData(); }

	void Disarm()
	{
		m_nNextPrimaryAttackTick(MAX(m_nNextPrimaryAttackTick(), gpGlobals->tickcount + 24));
		m_nNextSecondaryAttackTick(MAX(m_nNextSecondaryAttackTick(), gpGlobals->tickcount + 24));
	}
};

class CCSWeaponBase : public CBasePlayerWeapon
{
public:
	DECLARE_SCHEMA_CLASS(CCSWeaponBase)
};
