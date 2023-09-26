#include "chandle.h"
#include "interfaces/cs2_interfaces.h"

#include "tier0/memdbgon.h"

CBaseEntity *CHandle::GetBaseEntity() const
{
	CGameEntitySystem *pEntitySystem = CGameEntitySystem::GetInstance();
	if (!pEntitySystem)
		return nullptr;

	return pEntitySystem->GetBaseEntity(GetEntryIndex());
}