#pragma once

#include "tier0/platform.h"

class CDLLPatch
{
public:
	CDLLPatch(const char *pszModule, byte *pSignature, const char *pszPattern, byte *pPatch, const char *pszName, int iRepeat) :
		m_pszModule(pszModule), m_pSignature(pSignature), m_pszPattern(pszPattern), m_pPatch(pPatch), m_pszName(pszName), m_iRepeat(iRepeat)
	{
	}

	void PerformPatch();

	void *GetPatchAddress() { return m_pPatchAddress; }

private:
	const char *m_pszModule;
	byte *m_pSignature;
	const char *m_pszPattern;
	byte *m_pPatch;
	const char *m_pszName;
	int m_iRepeat;
	void *m_pPatchAddress;
};
