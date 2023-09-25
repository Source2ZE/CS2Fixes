#pragma once

#include "cbaseentity.h"
#include "../chandle.h"

class CBasePlayerController : public CBaseEntity {
public:
    SCHEMA_FIELD(m_steamID, "CBasePlayerController", "m_steamID", uint64_t);
    SCHEMA_FIELD(m_hPawn, "CBasePlayerController", "m_hPawn", CHandle);
};