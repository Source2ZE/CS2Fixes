#pragma once

#include "gameconfig.h"
#include "mempatch.h"

extern CMemPatch g_CommonPatches[];

bool InitPatches(CGameConfig* gameConfig);
void UndoPatches();
