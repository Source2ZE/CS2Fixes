/**
 * =============================================================================
 * CS2Fixes
 * Copyright (C) 2023-2024 Source2ZE
 * =============================================================================
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License, version 3.0, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once
#include "platform.h"
#include "stdint.h"
#include "utils/module.h"
#include "utlstring.h"
#include "variant.h"
#include "gametrace.h"

namespace modules
{
	inline CModule *engine;
	inline CModule *tier0;
	inline CModule *server;
	inline CModule *schemasystem;
	inline CModule *vscript;
	inline CModule *client;
	inline CModule* networksystem;
#ifdef _WIN32
	inline CModule *hammer;
#endif
}

class CEntityInstance;
class CEntityIdentity;
class CBasePlayerController;
class CCSPlayerController;
class CCSPlayerPawn;
class CBaseModelEntity;
class Z_CBaseEntity;
class CGameConfig;
class CEntitySystem;
class IEntityFindFilter;
class CGameRules;
class CEntityKeyValues;
class IRecipientFilter;
class CTraceFilterPlayerMovementCS;
class CTraceFilter;
struct bbox_t;

struct SndOpEventGuid_t;

namespace addresses
{
	bool Initialize(CGameConfig *g_GameConfig);

	inline void(FASTCALL *NetworkStateChanged)(int64 chainEntity, int64 offset, int64 a3);
	inline void(FASTCALL *StateChanged)(void *networkTransmitComponent, CEntityInstance *ent, int64 offset, int16 a4, int16 a5);
	inline void(FASTCALL *CBasePlayerController_SetPawn)(CBasePlayerController *pController, CCSPlayerPawn *pPawn, bool a3, bool a4);

	// typedef void InitPlayerMovementTraceFilter_t(CTraceFilterPlayerMovementCS &pFilter, CEntityInstance *pHandleEntity, uint64_t interactWith, int collisionGroup);
	inline void(FASTCALL *InitPlayerMovementTraceFilter)(CTraceFilterPlayerMovementCS &pFilter, CEntityInstance *pHandleEntity, uint64_t interactWith, int collisionGroup);
	// typedef void TracePlayerBBox_t(const Vector &start, const Vector &end, const bbox_t &bounds, CTraceFilterPlayerMovementCS *filter, trace_t_s2 &pm);
	inline void(FASTCALL *TracePlayerBBox)(const Vector &start, const Vector &end, const bbox_t &bounds, CTraceFilter *filter, trace_t &pm);
	inline void(FASTCALL*InitGameTrace)(trace_t *trace);
	
}
