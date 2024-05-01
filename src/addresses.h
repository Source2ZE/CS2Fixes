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
class CTakeDamageInfo;
class INetworkStringTable;

struct EmitSound_t;
struct SndOpEventGuid_t;

namespace addresses
{
	bool Initialize(CGameConfig *g_GameConfig);

	inline void(FASTCALL *NetworkStateChanged)(int64 chainEntity, int64 offset, int64 a3);
	inline void(FASTCALL *StateChanged)(void *networkTransmitComponent, CEntityInstance *ent, int64 offset, int16 a4, int16 a5);
	inline void(FASTCALL *UTIL_ClientPrintAll)(int msg_dest, const char *msg_name, const char *param1, const char *param2, const char *param3, const char *param4);
	inline void(FASTCALL *ClientPrint)(CBasePlayerController *player, int msg_dest, const char *msg_name, const char *param1, const char *param2, const char *param3, const char *param4);
	inline void(FASTCALL *SetGroundEntity)(Z_CBaseEntity *ent, Z_CBaseEntity *ground);
	inline void(FASTCALL *CCSPlayerController_SwitchTeam)(CCSPlayerController *pController, uint32 team);
	inline void(FASTCALL *CBasePlayerController_SetPawn)(CBasePlayerController *pController, CCSPlayerPawn *pPawn, bool a3, bool a4);
	inline void(FASTCALL *CBaseModelEntity_SetModel)(CBaseModelEntity *pModel, const char *szModel);
	inline void(FASTCALL *UTIL_Remove)(CEntityInstance*);

	inline void(FASTCALL *CEntitySystem_AddEntityIOEvent)(CEntitySystem *pEntitySystem, CEntityInstance *pTarget, const char *pszInput,
														CEntityInstance *pActivator, CEntityInstance *pCaller, variant_t *value, float flDelay, int outputID);

	inline void(FASTCALL *CEntityInstance_AcceptInput)(CEntityInstance *pThis, const char *pInputName,
													CEntityInstance *pActivator, CEntityInstance *pCaller, variant_t *value, int nOutputID);

	inline Z_CBaseEntity *(FASTCALL *CGameEntitySystem_FindEntityByClassName)(CEntitySystem *pEntitySystem, CEntityInstance *pStartEntity, const char *szName);

	inline Z_CBaseEntity *(FASTCALL *CGameEntitySystem_FindEntityByName)(CEntitySystem *pEntitySystem, CEntityInstance *pStartEntity, const char *szName, 
																		CEntityInstance *pSearchingEntity, CEntityInstance *pActivator, CEntityInstance *pCaller,
																		IEntityFindFilter *pFilter);
	inline void(FASTCALL *CGameRules_TerminateRound)(CGameRules* pGameRules, float delay, unsigned int reason, int64 a4, unsigned int a5);
	inline Z_CBaseEntity *(FASTCALL* CreateEntityByName)(const char* className, int iForceEdictIndex);
	inline void(FASTCALL *DispatchSpawn)(Z_CBaseEntity* pEntity, CEntityKeyValues *pEntityKeyValues);
	inline void(FASTCALL *CEntityIdentity_SetEntityName)(CEntityIdentity *pEntity, const char *pName);
	inline void(FASTCALL *CBaseEntity_EmitSoundParams)(Z_CBaseEntity *pEntity, const char *pszSound, int nPitch, float flVolume, float flDelay);
	inline void(FASTCALL *CBaseEntity_SetParent)(Z_CBaseEntity *pEntity, Z_CBaseEntity *pNewParent, CUtlStringToken nBoneOrAttachName, matrix3x4a_t *pOffsetTransform);
	inline int(FASTCALL *DispatchParticleEffect)(const char *pszParticleName, int iAttachType, Z_CBaseEntity *pEntity,
		char iAttachmentPoint, CUtlSymbolLarge iAttachmentName, bool bResetAllParticlesOnEntity, int nSplitScreenPlayerSlot, IRecipientFilter *a7, byte *a8);
	inline SndOpEventGuid_t(FASTCALL *CBaseEntity_EmitSoundFilter)(IRecipientFilter &filter, CEntityIndex ent, const EmitSound_t &params);
	inline void(FASTCALL *CBaseEntity_SetMoveType)(Z_CBaseEntity *pThis, MoveType_t nMoveType, MoveCollide_t nMoveCollide);
	inline void(FASTCALL *CTakeDamageInfo_Constructor)(CTakeDamageInfo *pThis, Z_CBaseEntity *pInflictor, Z_CBaseEntity *pAttacker, Z_CBaseEntity *pAbility,
		const Vector *vecDamageForce, const Vector *vecDamagePosition, float flDamage, int bitsDamageType, int iCustomDamage, void *a10);
	inline void(FASTCALL *CNetworkStringTable_DeleteAllStrings)(INetworkStringTable *pThis);
}
