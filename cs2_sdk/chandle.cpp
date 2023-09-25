#include "chandle.h"

#include "interfaces/interfaces.h"

CBaseEntity* CHandle::GetBaseEntity() const {
    CGameEntitySystem* pEntitySystem = CGameEntitySystem::GetInstance();
    if (!pEntitySystem) return nullptr;

    ConMsg("HANDLE INDEX: %d\n", GetEntryIndex());
    return pEntitySystem->GetBaseEntity(GetEntryIndex());
}