#pragma once

#include "cbasetrigger.h"
#include"../schema.h"

#define SF_TRIG_PUSH_ONCE 0x80

class CTriggerPush : public CBaseTrigger
{
public:
	DECLARE_SCHEMA_CLASS(CTriggerPush);

	SCHEMA_FIELD(Vector, m_vecPushDirEntitySpace)
	SCHEMA_FIELD(bool, m_bTriggerOnStartTouch)
};
