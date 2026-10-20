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

// Every point_template ForceSpawn loads a new spawn group and merges it into the template's owner
// spawn group (the map's group by default) when the spawn finishes. The merge moves the child's
// resource manifests into the owner, which keeps them until it is destroyed on map change, so the
// owner gains one manifest per ForceSpawn and every later ForceSpawn gets slower (measured 0.53 ms
// -> 1.47 ms per call after 65k calls, nothing left alive).
//
// With cs2f_spawngroup_release_manifests enabled, the manifests a merge hands to the owner are
// destroyed right away, the same way the spawn group destructor destroys them. ForceSpawn cost then
// stays flat (0.49 ms per call after 141k calls). Resources that are only referenced by those
// manifests may be unloaded, so this is opt-in.

#include "common.h"
#include "cs2fixes.h"
#include "gameconfig.h"
#include "khook.hpp"
#include "tier0/convar.h"
#include "utils/khook_helpers.h"

#include <vector>

#include "tier0/memdbgon.h"

CConVar<bool> g_cvarSpawnGroupReleaseManifests("cs2f_spawngroup_release_manifests", FCVAR_NONE, "Whether to destroy the resource manifests a point_template ForceSpawn hands to the map spawn group, stops ForceSpawn getting slower over the map, experimental", false);

namespace
{
	// CUtlVector layout used by CBaseSpawnGroup for its resource manifest list
	struct ManifestVector
	{
		int m_nSize;
		void** m_pMemory;
		int m_nAllocated;
		int m_nGrowSize;
	};

	int g_iManifestsOffset = -1;
	int g_iResourceServiceOffset = -1;
	int g_iDestroyManifestIndex = -1;
	bool g_bDisabled = false;

	// Manifest count of the owner before each (possibly nested) merge, -1 when not releasing
	thread_local std::vector<int> t_ownerManifestCounts;

	ManifestVector* GetManifests(void* pSpawnGroup)
	{
		return reinterpret_cast<ManifestVector*>(reinterpret_cast<uintptr_t>(pSpawnGroup) + g_iManifestsOffset);
	}

	void* GetResourceService(void* pSpawnGroup)
	{
		return *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(pSpawnGroup) + g_iResourceServiceOffset);
	}

	bool IsValidVector(const ManifestVector* pVec)
	{
		return pVec->m_nSize >= 0 && pVec->m_nSize <= pVec->m_nAllocated && (pVec->m_nSize == 0 || pVec->m_pMemory);
	}

	void Disable(const char* pszReason)
	{
		g_bDisabled = true;
		Warning("[SpawnGroup] %s, manifest release disabled\n", pszReason);
	}

	KHook::Return<void> Hook_MergeIntoOwner(void* pChild, void* pOwner)
	{
		int nCount = -1;

		if (pChild && pOwner && !g_bDisabled && g_cvarSpawnGroupReleaseManifests.Get())
		{
			ManifestVector* pManifests = GetManifests(pOwner);

			if (!IsValidVector(pManifests))
				Disable("Unexpected spawn group layout");
			else if (GetResourceService(pOwner))
				nCount = pManifests->m_nSize;
		}

		t_ownerManifestCounts.push_back(nCount);
		return {KHook::Action::Ignore};
	}

	KHook::Return<void> Hook_MergeIntoOwner_Post(void* pChild, void* pOwner)
	{
		if (t_ownerManifestCounts.empty())
			return {KHook::Action::Ignore};

		int nOldCount = t_ownerManifestCounts.back();
		t_ownerManifestCounts.pop_back();

		if (nOldCount < 0 || g_bDisabled)
			return {KHook::Action::Ignore};

		ManifestVector* pManifests = GetManifests(pOwner);
		void* pService = GetResourceService(pOwner);

		if (!IsValidVector(pManifests) || pManifests->m_nSize < nOldCount || !pService)
		{
			Disable("Owner manifest list changed unexpectedly during merge");
			return {KHook::Action::Ignore};
		}

		using DestroyManifestFn = void (*)(void*, void*);
		auto pfnDestroyManifest = (*reinterpret_cast<DestroyManifestFn**>(pService))[g_iDestroyManifestIndex];
		int nNewCount = pManifests->m_nSize;

		// Shrink first so the owner never points at a destroyed manifest
		pManifests->m_nSize = nOldCount;

		for (int i = nOldCount; i < nNewCount; i++)
		{
			if (pManifests->m_pMemory[i])
				pfnDestroyManifest(pService, pManifests->m_pMemory[i]);

			pManifests->m_pMemory[i] = nullptr;
		}

		return {KHook::Action::Ignore};
	}

	// Unlike CKHookFunction, a missing signature or offset only disables this feature instead of failing the plugin load
	class CMergeIntoOwnerHook : public CKHookBase
	{
	public:
		CMergeIntoOwnerHook() :
			m_hook(Hook_MergeIntoOwner, Hook_MergeIntoOwner_Post)
		{
			GetKHookList().push_back(this);
		}

		virtual void Configure() override
		{
			g_iManifestsOffset = g_GameConfig->GetOffset("CBaseSpawnGroup_ResourceManifests");
			g_iResourceServiceOffset = g_GameConfig->GetOffset("CBaseSpawnGroup_ResourceService");
			g_iDestroyManifestIndex = g_GameConfig->GetOffset("IGameResourceService_DestroyManifest");

			if (g_iManifestsOffset < 0 || g_iResourceServiceOffset < 0 || g_iDestroyManifestIndex < 0)
			{
				Warning("[SpawnGroup] Missing spawn group offsets, cs2f_spawngroup_release_manifests is unavailable\n");
				return;
			}

			auto address = g_GameConfig->ResolveSignature("CBaseSpawnGroup_MergeIntoOwner");

			if (!address)
			{
				Warning("[SpawnGroup] Failed to find CBaseSpawnGroup_MergeIntoOwner, cs2f_spawngroup_release_manifests is unavailable\n");
				return;
			}

			m_hook.Configure(address);

			Message("Hooked CBaseSpawnGroup_MergeIntoOwner at 0x%p\n", address);
		}

	private:
		KHook::Function<void, void*, void*> m_hook;
	};

	CMergeIntoOwnerHook g_hookMergeIntoOwner;
} // namespace
