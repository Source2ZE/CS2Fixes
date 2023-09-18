#pragma once

#include "platform.h"

class CDetour
{
public:
	CDetour(const char *pszModule, void *pfnDetour, const char *pszName, byte *pSignature = nullptr, const char *pszPattern = nullptr) :
		m_pszModule(pszModule), m_pfnDetour(pfnDetour), m_pszName(pszName), m_pSignature(pSignature), m_pszPattern(pszPattern)
	{
		m_pfnFunc = m_pfnOrigFunc = nullptr;
	}

	void CreateDetour();
	void EnableDetour();
	void DisableDetour();

	void *GetOriginalFunction() { return m_pfnOrigFunc; }

private:
	const char *m_pszModule;
	const char *m_pszName;
	byte *m_pSignature;
	const char *m_pszPattern;
	void *m_pfnFunc;
	void *m_pfnOrigFunc;
	void *m_pfnDetour;
};
