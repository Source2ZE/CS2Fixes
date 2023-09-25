#include "interfaces.h"

CGameEntitySystem* CGameEntitySystem::GetInstance() {
    if (!interfaces::pGameResourceServiceServer) return nullptr;
    return interfaces::pGameResourceServiceServer->GetGameEntitySystem();
}