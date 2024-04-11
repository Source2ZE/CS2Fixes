/**
 * =============================================================================
 * MultiAddonManager
 * Copyright (C) 2024 xen
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

#define MULTIADDONMANAGER_INTERFACE "MultiAddonManager002"

class IMultiAddonManager
{
public:
	// These add/remove to the internal list without reloading anything
	// pszWorkshopID is the workshop ID in string form (e.g. "3157463861")
	virtual bool AddAddon(const char *pszWorkshopID) = 0;
	virtual bool RemoveAddon(const char *pszWorkshopID) = 0;
	
	// Returns true if the given addon is mounted in the filesystem
	virtual bool IsAddonMounted(const char *pszWorkshopID) = 0;

	// Start an addon download of the given workshop ID
	// Returns true if the download successfully started or the addon already exists, and false otherwise
	// bImportant: If set, the map will be reloaded once the download finishes 
	// bForce: If set, will start the download even if the addon already exists
	virtual bool DownloadAddon(const char *pszWorkshopID, bool bImportant = false, bool bForce = true) = 0;

	// Refresh addons, applying any changes from add/remove
	// This will trigger a map reload once all addons are updated and mounted
	virtual void RefreshAddons() = 0;

	// Clear the internal list and unmount all addons excluding the current workshop map
	virtual void ClearAddons() = 0;
};
