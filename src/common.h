#pragma once

#include "platform.h"

// #define USE_DEBUG_CONSOLE

#define CS_TEAM_NONE        0
#define CS_TEAM_SPECTATOR   1
#define CS_TEAM_T           2
#define CS_TEAM_CT          3

#define HUD_PRINTNOTIFY		1
#define HUD_PRINTCONSOLE	2
#define HUD_PRINTTALK		3
#define HUD_PRINTCENTER		4

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
