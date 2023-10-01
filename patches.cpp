#include "detour.h"
#include "common.h"
#include "mempatch.h"
#include "icvar.h"
#include "irecipientfilter.h"
#include "interfaces/cs2_interfaces.h"
#include "entity/ccsplayercontroller.h"
#include "entity/ccsplayerpawn.h"
#include "entity/cbasemodelentity.h"
#include "addresses.h"
#include "tier0/memdbgon.h"

extern CEntitySystem *g_pEntitySystem;

CMemPatch g_CommonPatches[] =
{
	CMemPatch(&modules::server, sigs::MovementUnlock, sigs::Patch_MovementUnlock, "ServerMovementUnlock", 1),
	CMemPatch(&modules::vscript, sigs::VScriptEnable, sigs::Patch_VScriptEnable, "VScriptEnable", 1),
};

CMemPatch g_ClientPatches[] =
{
	CMemPatch(&modules::client, sigs::MovementUnlock, sigs::Patch_MovementUnlock, "ClientMovementUnlock", 1),
};

#ifdef _WIN32
CMemPatch g_ToolsPatches[] =
{
	// Remove some -nocustomermachine checks without needing -nocustomermachine itself as it can break stuff, mainly to enable device selection in compiles
	CMemPatch(
		&modules::hammer,
		(byte*)"\xFF\x15\x2A\x2A\x2A\x2A\x84\xC0\x0F\x85\x2A\x2A\x2A\x2A\xB9",
		(byte*)"\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90",
		"HammerNoCustomerMachine",
		4
	),
};
#endif

void Detour_Log()
{
	return;
}

bool FASTCALL Detour_IsChannelEnabled(LoggingChannelID_t channelID, LoggingSeverity_t severity)
{
	return false;
}

CDetour<decltype(Detour_Log)> g_LoggingDetours[] =
{
	CDetour<decltype(Detour_Log)>(&modules::tier0, Detour_Log, "Msg" ),
	//CDetour<decltype(Detour_Log)>( modules::tier0, Detour_Log, "?ConMsg@@YAXPEBDZZ" ),
	//CDetour<decltype(Detour_Log)>( modules::tier0, Detour_Log, "?ConColorMsg@@YAXAEBVColor@@PEBDZZ" ),
	CDetour<decltype(Detour_Log)>( &modules::tier0, Detour_Log, "ConDMsg" ),
	CDetour<decltype(Detour_Log)>( &modules::tier0, Detour_Log, "DevMsg" ),
	CDetour<decltype(Detour_Log)>( &modules::tier0, Detour_Log, "Warning" ),
	CDetour<decltype(Detour_Log)>( &modules::tier0, Detour_Log, "DevWarning" ),
	//CDetour<decltype(Detour_Log)>( modules::tier0, Detour_Log, "?DevWarning@@YAXPEBDZZ" ),
	CDetour<decltype(Detour_Log)>( &modules::tier0, Detour_Log, "LoggingSystem_Log" ),
	CDetour<decltype(Detour_Log)>( &modules::tier0, Detour_Log, "LoggingSystem_LogDirect" ),
	CDetour<decltype(Detour_Log)>( &modules::tier0, Detour_Log, "LoggingSystem_LogAssert" ),
	//CDetour<decltype(Detour_Log)>( modules::tier0, Detour_IsChannelEnabled, "LoggingSystem_IsChannelEnabled" ),
};

void InitLoggingDetours()
{
	for (int i = 0; i < sizeof(g_LoggingDetours) / sizeof(*g_LoggingDetours); i++)
		g_LoggingDetours[i].CreateDetour();
}

void ToggleLogs()
{
	static bool bBlock = false;

	if (!bBlock)
	{
		for (int i = 0; i < sizeof(g_LoggingDetours) / sizeof(*g_LoggingDetours); i++)
			g_LoggingDetours[i].EnableDetour();
	}
	else
	{
		for (int i = 0; i < sizeof(g_LoggingDetours) / sizeof(*g_LoggingDetours); i++)
			g_LoggingDetours[i].DisableDetour();
	}

	bBlock = !bBlock;
}

void FASTCALL Detour_UTIL_SayTextFilter(IRecipientFilter &, const char *, CBasePlayerController *, uint64);
void FASTCALL Detour_UTIL_SayText2Filter(IRecipientFilter &, CBasePlayerController *, uint64, const char *, const char *, const char *, const char *, const char *);
void FASTCALL Detour_Host_Say(CCSPlayerController *, CCommand *, bool, int, const char *);
bool FASTCALL Detour_VoiceShouldHear(CCSPlayerController *a, CCSPlayerController *b, bool a3, bool voice);
void FASTCALL Detour_CSoundEmitterSystem_EmitSound(ISoundEmitterSystemBase *, CEntityIndex *, IRecipientFilter &, uint32, void *);

DECLARE_DETOUR(Host_Say, Detour_Host_Say, &modules::server);
DECLARE_DETOUR(UTIL_SayTextFilter, Detour_UTIL_SayTextFilter, &modules::server);
DECLARE_DETOUR(UTIL_SayText2Filter, Detour_UTIL_SayText2Filter, &modules::server);
DECLARE_DETOUR(VoiceShouldHear, Detour_VoiceShouldHear, &modules::server);
DECLARE_DETOUR(CSoundEmitterSystem_EmitSound, Detour_CSoundEmitterSystem_EmitSound, &modules::server);

int g_iBlockSoundIndex = -1;

void FASTCALL Detour_CSoundEmitterSystem_EmitSound(ISoundEmitterSystemBase *pSoundEmitterSystem, CEntityIndex *a2, IRecipientFilter &filter, uint32 a4, void *a5)
{
	if (g_iBlockSoundIndex == -1)
		return CSoundEmitterSystem_EmitSound(pSoundEmitterSystem, a2, filter, a4, a5);

	CCopyRecipientFilter copyFilter(&filter, g_iBlockSoundIndex);
	CSoundEmitterSystem_EmitSound(pSoundEmitterSystem, a2, copyFilter, a4, a5);
}

bool FASTCALL Detour_VoiceShouldHear(CCSPlayerController* a, CCSPlayerController* b, bool a3, bool voice)
{
	if (voice)
	{
		ConMsg("block");
		return 0;
	}

	ConMsg("a: %s, b: %s\n", &a->m_iszPlayerName(), &b->m_iszPlayerName());
	ConMsg("VoiceShouldHear: %llx %llx %i %i\n", a, b, a3, voice);
	//Detour_VoiceShouldHear(a, b, a3, a4);

	return VoiceShouldHear(a, b, a3, voice);
}

void FASTCALL Detour_UTIL_SayTextFilter(IRecipientFilter &filter, const char *pText, CBasePlayerController *pPlayer, uint64 eMessageType)
{
	int entindex = filter.GetRecipientIndex(0).Get();
	CBasePlayerController *target = (CBasePlayerController *)g_pEntitySystem->GetBaseEntity((CEntityIndex)entindex);

	if (target)
		Message("[CS2Fixes] Chat from %s to %s: %s\n", pPlayer ? &pPlayer->m_iszPlayerName() : "console", &target->m_iszPlayerName(), pText);

	if (pPlayer)
		return UTIL_SayTextFilter(filter, pText, pPlayer, eMessageType);

	char buf[256];
	V_snprintf(buf, sizeof(buf), "%s%s", " \7CONSOLE:\4", pText + sizeof("Console:"));

	UTIL_SayTextFilter(filter, buf, pPlayer, eMessageType);
}

void FASTCALL Detour_UTIL_SayText2Filter(
	IRecipientFilter &filter, 
	CBasePlayerController *pEntity, 
	uint64 eMessageType, 
	const char *msg_name,
	const char *param1, 
	const char *param2, 
	const char *param3, 
	const char *param4)
{
	int entindex = filter.GetRecipientIndex(0).Get() + 1;
	CBasePlayerController *target = (CBasePlayerController *)g_pEntitySystem->GetBaseEntity((CEntityIndex)entindex);

	if (target)	
		Message("[CS2Fixes] Chat from %s to %s: %s\n", param1, &target->m_iszPlayerName(), param2);

	UTIL_SayText2Filter(filter, pEntity, eMessageType, msg_name, param1, param2, param3, param4);
}


void FASTCALL Detour_Host_Say(CCSPlayerController *pController, CCommand *args, bool teamonly, int unk1, const char *unk2)
{
	if(g_pEntitySystem == nullptr)
		g_pEntitySystem = interfaces::pGameResourceServiceServer->GetGameEntitySystem();

	const char *pMessage = args->Arg(1);

	if (*pMessage == '!' || *pMessage == '/')
		ParseChatCommand(pMessage, pController);

	if (*pMessage == '/')
		return;

	Host_Say(pController, args, teamonly, unk1, unk2);
}

void InitPatches()
{
	for (int i = 0; i < sizeof(g_CommonPatches) / sizeof(*g_CommonPatches); i++)
		g_CommonPatches[i].PerformPatch();

	// Dedicated servers don't load client
	if (!CommandLine()->HasParm("-dedicated"))
	{
		for (int i = 0; i < sizeof(g_ClientPatches) / sizeof(*g_ClientPatches); i++)
			g_ClientPatches[i].PerformPatch();
	}

#ifdef _WIN32
	// None of the tools are loaded without, well, -tools
	if (CommandLine()->HasParm("-tools"))
	{
		for (int i = 0; i < sizeof(g_ToolsPatches) / sizeof(*g_ToolsPatches); i++)
			g_ToolsPatches[i].PerformPatch();
	}
#endif
	
	UTIL_SayTextFilter.CreateDetour();
	UTIL_SayTextFilter.EnableDetour();

	UTIL_SayText2Filter.CreateDetour();
	UTIL_SayText2Filter.EnableDetour();

	Host_Say.CreateDetour();
	Host_Say.EnableDetour();

	VoiceShouldHear.CreateDetour();
	VoiceShouldHear.EnableDetour();

	CSoundEmitterSystem_EmitSound.CreateDetour();
	CSoundEmitterSystem_EmitSound.EnableDetour();
}

void UndoPatches()
{
	for (int i = 0; i < sizeof(g_CommonPatches) / sizeof(*g_CommonPatches); i++)
		g_CommonPatches[i].UndoPatch();

	if (!CommandLine()->HasParm("-dedicated"))
	{
		for (int i = 0; i < sizeof(g_ClientPatches) / sizeof(*g_ClientPatches); i++)
			g_ClientPatches[i].UndoPatch();
	}

#ifdef _WIN32
	if (CommandLine()->HasParm("-tools"))
	{
		for (int i = 0; i < sizeof(g_ToolsPatches) / sizeof(*g_ToolsPatches); i++)
			g_ToolsPatches[i].UndoPatch();
	}
#endif
}
