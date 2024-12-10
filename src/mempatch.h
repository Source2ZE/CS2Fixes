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

#include "platform.h"
#include "utils/module.h"
#include "gameconfig.h"

class CMemPatch
{
public:
	CMemPatch(const char *pSignatureName, const char *pszName ,const char *pOffsetName = "") :
		m_pSignatureName(pSignatureName), m_pszName(pszName), m_pOffsetName(pOffsetName)
	{
		m_pModule = nullptr;
		m_pPatchAddress = 0x00;
		m_pOriginalBytes = nullptr;
		m_pSignature = nullptr;
		m_pPatch = nullptr;
		m_iPatchLength = 0;
		m_iOffset = 0;
	}

	bool PerformPatch(CGameConfig *gameConfig);
	void UndoPatch();

	uintptr_t GetPatchAddress() { return m_pPatchAddress; }

private:
	CModule **m_pModule;
	const byte *m_pSignature;
	const byte *m_pPatch;
	byte *m_pOriginalBytes;
	const char *m_pSignatureName;
	const char *m_pszName;
	const char *m_pOffsetName;
	int m_iOffset;
	size_t m_iPatchLength;
	uintptr_t m_pPatchAddress;
};
