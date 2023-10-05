#include "entity.h"
#include "../common.h"
#include "entitysystem.h"

extern CEntitySystem *g_pEntitySystem;

CEntityInstance* UTIL_FindEntityByClassname(CEntityInstance* pStart, const char* name)
{
	CEntityIdentity* pEntity = pStart ? pStart->m_pEntity->m_pNext : g_pEntitySystem->m_EntityList.m_pFirstActiveEntity;

	for (; pEntity; pEntity = pEntity->m_pNext)
	{
		if (!V_strnicmp(pEntity->m_designerName.String(), name, V_strlen(name)))
			return pEntity->m_pInstance;
	};

	return nullptr;
}
