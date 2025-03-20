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

#include "services.h"
#include "ccsplayerpawn.h"

bool g_bAwsChangingTeam = false;

[[nodiscard]] gear_slot_t CCSPlayer_ItemServices::GetItemGearSlot(const char* item) noexcept
{
	if (const auto pInfo = FindWeaponInfoByClass(item))
		return pInfo->m_eSlot;

	return GEAR_SLOT_INVALID;
}

CBasePlayerWeapon* CCSPlayer_ItemServices::GiveNamedItemAws(const char* item) noexcept
{
	auto pPawn = GetPawn();
	if (!pPawn || pPawn->m_iTeamNum() != CS_TEAM_CT) // only for CT
		return GiveNamedItem(item);

	CCSPlayer_WeaponServices* pWeaponServices = pPawn->m_pWeaponServices();

	if (!pWeaponServices)
		return GiveNamedItem(item);

	const auto pInfo = FindWeaponInfoByClass(item);
	if (!pInfo)
		return GiveNamedItem(item);

	if (pInfo->m_iTeamNum == 0
		|| !(pInfo->m_eSlot == GEAR_SLOT_RIFLE || pInfo->m_eSlot == GEAR_SLOT_PISTOL)
		|| pInfo->m_iTeamNum == pPawn->m_iTeamNum())
		return GiveNamedItem(item);

	const auto team = pPawn->m_iTeamNum();
	g_bAwsChangingTeam = true;
	pPawn->m_iTeamNum(pInfo->m_iTeamNum);
	const auto pWeapon = GiveNamedItem(item);

	// Forcibly equip the weapon, because it spawns dropped in-world due to ZR enforcing mp_weapons_allow_* cvars against T's, which meant other players could pick up the weapon instead
	pWeaponServices->EquipWeapon(pWeapon);

	pPawn->m_iTeamNum(team);
	g_bAwsChangingTeam = false;
	return pWeapon;
}