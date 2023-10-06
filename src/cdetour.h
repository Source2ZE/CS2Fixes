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
#include <functional>
#include "funchook.h"
#include "module.h"
#include "utlvector.h"
#include "plat.h"

class CDetourBase
{
public:
	virtual const char* GetName() = 0;
	virtual void FreeDetour() = 0;
};

template <typename T>
class CDetour : public CDetourBase
{
public:
	CDetour(CModule** pModule, T* pfnDetour, const char* pszName, byte* pSignature = nullptr) :
		m_pModule(pModule), m_pfnDetour(pfnDetour), m_pszName(pszName), m_pSignature(pSignature)
	{
		m_hook = nullptr;
		m_bInstalled = false;
	}

	~CDetour()
	{
		FreeDetour();
	}

	void CreateDetour();
	void EnableDetour();
	void DisableDetour();
	void FreeDetour() override;
	const char* GetName() override { return m_pszName; }
	T *GetFunc() { return m_pfnFunc; }

	// Shorthand for calling original.
	template <typename... Args>
	auto operator()(Args &&...args)
	{
		return std::invoke(m_pfnFunc, std::forward<Args>(args)...);
	}

private:
	CModule** m_pModule;
	T* m_pfnDetour;
	const char* m_pszName;
	byte* m_pSignature;
	T* m_pfnFunc;
	funchook_t* m_hook;
	bool m_bInstalled;
};

extern CUtlVector<CDetourBase*> g_vecDetours;

template <typename T>
void CDetour<T>::CreateDetour()
{
	if (!m_pModule || !(*m_pModule))
	{
		Panic("Invalid Module\n", m_pszName);
		return;
	}

	if (!m_pSignature)
		m_pfnFunc = (T*)dlsym((*m_pModule)->m_hModule, m_pszName);
	else
		m_pfnFunc = (T*)(*m_pModule)->FindSignature(m_pSignature);

	if (!m_pfnFunc)
	{
		Panic("Could not find the address for %s to detour\n", m_pszName);
		return;
	}

	m_hook = funchook_create();
	funchook_prepare(m_hook, (void**)&m_pfnFunc, (void*)m_pfnDetour);

	g_vecDetours.AddToTail(this);

	Message("Successfully initialized detour for %s!\n", m_pszName);
}

template <typename T>
void CDetour<T>::EnableDetour()
{
	if (!m_hook)
		return;

	int error = funchook_install(m_hook, 0);

	if (!error)
		m_bInstalled = true;
	else
		Warning("funchook_install error for %s: %d %s\n", m_pszName, error, funchook_error_message(m_hook));
}

template <typename T>
void CDetour<T>::DisableDetour()
{
	if (!m_hook )
		return;

	int error = funchook_uninstall(m_hook, 0);
	
	if (!error)
		m_bInstalled = false;
	else
		Warning("funchook_uninstall error for %s: %d %s\n", m_pszName, error, funchook_error_message(m_hook));
}

template <typename T>
void CDetour<T>::FreeDetour()
{
	if (!m_hook)
		return;

	if (m_bInstalled)
		DisableDetour();

	int error = funchook_destroy(m_hook);

	if (error != 0)
		Warning("funchook_destroy error for %s: %d %s\n", m_pszName, error, funchook_error_message(m_hook));
}

#define DECLARE_DETOUR(name, detour, modulepath) \
	CDetour<decltype(detour)> name(modulepath, detour, #name, (byte*)sigs::name)

void FlushAllDetours();
