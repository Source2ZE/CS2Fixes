#pragma once
#include "entity/ccsplayercontroller.h"

struct WeaponMapEntry_t
{
	const char *command;
	const char *szWeaponName;
	int iPrice;
};

void RegisterChatCommands();
void ParseChatCommand(const char *, CCSPlayerController *);