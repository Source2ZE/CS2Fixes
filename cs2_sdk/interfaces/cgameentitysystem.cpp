#include "cs2_interfaces.h"
#include "tier0/memdbgon.h"


/*
CGameEntitySystem *CGameEntitySystem::GetInstance()
{
	if (!interfaces::pGameResourceServiceServer)
		return nullptr;

	return interfaces::pGameResourceServiceServer->GetGameEntitySystem();
}*/

/*
CBaseEntity* CEntitySystem::GetBaseEntity(CEntityIndex index)
{
	if (index.Get() <= -1 || index.Get() >= MAX_TOTAL_ENTITIES)
		return nullptr;

	int listToUse = (index.Get() / MAX_ENTITIES_IN_LIST);
	if (!m_pEntityList[listToUse])
		return nullptr;

	return m_pEntityList[listToUse]->m_pIdentities[index.Get() % MAX_ENTITIES_IN_LIST].entity;
}*/