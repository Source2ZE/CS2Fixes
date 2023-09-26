#pragma once

#include "MinHook.h"
#include "tier0/dbg.h"
#include <Psapi.h>
#include <array>

template <typename T>
class CDetour
{
public:
	CDetour(const char *pszModule, T *pfnDetour, const char *pszName, byte *pSignature = nullptr, const char *pszPattern = nullptr) :
		m_pszModule(pszModule), m_pfnDetour(pfnDetour), m_pszName(pszName), m_pSignature(pSignature), m_pszPattern(pszPattern)
	{
	}

	void CreateDetour();
	void EnableDetour();
	void DisableDetour();

	// Shorthand for calling original.
	template <typename... Args>
	auto operator()(Args &&...args)
	{
		return std::invoke(m_pfnOrigFunc, std::forward<Args>(args)...);
	}

private:
	const char *m_pszModule;
	const char *m_pszName;
	byte *m_pSignature;
	const char *m_pszPattern;
	void *m_pfnFunc;
	T *m_pfnOrigFunc;
	T *m_pfnDetour;
};

template <typename T>
void CDetour<T>::CreateDetour()
{
	char szModule[MAX_PATH];

	V_strcpy(szModule, Plat_GetGameDirectory());
	V_strcat(szModule, m_pszModule, MAX_PATH);

	HMODULE lib = LoadLibraryA(szModule);

	if (!lib)
		Error("Could not find %s", szModule);

	MODULEINFO moduleInfo;
	GetModuleInformation(GetCurrentProcess(), lib, &moduleInfo, sizeof(moduleInfo));

	if (!m_pSignature)
		m_pfnFunc = GetProcAddress(lib, m_pszName);
	else
		m_pfnFunc = FindSignature(moduleInfo.lpBaseOfDll, m_pSignature, moduleInfo.SizeOfImage);

	if (!m_pfnFunc)
	{
		Panic("[CS2Fixes] Could not find the address for %s to detour", m_pszName);
		return;
	}

	MH_CreateHook(m_pfnFunc, m_pfnDetour, (void **)&m_pfnOrigFunc);
}

template <typename T>
void CDetour<T>::EnableDetour()
{
	MH_EnableHook(m_pfnFunc);
}

template <typename T>
void CDetour<T>::DisableDetour()
{
	MH_DisableHook(m_pfnFunc);
}

#define DECLARE_DETOUR(name, detour, modulepath, signature) \
	CDetour<decltype(detour)> name(modulepath, detour, #name, (byte*)signature)
