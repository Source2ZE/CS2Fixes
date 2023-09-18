#include "detour.h"

#include "MinHook.h"
#include "common.h"
#include "platform.h"
#include "strtools.h"
#include <Psapi.h>

#include "tier0/memdbgon.h"

void CDetour::CreateDetour()
{
	char szModule[MAX_PATH];

	V_strcpy(szModule, Plat_GetGameDirectory());
	V_strcat(szModule, m_pszModule, MAX_PATH);

	HMODULE lib = LoadLibraryA(szModule);

	if (!lib)
		Plat_FatalErrorFunc("Could not find %s", szModule);

	MODULEINFO moduleInfo;
	GetModuleInformation(GetCurrentProcess(), lib, &moduleInfo, sizeof(moduleInfo));

	if (!m_pSignature)
		m_pfnFunc = GetProcAddress(lib, m_pszName);
	else
		m_pfnFunc = FindPattern(moduleInfo.lpBaseOfDll, m_pSignature, m_pszPattern, moduleInfo.SizeOfImage);

	if (!m_pfnFunc)
	{
		Panic("[CS2Fixes] Could not find the address for %s to detour", m_pszName);
		return;
	}

	MH_CreateHook(m_pfnFunc, m_pfnDetour, &m_pfnOrigFunc);
}

void CDetour::EnableDetour()
{
	MH_EnableHook(m_pfnFunc);
}

void CDetour::DisableDetour()
{
	MH_DisableHook(m_pfnFunc);
}