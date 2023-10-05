#include "addresses.h"
#include "utils/module.h"
#include <interfaces/cs2_interfaces.h>

#include "tier0/memdbgon.h"


#define RESOLVE_SIG(module, sig, variable) variable = (decltype(variable))module->FindSignature((uint8*)sig)

void addresses::Initialize()
{
	modules::engine = new CModule(ROOTBIN, "engine2");
	modules::tier0 = new CModule(ROOTBIN, "tier0");
	modules::server = new CModule(GAMEBIN, "server");
	modules::schemasystem = new CModule(ROOTBIN, "schemasystem");
	modules::vscript = new CModule(ROOTBIN, "vscript");
	modules::client = nullptr;
	modules::hammer = nullptr;

	if (!CommandLine()->HasParm("-dedicated"))
		modules::client = new CModule(GAMEBIN, "client");

	if (CommandLine()->HasParm("-tools"))
		modules::hammer = new CModule(ROOTBIN, "tools/hammer");

	RESOLVE_SIG(modules::server, sigs::NetworkStateChanged, addresses::NetworkStateChanged);
	RESOLVE_SIG(modules::server, sigs::StateChanged, addresses::StateChanged);
}