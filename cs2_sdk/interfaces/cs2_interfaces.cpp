#include "interface.h"
#include "cs2_interfaces.h"
#include "strtools.h"
#include "../../common.h"
#include "tier0/dbg.h"
#include "interfaces/interfaces.h"
#include "../../utils/module.h"

#include "tier0/memdbgon.h"

ICvar *g_pCVar = nullptr;
ISource2GameEntities *g_pSource2GameEntities = nullptr;

void interfaces::Initialize()
{
	CModule engine(ROOTBIN "engine2.dll");
	CModule tier0(ROOTBIN "tier0.dll");
	CModule server(GAMEBIN "server.dll");
	CModule schemasystem(ROOTBIN "schemasystem.dll");

	pGameResourceServiceServer = (CGameResourceService*)engine.FindInterface(GAMERESOURCESERVICESERVER_INTERFACE_VERSION);
	g_pCVar = (ICvar*)tier0.FindInterface(CVAR_INTERFACE_VERSION);
	g_pSource2GameEntities = (ISource2GameEntities*)server.FindInterface(SOURCE2GAMEENTITIES_INTERFACE_VERSION);
	pSchemaSystem = (CSchemaSystem*)schemasystem.FindInterface(SCHEMASYSTEM_INTERFACE_VERSION);
}
