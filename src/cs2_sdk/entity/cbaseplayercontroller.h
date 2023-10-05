#pragma once

#include "ehandle.h"
#include "cbaseentity.h"
#include "ccsplayerpawn.h"

class CBasePlayerController : public Z_CBaseEntity
{
public:
	DECLARE_SCHEMA_CLASS(CBasePlayerController);

	SCHEMA_FIELD(uint64, m_steamID)
	SCHEMA_FIELD(CHandle<CBasePlayerPawn>, m_hPawn)
	SCHEMA_FIELD(char, m_iszPlayerName)

	CBasePlayerPawn *GetPawn() { return m_hPawn().Get(); }
};