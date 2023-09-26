#include "cs2_interfaces.h"

#include "tier0/memdbgon.h"

CGameEntitySystem *CGameEntitySystem::GetInstance()
{
	if (!interfaces::pGameResourceServiceServer)
		return nullptr;

	return interfaces::pGameResourceServiceServer->GetGameEntitySystem();
}