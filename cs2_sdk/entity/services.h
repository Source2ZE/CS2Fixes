#pragma once
#include <platform.h>

#include "../schema.h"

class CPlayer_MovementServices
{
public:
	DECLARE_CLASS(CPlayer_MovementServices);
};

class CCSPlayerController_InGameMoneyServices
{
public:
	DECLARE_CLASS(CCSPlayerController_InGameMoneyServices);

    SCHEMA_FIELD(int, m_iAccount)
};