#pragma once

#include "cgameentitysystem.h"
#include "cgameresourceserviceserver.h"
#include "cschemasystem.h"

namespace interfaces
{
	void Initialize();

	inline CGameResourceService *pGameResourceServiceServer = nullptr;
	inline CSchemaSystem *pSchemaSystem = nullptr;
}