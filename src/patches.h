#pragma once

#include "mempatch.h"

extern CMemPatch g_CommonPatches[];

bool InitPatches();
void UndoPatches();
