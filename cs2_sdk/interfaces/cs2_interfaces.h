#pragma once

#include "cgameresourceserviceserver.h"
#include "cschemasystem.h"

namespace interfaces
{
	void Initialize();

	inline CGameResourceService *pGameResourceServiceServer = nullptr;
	inline CSchemaSystem *pSchemaSystem = nullptr;
}