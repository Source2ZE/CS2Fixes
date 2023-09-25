#include "addresses.h"
#include "utils/module.h"

#define RESOLVE_SIG(module, sig, variable) variable = (decltype(variable))module.FindSignature(sig)

void addresses::Initialize()
{
	CModule server(GAMEBIN "server.dll");

	RESOLVE_SIG(server, (byte*)"\x4C\x8B\xC9\x48\x8B\x09\x48\x85\xC9\x74\x2A\x48\x8B\x41\x10", addresses::NetworkStateChanged);
	RESOLVE_SIG(server, (byte*)"\x48\x89\x5C\x24\x18\x48\x89\x74\x24\x20\x55\x57\x41\x54\x41\x56\x41\x57\x48\x8D\x6C\x24\xD9", addresses::GiveNamedItem);
}