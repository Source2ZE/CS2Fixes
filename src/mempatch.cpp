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

#include "mempatch.h"
#include "common.h"
#include "tier0/dbg.h"
#include "tier1/strtools.h"
#include "utils/module.h"

#include "tier0/memdbgon.h"

bool CMemPatch::PerformPatch(CGameConfig* gameConfig)
{
	// We're patched already
	if (m_pOriginalBytes)
		return true;

	// If we already have an address, no need to look for it again
	if (!m_pPatchAddress)
	{
		m_pPatchAddress = (uintptr_t)gameConfig->ResolveSignature(m_pSignatureName);

		if (!m_pPatchAddress)
			return false;
	}

	const char* patch = gameConfig->GetPatch(m_pszName);
	if (!patch)
	{
		Panic("Failed to find patch for %s\n", m_pszName);
		return false;
	}
	m_pPatch = gameConfig->HexToByte(patch, m_iPatchLength);
	if (!m_pPatch)
		return false;

	if (V_strcmp(m_pOffsetName, ""))
	{
		m_iOffset = gameConfig->GetOffset(m_pOffsetName);
		if (m_iOffset == -1)
		{
			Panic("Failed to find offset %s for patch %s\n", m_pOffsetName, m_pszName);
			return false;
		}

		m_pPatchAddress += m_iOffset;
	}

	m_pOriginalBytes = new byte[m_iPatchLength];
	V_memcpy(m_pOriginalBytes, (void*)m_pPatchAddress, m_iPatchLength);

	Plat_WriteMemory((void*)m_pPatchAddress, (byte*)m_pPatch, m_iPatchLength);

	Message("Patched %s at %p\n", m_pszName, m_pPatchAddress);
	return true;
}

void CMemPatch::UndoPatch()
{
	if (!m_pPatchAddress)
		return;

	Message("Undoing patch %s at %p\n", m_pszName, m_pPatchAddress);

	Plat_WriteMemory((void*)m_pPatchAddress, m_pOriginalBytes, m_iPatchLength);

	delete[] m_pOriginalBytes;
	m_pOriginalBytes = nullptr;
}
