/**
 * =============================================================================
 * CS2Fixes
 * Copyright (C) 2023-2025 Source2ZE
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
#include "addresses.h"
#include "funchook.h"
#include "gameconfig.h"
#include "module.h"
#include "plat.h"
#include "utlvector.h"
#include <functional>

class CDetourBase
{
public:
	virtual const char* GetName() = 0;
	virtual bool CreateDetour(CGameConfig* gameConfig) = 0;
	virtual void FreeDetour() = 0;
	virtual void EnableDetour() = 0;
	virtual void DisableDetour() = 0;
};

template <typename T>
class CDetour : public CDetourBase
{
public:
	CDetour(T* pfnDetour, const char* pszName);

	~CDetour()
	{
		FreeDetour();
	}

	bool CreateDetour(CGameConfig* gameConfig) override;
	void EnableDetour();
	void DisableDetour();
	void FreeDetour() override;
	const char* GetName() override { return m_pszName; }
	T* GetFunc() { return m_pfnFunc; }

	// Shorthand for calling original.
	template <typename... Args>
	auto operator()(Args&&... args)
	{
		return std::invoke(m_pfnFunc, std::forward<Args>(args)...);
	}

private:
	CModule** m_pModule;
	T* m_pfnDetour;
	const char* m_pszName;
	byte* m_pSignature;
	const char* m_pSymbol;
	T* m_pfnFunc;
	funchook_t* m_hook;
	bool m_bInstalled;
};

extern CUtlVector<CDetourBase*> g_vecDetours;

template <typename T>
CDetour<T>::CDetour(T* pfnDetour, const char* pszName) :
	m_pfnDetour(pfnDetour), m_pszName(pszName)
{
	m_hook = nullptr;
	m_bInstalled = false;
	m_pSignature = nullptr;
	m_pSymbol = nullptr;
	m_pModule = nullptr;

	g_vecDetours.AddToTail(this);
}

template <typename T>
bool CDetour<T>::CreateDetour(CGameConfig* gameConfig)
{
	m_pfnFunc = (T*)gameConfig->ResolveSignature(m_pszName);
	if (!m_pfnFunc)
		return false;

	T* pFunc = m_pfnFunc;

	m_hook = funchook_create();
	funchook_prepare(m_hook, (void**)&m_pfnFunc, (void*)m_pfnDetour);

	Message("Detoured %s at 0x%p\n", m_pszName, pFunc);
	return true;
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
	if (!m_hook)
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
	m_hook = nullptr;

	if (error == 0)
		Message("Removed detour %s\n", m_pszName);
	else
		Warning("funchook_destroy error for %s: %d %s\n", m_pszName, error, funchook_error_message(m_hook));
}

#define DECLARE_DETOUR(name, detour) \
	CDetour<decltype(detour)> name(detour, #name)
