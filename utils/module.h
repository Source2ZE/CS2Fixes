#pragma once
#include <strtools.h>
#include <interface.h>
#include <dbg.h>
#include <Psapi.h>
#include "../common.h"


class CModule {
public:
	CModule(const char* module) : m_pModule(module)
	{
		char szModule[MAX_PATH];

		V_strcpy(szModule, Plat_GetGameDirectory());
		V_strcat(szModule, m_pModule, MAX_PATH);

		m_hModule = LoadLibraryA(szModule);

		if (!m_hModule)
			Error("Could not find %s\n", szModule);

		GetModuleInformation(GetCurrentProcess(), m_hModule, &m_hModuleInfo, sizeof(m_hModuleInfo));
	}

	void* FindSignature(const byte *pData)
	{
		unsigned char* pMemory;
		void* return_addr = nullptr;

		size_t iSigLength = V_strlen((const char*)pData);

		pMemory = (byte*)m_hModuleInfo.lpBaseOfDll;

		for (size_t i = 0; i < m_hModuleInfo.SizeOfImage; i++)
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

	void* FindInterface(const char* name)
	{
		CreateInterfaceFn fn = (CreateInterfaceFn)GetProcAddress(m_hModule, "CreateInterface");

		if (!fn)
			Error("Could not find CreateInterface in %s\n", m_pModule);

		void* pInterface = fn(name, nullptr);

		if (!pInterface)
			Error("Could not find %s in %s\n", name, m_pModule);

		Message("[CS2Fixes] Found %s in %s!\n", name, m_pModule);

		return pInterface;
	}

	const char* m_pModule;
	HMODULE m_hModule;
	MODULEINFO m_hModuleInfo;
};