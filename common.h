#pragma once

#include "platform.h"
#include "irecipientfilter.h"
#include "entitysystem.h"

extern CEntitySystem* g_pEntitySystem;

// #define HOOK_CONVARS
// #define HOOK_CONCOMMANDS
// #define USE_TICKRATE
// #define USE_DEBUG_CONSOLE

#ifdef _WIN32
#define ROOTBIN "/bin/win64/"
#define GAMEBIN "/csgo/bin/win64/"
#else
#define ROOTBIN "/bin/linuxsteamrt64/"
#define GAMEBIN "/csgo/bin/linuxsteamrt64/"
#endif

void UnlockConVars();
void UnlockConCommands();
void ToggleLogs();
void SetMaxPlayers(byte);

void InitPatches();
void UndoPatches();
void InitLoggingDetours();

void Message(const char *, ...);
void Panic(const char *, ...);

void *FindSignature(void *, const byte *, size_t);

