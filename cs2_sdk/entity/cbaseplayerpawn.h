#pragma once

#include "cbaseentity.h"
#include "services.h"

class CBasePlayerPawn : public CBaseEntity
{
public:
	DECLARE_CLASS(CBasePlayerPawn);

	SCHEMA_FIELD(CPlayer_MovementServices*, m_pMovementServices)
	SCHEMA_FIELD(uint8*, m_pWeaponServices)
	SCHEMA_FIELD(uint8**, m_pItemServices)
};