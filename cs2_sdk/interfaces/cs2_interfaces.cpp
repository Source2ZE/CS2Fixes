#include "interface.h"
#include "cs2_interfaces.h"
#include "strtools.h"
#include "../../common.h"
#include "tier0/dbg.h"
#include "interfaces/interfaces.h"
#include "../../utils/module.h"
#include "../addresses.h"

#include "tier0/memdbgon.h"

void interfaces::Initialize()
{
	pGameResourceServiceServer = (CGameResourceService*)modules::engine->FindInterface(GAMERESOURCESERVICESERVER_INTERFACE_VERSION);
	g_pCVar = (ICvar*)modules::tier0->FindInterface(CVAR_INTERFACE_VERSION);
	g_pSource2GameEntities = (ISource2GameEntities*)modules::server->FindInterface(SOURCE2GAMEENTITIES_INTERFACE_VERSION);
	pSchemaSystem = (CSchemaSystem*)modules::schemasystem->FindInterface(SCHEMASYSTEM_INTERFACE_VERSION);
}
