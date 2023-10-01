#include "mempatch.h"
#include "common.h"
#include "utils/module.h"
#include "tier0/dbg.h"
#include "tier1/strtools.h"

#include "tier0/memdbgon.h"

void CMemPatch::PerformPatch()
{
	if (!m_pModule)
		return;

	m_pPatchAddress = (*m_pModule)->FindSignature(m_pSignature);

	if (!m_pPatchAddress)
	{
		Panic("Failed to find signature for %s\n", m_pszName);
		return;
	}

	m_pOriginalBytes = new byte[m_iPatchLength];
	V_strncpy((char*)m_pOriginalBytes, (char*)m_pPatchAddress, m_iPatchLength);

	Plat_WriteMemory(m_pPatchAddress, (byte*)m_pPatch, m_iPatchLength);

	Message("[CS2Fixes] Successfully patched %s!\n", m_pszName);
}

void CMemPatch::UndoPatch()
{
	if (!m_pPatchAddress)
		return;

	Plat_WriteMemory(m_pPatchAddress, m_pOriginalBytes, m_iPatchLength);
}
