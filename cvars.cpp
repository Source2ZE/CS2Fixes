#include "common.h"
#include "icvar.h"
#include <Psapi.h>

#include "tier0/memdbgon.h"

extern MODULEINFO g_Tier0Info;

static uint64 g_iFlagsToRemove = (FCVAR_HIDDEN | FCVAR_DEVELOPMENTONLY | FCVAR_MISSING0 | FCVAR_MISSING1 | FCVAR_MISSING2 | FCVAR_MISSING3);

void UnlockConVars()
{
	if (!g_pCVar)
		return;

	int iUnhiddenConVars = 0;

	ConVar *pCvar = nullptr;
	ConVarHandle hCvarHandle;
	hCvarHandle.Set(0);

	// Can't use FindFirst/Next here as it would skip cvars with certain flags, so just loop through the handles
	do
	{
		pCvar = g_pCVar->GetConVar(hCvarHandle);

#ifdef _DEBUG
		printf("CONVAR %d: %s\n", hCvarHandle.Get(), pCvar ? pCvar->m_pszName : "---------NULL");
#endif

		hCvarHandle.Set(hCvarHandle.Get() + 1);

		if (!pCvar || !(pCvar->flags & g_iFlagsToRemove))
			continue;

		pCvar->flags &= ~g_iFlagsToRemove;
		iUnhiddenConVars++;
	} while (pCvar);

	Message("[CS2Fixes] Removed hidden flags from %d convars\n", iUnhiddenConVars);
}

void UnlockConCommands()
{
	if (!g_pCVar)
		return;

	ConCommand *say = g_pCVar->GetCommand(g_pCVar->FindCommand("say"));

	int iUnhiddenConCommands = 0;

	ConCommand *pConCommand = nullptr;
	ConCommand *pInvalidCommand = g_pCVar->GetCommand(ConCommandHandle());
	ConCommandHandle hConCommandHandle;
	hConCommandHandle.Set(0);

	do
	{
		pConCommand = g_pCVar->GetCommand(hConCommandHandle);

#ifdef _DEBUG
		printf("COMMAND %d: %s\n", hConCommandHandle.Get(), pConCommand && pConCommand != pInvalidCommand ? pConCommand->GetName() : "---------NULL");
#endif

		hConCommandHandle.Set(hConCommandHandle.Get() + 1);

		if (!pConCommand || pConCommand == pInvalidCommand || !(pConCommand->GetFlags() & g_iFlagsToRemove))
			continue;

		pConCommand->RemoveFlags(g_iFlagsToRemove);
		iUnhiddenConCommands++;
	} while (pConCommand && pConCommand != pInvalidCommand);

	Message("[CS2Fixes] Removed hidden flags from %d commands\n", iUnhiddenConCommands);
}

#ifdef HOOK_CONVARS
void *g_pfnRegisterConvar = nullptr;

void RegisterConVarHook(void *self, ConVarInfo *info, void *unk1, void *unk2, void *unk3)
{
#ifdef _DEBUG
	printf("CONVAR : %s\n", info->name);
#endif

	// Unhide hidden and devonly convars
	info->flags &= ~g_iFlagsToRemove;

	((RegisterConVar)g_pfnRegisterConvar)(self, info, unk1, unk2, unk3);
}

void HookConVars()
{
	// use string "%s %s has conflicting %s flags (child: %s%s, parent: %s%s, parent" to find this shit
	void *pRegisterConvar = FindPattern(g_Tier0Info.lpBaseOfDll, (byte *)"\x48\x8B\xC4\x4C\x89\x48\x20\x4C\x89\x40\x18\x48\x89\x50\x10\x55", "xxxxxxxxxxxxxxxx", g_Tier0Info.SizeOfImage);

	if (pRegisterConvar)
	{
		MH_CreateHook(pRegisterConvar, RegisterConVarHook, &g_pfnRegisterConvar);
		MH_EnableHook(pRegisterConvar);
	}
	else
	{
		Panic("[CS2Fixes] Failed to find signature for RegisterConvar!");
	}
}
#endif // HOOK_CONVARS

#ifdef HOOK_CONCOMMANDS
void *g_pfnRegisterConCommand = nullptr;

void *RegisterConCommandHook(void *self, void *unk1, ConCommandInfo *info, void *unk2, void *unk3)
{
#ifdef _DEBUG
	printf("CONCOMMAND : %s\n", info->name);
#endif

	// Unhide hidden and devonly commands
	info->flags &= ~g_iFlagsToRemove;

	return ((RegisterConCommand)g_pfnRegisterConCommand)(self, unk1, info, unk2, unk3);
}

void HookConCommands()
{
	void *pRegisterConCommand = FindPattern(g_Tier0Info.lpBaseOfDll, (byte *)"\x48\x8B\xC4\x4C\x89\x48\x20\x4C\x89\x40\x18\x56", "xxxxxxxxxxxx", g_Tier0Info.SizeOfImage);

	if (pRegisterConCommand)
	{
		MH_CreateHook(pRegisterConCommand, RegisterConCommandHook, &g_pfnRegisterConCommand);
		MH_EnableHook(pRegisterConCommand);
	}
	else
	{
		Panic("[CS2Fixes] Failed to find signature for RegisterConCommand!");
	}
}
#endif // HOOK_CONCOMMANDS
