#pragma once

#include "cbaseentity.h"
#include "services.h"

class CBasePlayerPawn : public Z_CBaseEntity
{
public:
	DECLARE_SCHEMA_CLASS(CBasePlayerPawn);

	SCHEMA_FIELD(CPlayer_MovementServices*, m_pMovementServices)
	SCHEMA_FIELD(uint8*, m_pWeaponServices)
	SCHEMA_FIELD(CCSPlayer_ItemServices*, m_pItemServices)
};