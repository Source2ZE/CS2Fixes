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
#include "../common.h"
#include "dbg.h"
#include "interface.h"
#include "plat.h"
#include "strtools.h"

#include <string>
#include <vector>

#ifdef _WIN32
	#include <Psapi.h>
#endif

enum SigError
{
	SIG_OK,
	SIG_NOT_FOUND,
	SIG_FOUND_MULTIPLE,
};

// equivalent to FindSignature, but allows for multiple signatures to be found and iterated over
class SignatureIterator
{
public:
	SignatureIterator(void* pBase, size_t iSize, const byte* pSignature, size_t iSigLength) :
		m_pBase((byte*)pBase), m_iSize(iSize), m_pSignature(pSignature), m_iSigLength(iSigLength)
	{
		m_pCurrent = m_pBase;
	}

	void* FindNext(bool allowWildcard)
	{
		for (size_t i = 0; i < m_iSize; i++)
		{
			size_t Matches = 0;
			while (*(m_pCurrent + i + Matches) == m_pSignature[Matches] || (allowWildcard && m_pSignature[Matches] == '\x2A'))
			{
				Matches++;
				if (Matches == m_iSigLength)
				{
					m_pCurrent += i + 1;
					return m_pCurrent - 1;
				}
			}
		}

		return nullptr;
	}

private:
	byte* m_pBase;
	size_t m_iSize;
	const byte* m_pSignature;
	size_t m_iSigLength;
	byte* m_pCurrent;
};

class CModule
{
public:
	CModule(const char* path, const char* module) :
		m_pszModule(module), m_pszPath(path)
	{
		char szModule[MAX_PATH];

		V_snprintf(szModule, MAX_PATH, "%s%s%s%s%s", Plat_GetGameDirectory(), path, MODULE_PREFIX, m_pszModule, MODULE_EXT);

		m_hModule = dlmount(szModule);

		if (!m_hModule)
			Error("Could not find %s\n", szModule);

#ifdef _WIN32
		MODULEINFO m_hModuleInfo;
		GetModuleInformation(GetCurrentProcess(), m_hModule, &m_hModuleInfo, sizeof(m_hModuleInfo));

		m_base = (void*)m_hModuleInfo.lpBaseOfDll;
		m_size = m_hModuleInfo.SizeOfImage;
		InitializeSections();
#else
		if (int e = GetModuleInformation(m_hModule, &m_base, &m_size, m_sections))
			Error("Failed to get module info for %s, error %d\n", szModule, e);
#endif

		for (auto& section : m_sections)
			Message("Section %s base: 0x%p | size: %d\n", section.m_szName.c_str(), section.m_pBase, section.m_iSize);

		Message("Initialized module %s base: 0x%p | size: %d\n", m_pszModule, m_base, m_size);
	}

	void* FindSignature(const byte* pData, size_t iSigLength, int& error)
	{
		unsigned char* pMemory;
		void* return_addr = nullptr;
		error = 0;

		pMemory = (byte*)m_base;

		for (size_t i = 0; i < m_size; i++)
		{
			size_t Matches = 0;
			while (*(pMemory + i + Matches) == pData[Matches] || pData[Matches] == '\x2A')
			{
				Matches++;
				if (Matches == iSigLength)
				{
					if (return_addr)
					{
						error = SIG_FOUND_MULTIPLE;
						return return_addr;
					}

					return_addr = (void*)(pMemory + i);
					break;
				}
			}
		}

		if (!return_addr)
			error = SIG_NOT_FOUND;

		return return_addr;
	}

	void* FindInterface(const char* name)
	{
		CreateInterfaceFn fn = (CreateInterfaceFn)dlsym(m_hModule, "CreateInterface");

		if (!fn)
			Error("Could not find CreateInterface in %s\n", m_pszModule);

		void* pInterface = fn(name, nullptr);

		if (!pInterface)
			Error("Could not find %s in %s\n", name, m_pszModule);

		Message("Found interface %s in %s\n", name, m_pszModule);

		return pInterface;
	}

	Section* GetSection(const std::string_view name)
	{
		for (auto& section : m_sections)
			if (section.m_szName == name)
				return &section;

		return nullptr;
	}
#ifdef _WIN32
	void InitializeSections();
#endif
	void* FindVirtualTable(const std::string& name);

public:
	const char* m_pszModule;
	const char* m_pszPath;
	HINSTANCE m_hModule;
	void* m_base;
	size_t m_size;
	std::vector<Section> m_sections;
};