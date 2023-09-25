#pragma once

#include "cbaseplayercontroller.h"
#include "services.h"

class CCSPlayerController : public CBasePlayerController
{
public:
	SCHEMA_FIELD(m_pInGameMoneyServices, "CCSPlayerController", "m_pInGameMoneyServices", CCSPlayerController_InGameMoneyServices*)
};