#include "entity.h"
#include "../common.h"

CEntityInstance* UTIL_FindEntityByClassname(CEntityInstance* pStart, const char* name)
{
	CEntityIdentity* pEntity = pStart ? pStart->m_pEntity->m_pNext : g_pEntitySystem->m_EntityList.m_pFirstActiveEntity;

	for (; pEntity; pEntity = pEntity->m_pNext)
	{
		if (!strcmp(pEntity->m_designerName.String(), name))
			return pEntity->m_pInstance;
	};

	return nullptr;
}
