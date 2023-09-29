#pragma once 
#include "subhook.h"
#include "module.h"
#include "tier0/dbg.h"
#include "utlvector.h"
#include "plat.h"

class CDetourBase
{
public:
	virtual const char *GetName() = 0;
	virtual void FreeDetour() = 0;
};

template <typename T>
class CDetour : public CDetourBase
{
public:
	CDetour(CModule *pModule, T *pfnDetour, const char *pszName, byte *pSignature = nullptr) :
		m_pModule(pModule), m_pfnDetour(pfnDetour), m_pszName(pszName), m_pSignature(pSignature)
	{
	}

	void CreateDetour();
	void EnableDetour();
	void DisableDetour();
	void FreeDetour() override;
	const char* GetName() override
	{
		return m_pszName;
	}

	T *GetOriginal();

	// Shorthand for calling original.
	//template <typename... Args>
	//auto operator()(Args &&...args)
	//{
	//	return std::invoke(m_pfnOrigFunc, std::forward<Args>(args)...);
	//}

private:
	CModule *m_pModule;
	T *m_pfnDetour;
	const char* m_pszName;
	byte* m_pSignature;
	void *m_pfnFunc;
	T *m_pfnOrigFunc;
	subhook_t m_hook;
};

static CUtlVector<CDetourBase*> g_vecDetours;

template <typename T>
void CDetour<T>::CreateDetour()
{
	if (!m_pSignature)
		m_pfnFunc = dlsym(m_pModule->m_hModule, m_pszName);
	else
		m_pfnFunc = m_pModule->FindSignature(m_pSignature);

	if (!m_pfnFunc)
	{
		Panic("[CS2Fixes] Could not find the address for %s to detour", m_pszName);
		return;
	}

	m_hook = subhook_new((void*)m_pfnFunc, (void*)m_pfnDetour, (subhook_flags_t)0);

	g_vecDetours.AddToTail(this);
}

template <typename T>
void CDetour<T>::EnableDetour()
{
	subhook_install(m_hook);
}

template <typename T>
void CDetour<T>::DisableDetour()
{
	subhook_remove(m_hook);
}

template <typename T>
void CDetour<T>::FreeDetour()
{
	DisableDetour();
	subhook_free(m_hook);
}

template <typename T>
T* CDetour<T>::GetOriginal()
{
	return m_pfnOrigFunc;
}

#define DECLARE_DETOUR(name, detour, modulepath, signature) \
	CDetour<decltype(detour)> name(modulepath, detour, #name, (byte*)signature)

void FlushAllDetours();