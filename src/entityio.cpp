#include "detours.h"
#include "entityio.h"
#include "cs2fixes.h"

extern CDetour<decltype(Detour_CEntityIOOutput_FireOutputInternal)> CEntityIOOutput_FireOutputInternal;

void FASTCALL Detour_CEntityIOOutput_FireOutputInternal(CEntityIOOutput* const pThis, CEntityInstance* pActivator, CEntityInstance* pCaller, const CVariant* const value, float flDelay)
{
#ifdef DEBUG
	// pCaller can absolutely be null. Needs to be checked
	if(pCaller)
		ConMsg("Output %s fired on %s\n", pThis->m_pDesc->m_pName, pCaller->GetClassname());
	else
		ConMsg("Output %s fired with no caller\n", pThis->m_pDesc->m_pName);
#endif

	CEntityIOOutput_FireOutputInternal(pThis, pActivator, pCaller, value, flDelay);
}