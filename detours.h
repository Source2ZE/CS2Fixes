#pragma once
#include "cdetour.h"
#include "entity/ccsplayercontroller.h"

void InitDetours();
void FlushAllDetours();

void FASTCALL Detour_UTIL_SayTextFilter(IRecipientFilter &, const char *, CCSPlayerController *, uint64);
void FASTCALL Detour_UTIL_SayText2Filter(IRecipientFilter &, CCSPlayerController *, uint64, const char *, const char *, const char *, const char *, const char *);
void FASTCALL Detour_Host_Say(CCSPlayerController *, CCommand *, bool, int, const char *);
bool FASTCALL Detour_VoiceShouldHear(CCSPlayerController *a, CCSPlayerController *b, bool a3, bool voice);
void FASTCALL Detour_CSoundEmitterSystem_EmitSound(ISoundEmitterSystemBase *, CEntityIndex *, IRecipientFilter &, uint32, void *);

extern CDetour<decltype(Detour_Host_Say)> Host_Say;
extern CDetour<decltype(Detour_UTIL_SayTextFilter)> UTIL_SayTextFilter;
