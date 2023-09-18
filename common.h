#pragma once

#include "platform.h"

// #define HOOK_CONVARS
// #define HOOK_CONCOMMANDS
// #define USE_TICKRATE

#define ROOTBIN "/bin/win64/"
#define GAMEBIN "/csgo/bin/win64/"

#define SOURCE2SERVERCONFIG_INTERFACE_VERSION "Source2ServerConfig001"

#ifdef HOOK_CONVARS
struct ConVarInfo
{
	char *name;		  // 0x00
	char *desc;		  // 0x08
	uint64 flags;	  // 0x10
	char pad18[0x40]; // 0x18
	uint64 type;	  // 0x58
};

typedef void (*RegisterConVar)(void *, ConVarInfo *, void *, void *, void *);

void HookConVars();
#endif

#ifdef HOOK_CONCOMMANDS
struct ConCommandInfo
{
	char *name;	  // 0x00
	char *desc;	  // 0x08
	uint64 flags; // 0x10
};

typedef void *(*RegisterConCommand)(void *, void *, ConCommandInfo *, void *, void *);

void HookConCommands();
#endif

void UnlockConVars();
void UnlockConCommands();
void ToggleLogs();
void SetMaxPlayers(byte);

void InitPatches();
void InitLoggingDetours();

void Message(const char *, ...);
void Panic(const char *, ...);

void *FindPattern(void *, const byte *, const char *, size_t);
