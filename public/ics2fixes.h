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

#include <cstdint>

#define CS2FIXES_INTERFACE "CS2Fixes001"

class ICS2Fixes
{
public:
	// Returns a bit flag of admin permissions. 0 if admin system is not initialized or user has no permissions.
	// Valid flags can be found in cs2fixes/src/adminsystem.h
	// What permission each flag correlates to is described in cs2fixes/configs/admins.jsonc.example
	virtual std::uint64_t GetAdminFlags(std::uint64_t iSteam64ID) const = 0;

	// Sets a player's admin permissions bit flag. This will be overwritten if the plugin is reloaded
	// or an admin uses c_reload_admins as it does not alter the config file.
	// Returns false if unable to modify the admin (internal admin system is not set up yet)
	virtual bool SetAdminFlags(std::uint64_t iSteam64ID, std::uint64_t iFlags) = 0;

	// Returns an integer for admin immunity level. 0 if admin system is not initialized or if user has no immunity.
	// For behavior related to immunity, fetch the value for the "cs2f_admin_immunity" ConVar.
	// Immunity targetting should work in the following ways for each value of cs2f_admin_immunity:
	//	0 - Commands using immunity targetting can only target players with immunities LOWER than the user's
	//	1 - Commands using immunity targetting can only target players with immunities EQUAL TO OR LOWER than the user's
	//	2 - Commands ignore immunity levels
	virtual int GetAdminImmunity(std::uint64_t iSteam64ID) const = 0;

	// Sets a player's immunity level. This will be overwritten if the plugin is reloaded
	// or an admin uses c_reload_admins as it does not alter the config file.
	// iImmunity's max value is INT_MAX and will be defaulted to INT_MAX if higher
	// Returns false if unable to modify the admin (internal admin system is not set up yet)
	virtual bool SetAdminImmunity(std::uint64_t iSteam64ID, std::uint32_t iImmunity) = 0;
};