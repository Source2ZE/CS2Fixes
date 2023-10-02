#pragma once
#include "cbaseentity.h"

class CGlowProperty
{
public:
	DECLARE_SCHEMA_CLASS_INLINE(CGlowProperty)

	SCHEMA_FIELD(Vector, m_fGlowColor)
	SCHEMA_FIELD(int, m_iGlowType)
	SCHEMA_FIELD(int, m_nGlowRange)
	SCHEMA_FIELD(Color, m_glowColorOverride)
	SCHEMA_FIELD(bool, m_bFlashing)
	SCHEMA_FIELD(bool, m_bGlowing)
};