#pragma once

#include "cbaseplayercontroller.h"
#include "services.h"

class CCSPlayerController : public CBasePlayerController
{
public:
	DECLARE_CLASS(CCSPlayerController);

	SCHEMA_FIELD(CCSPlayerController_InGameMoneyServices*, m_pInGameMoneyServices)
};