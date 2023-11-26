#pragma once

#include "common.h"
#include <tier1/utlvector.h>

#define DISPATCH_COMPONENT_METHOD(method)				\
	FOR_EACH_VEC(CComponent::g_vecComponents, i)		\
	{													\
		CComponent::g_vecComponents[i]->method();		\
	}

class CComponent
{
public:
	CComponent()
	{
		g_vecComponents.AddToTail(this);
	}
	virtual void Load() {};
	virtual void Unload() {};

public:
	static inline CUtlVector<CComponent*> g_vecComponents;
};