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

#include "cbaseentity.h"

extern CGlobalVars* GetGlobals();

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
		if (!GetGlobals())
			return;

		m_nNextPrimaryAttackTick(MAX(m_nNextPrimaryAttackTick(), GetGlobals()->tickcount + 24));
		m_nNextSecondaryAttackTick(MAX(m_nNextSecondaryAttackTick(), GetGlobals()->tickcount + 24));
	}

	const char* GetWeaponClassname() noexcept
	{
		const char* pszClassname = GetClassname();
		if (V_StringHasPrefixCaseSensitive(pszClassname, "item_"))
			return pszClassname;

		switch (m_AttributeManager().m_Item().m_iItemDefinitionIndex)
		{
			case 23:
				return "weapon_mp5sd";
			case 41:
				return "weapon_knifegg";
			case 42:
				return "weapon_knife";
			case 59:
				return "weapon_knife_t";
			case 60:
				return "weapon_m4a1_silencer";
			case 61:
				return "weapon_usp_silencer";
			case 63:
				return "weapon_cz75a";
			case 64:
				return "weapon_revolver";
			default:
				return pszClassname;
		}
	}
};

class CCSWeaponBase : public CBasePlayerWeapon
{
public:
	DECLARE_SCHEMA_CLASS(CCSWeaponBase)
};
