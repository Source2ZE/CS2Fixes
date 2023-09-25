#pragma once

#include "cbaseentity.h"
#include "services.h"

class CBasePlayerPawn : public CBaseEntity
{
public:
	SCHEMA_FIELD(m_pMovementServices, "CBasePlayerPawn", "m_pMovementServices", CPlayer_MovementServices*)
	SCHEMA_FIELD_NEW(m_pWeaponServices, "CBasePlayerPawn", 0, uint8*)
	SCHEMA_FIELD_NEW(m_pItemServices, "CBasePlayerPawn", 0, uint8**)
	//SCHEMA_FIELD(m_pItemServices, "CBasePlayerPawn", "m_pItemServices", uint8**)
};