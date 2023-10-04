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
	V_memcpy(m_pOriginalBytes, m_pPatchAddress, m_iPatchLength);

	Plat_WriteMemory(m_pPatchAddress, (byte*)m_pPatch, m_iPatchLength);

	Message("Successfully patched %s at %p\n", m_pszName, m_pPatchAddress);
}

void CMemPatch::UndoPatch()
{
	if (!m_pPatchAddress)
		return;

	Message("Undoing patch %s at %p\n", m_pszName, m_pPatchAddress);

	Plat_WriteMemory(m_pPatchAddress, m_pOriginalBytes, m_iPatchLength);

	delete[] m_pOriginalBytes;
}
