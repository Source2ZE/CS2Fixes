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

#include "platform.h"
#include "utils/module.h"

class CMemPatch
{
public:
    CMemPatch(CModule **pModule, const byte *pSignature, const byte *pPatch, const char *pszName, int iRepeat = 1) :
		m_pModule(pModule), m_pSignature(pSignature), m_pPatch(pPatch), m_pszName(pszName), m_iRepeat(iRepeat)
	{
		m_pPatchAddress = nullptr;
        m_pOriginalBytes = nullptr;
		m_iPatchLength = V_strlen((char*)m_pPatch);
	}

	void PerformPatch();
	void UndoPatch();

	void *GetPatchAddress() { return m_pPatchAddress; }

private:
	CModule **m_pModule;
	const byte *m_pSignature;
	const byte *m_pPatch;
	byte *m_pOriginalBytes;
	const char *m_pszName;
	int m_iRepeat;
	int m_iPatchLength;
	void *m_pPatchAddress;
};
