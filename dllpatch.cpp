#include "dllpatch.h"

#include "common.h"
#include "dbg.h"
#include "strtools.h"
#include <Psapi.h>

#include "tier0/memdbgon.h"

void CDLLPatch::PerformPatch()
{
	HMODULE lib = 0;

	char szModule[MAX_PATH];

	V_strcpy(szModule, Plat_GetGameDirectory());
	V_strcat(szModule, m_pszModule, MAX_PATH);

	lib = LoadLibraryA(szModule);

	if (!lib)
		Error("Could not find %s", szModule);

	MODULEINFO moduleInfo;

	GetModuleInformation(GetCurrentProcess(), lib, &moduleInfo, sizeof(moduleInfo));

	for (int i = 0; i < m_iRepeat; i++)
	{
		m_pPatchAddress = (void *)FindPattern(moduleInfo.lpBaseOfDll, m_pSignature, m_pszPattern, moduleInfo.SizeOfImage);

		if (!m_pPatchAddress)
		{
			if (i == 0)
				Panic("Failed to find signature for %s", m_pszName);

			return;
		}

		WriteProcessMemory(GetCurrentProcess(), m_pPatchAddress, m_pPatch, V_strlen((char *)m_pPatch), nullptr);
	}

	ConColorMsg(Color(255, 0, 255, 255), "[CS2Fixes] Successfully patched %s!\n", m_pszName);

#ifdef _DEBUG
	printf("[CS2Fixes] Successfully patched %s!\n", m_pszName);
#endif
}

void *FindPattern(void *BaseAddr, const byte *pData, const char *pPattern, size_t MaxSize)
{
	unsigned char *pMemory;
	uintptr_t PatternLen = strlen(pPattern);
	void *return_addr = nullptr;

	pMemory = (byte *)BaseAddr;

	for (uintptr_t i = 0; i < MaxSize; i++)
	{
		uintptr_t Matches = 0;
		while (*(pMemory + i + Matches) == pData[Matches] || pPattern[Matches] != 'x')
		{
			Matches++;
			if (Matches == PatternLen)
				return_addr = (void *)(pMemory + i);
		}
	}

	return return_addr;
}
