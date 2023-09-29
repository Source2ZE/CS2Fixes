#ifdef _WIN32

#include "dllpatch.h"
#include "common.h"
#include "tier0/dbg.h"
#include "tier1/strtools.h"
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
		Error("Could not find %s\n", szModule);

	MODULEINFO moduleInfo;

	GetModuleInformation(GetCurrentProcess(), lib, &moduleInfo, sizeof(moduleInfo));

	for (int i = 0; i < m_iRepeat; i++)
	{
		m_pPatchAddress = (void *)FindSignature(moduleInfo.lpBaseOfDll, m_pSignature, moduleInfo.SizeOfImage);

		if (!m_pPatchAddress)
		{
			if (i == 0)
				Panic("Failed to find signature for %s\n", m_pszName);

			return;
		}

		WriteProcessMemory(GetCurrentProcess(), m_pPatchAddress, m_pPatch, V_strlen((char *)m_pPatch), nullptr);
	}

	ConColorMsg(Color(255, 0, 255, 255), "[CS2Fixes] Successfully patched %s!\n", m_pszName);

#ifdef USE_DEBUG_CONSOLE
	printf("[CS2Fixes] Successfully patched %s!\n", m_pszName);
#endif
}

void* FindSignature(void* BaseAddr, const byte* pData, size_t MaxSize)
{
	unsigned char* pMemory;
	void* return_addr = nullptr;

	size_t iSigLength = V_strlen((const char*)pData);

	pMemory = (byte*)BaseAddr;

	for (size_t i = 0; i < MaxSize; i++)
	{
		size_t Matches = 0;
		while (*(pMemory + i + Matches) == pData[Matches] || pData[Matches] == '\x2A')
		{
			Matches++;
			if (Matches == iSigLength)
				return_addr = (void*)(pMemory + i);
		}
	}

	return return_addr;
}

#endif
