#pragma once

#include "platform.h"
#include "irecipientfilter.h"
#include "entitysystem.h"

extern CEntitySystem* g_pEntitySystem;

// #define HOOK_CONVARS
// #define HOOK_CONCOMMANDS
// #define USE_TICKRATE
// #define USE_DEBUG_CONSOLE

#define CS_TEAM_NONE        0
#define CS_TEAM_SPECTATOR   1
#define CS_TEAM_T           2
#define CS_TEAM_CT          3

#define MAXPLAYERS 64

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

void InitPatches();
void UndoPatches();

void Message(const char *, ...);
void Panic(const char *, ...);
