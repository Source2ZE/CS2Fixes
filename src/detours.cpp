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

#include "networkbasetypes.pb.h"
#include "usercmd.pb.h"
#include "cs_usercmd.pb.h"

#include "cdetour.h"
#include "common.h"
#include "module.h"
#include "addresses.h"
#include "commands.h"
#include "detours.h"
#include "ctimer.h"
#include "irecipientfilter.h"
#include "entity/ccsplayercontroller.h"
#include "entity/ccsplayerpawn.h"
#include "entity/cbasemodelentity.h"
#include "entity/ccsweaponbase.h"
#include "entity/ctriggerpush.h"
#include "entity/cgamerules.h"
#include "entity/ctakedamageinfo.h"
#include "entity/services.h"
#include "playermanager.h"
#include "igameevents.h"
#include "gameconfig.h"
#include "zombiereborn.h"
#include "customio.h"
#include "entities.h"
#include "serversideclient.h"
#include "networksystem/inetworkserializer.h"
#include "map_votes.h"

#define VPROF_ENABLED
#include "tier0/vprof.h"

#include "tier0/memdbgon.h"

extern CGlobalVars *gpGlobals;
extern CGameEntitySystem *g_pEntitySystem;
extern IGameEventManager2 *g_gameEventManager;
extern CCSGameRules *g_pGameRules;
extern CMapVoteSystem *g_pMapVoteSystem;

CUtlVector<CDetourBase *> g_vecDetours;

DECLARE_DETOUR(UTIL_SayTextFilter, Detour_UTIL_SayTextFilter);
DECLARE_DETOUR(UTIL_SayText2Filter, Detour_UTIL_SayText2Filter);
DECLARE_DETOUR(IsHearingClient, Detour_IsHearingClient);
DECLARE_DETOUR(TriggerPush_Touch, Detour_TriggerPush_Touch);
DECLARE_DETOUR(CGameRules_Constructor, Detour_CGameRules_Constructor);
DECLARE_DETOUR(CBaseEntity_TakeDamageOld, Detour_CBaseEntity_TakeDamageOld);
DECLARE_DETOUR(CCSPlayer_WeaponServices_CanUse, Detour_CCSPlayer_WeaponServices_CanUse);
DECLARE_DETOUR(CEntityIdentity_AcceptInput, Detour_CEntityIdentity_AcceptInput);
DECLARE_DETOUR(CNavMesh_GetNearestNavArea, Detour_CNavMesh_GetNearestNavArea);
DECLARE_DETOUR(CNetworkStringTable_AddString, Detour_AddString);
DECLARE_DETOUR(ProcessMovement, Detour_ProcessMovement);
DECLARE_DETOUR(TryPlayerMove, Detour_TryPlayerMove);
DECLARE_DETOUR(CategorizePosition, Detour_CategorizePosition);
DECLARE_DETOUR(ProcessUsercmds, Detour_ProcessUsercmds);
DECLARE_DETOUR(CGamePlayerEquip_InputTriggerForAllPlayers, Detour_CGamePlayerEquip_InputTriggerForAllPlayers);
DECLARE_DETOUR(CGamePlayerEquip_InputTriggerForActivatedPlayer, Detour_CGamePlayerEquip_InputTriggerForActivatedPlayer);
DECLARE_DETOUR(CCSGameRules_GoToIntermission, Detour_CCSGameRules_GoToIntermission);

void FASTCALL Detour_CGameRules_Constructor(CGameRules *pThis)
{
	g_pGameRules = (CCSGameRules*)pThis;
	CGameRules_Constructor(pThis);
}

static bool g_bBlockMolotovSelfDmg = false;
static bool g_bBlockAllDamage = false;

FAKE_BOOL_CVAR(cs2f_block_molotov_self_dmg, "Whether to block self-damage from molotovs", g_bBlockMolotovSelfDmg, false, false)
FAKE_BOOL_CVAR(cs2f_block_all_dmg, "Whether to block all damage to players", g_bBlockAllDamage, false, false)

void FASTCALL Detour_CBaseEntity_TakeDamageOld(Z_CBaseEntity *pThis, CTakeDamageInfo *inputInfo)
{
#ifdef _DEBUG
	Message("\n--------------------------------\n"
			"TakeDamage on %s\n"
			"Attacker: %s\n"
			"Inflictor: %s\n"
			"Ability: %s\n"
			"Damage: %.2f\n"
			"Damage Type: %i\n"
			"--------------------------------\n",
			pThis->GetClassname(),
			inputInfo->m_hAttacker.Get() ? inputInfo->m_hAttacker.Get()->GetClassname() : "NULL",
			inputInfo->m_hInflictor.Get() ? inputInfo->m_hInflictor.Get()->GetClassname() : "NULL",
			inputInfo->m_hAbility.Get() ? inputInfo->m_hAbility.Get()->GetClassname() : "NULL",
			inputInfo->m_flDamage,
			inputInfo->m_bitsDamageType);
#endif
	
	// Block all player damage if desired
	if (g_bBlockAllDamage && pThis->IsPawn())
		return;

	CBaseEntity *pInflictor = inputInfo->m_hInflictor.Get();
	const char *pszInflictorClass = pInflictor ? pInflictor->GetClassname() : "";

	// Prevent everything but nades from inflicting blast damage
	if (inputInfo->m_bitsDamageType == DamageTypes_t::DMG_BLAST && V_strncmp(pszInflictorClass, "hegrenade", 9))
		inputInfo->m_bitsDamageType = DamageTypes_t::DMG_GENERIC;

	if (g_bEnableZR && ZR_Detour_TakeDamageOld((CCSPlayerPawn*)pThis, inputInfo))
		return;

	// Prevent molly on self
	if (g_bBlockMolotovSelfDmg && inputInfo->m_hAttacker == pThis && !V_strncmp(pszInflictorClass, "inferno", 7))
		return;

	CBaseEntity_TakeDamageOld(pThis, inputInfo);
}

static bool g_bUseOldPush = false;
FAKE_BOOL_CVAR(cs2f_use_old_push, "Whether to use the old CSGO trigger_push behavior", g_bUseOldPush, false, false)

static bool g_bLogPushes = false;
FAKE_BOOL_CVAR(cs2f_log_pushes, "Whether to log pushes (cs2f_use_old_push must be enabled)", g_bLogPushes, false, false)

void FASTCALL Detour_TriggerPush_Touch(CTriggerPush* pPush, Z_CBaseEntity* pOther)
{
	// This trigger pushes only once (and kills itself) or pushes only on StartTouch, both of which are fine already
	if (!g_bUseOldPush || pPush->m_spawnflags() & SF_TRIG_PUSH_ONCE || pPush->m_bTriggerOnStartTouch())
	{
		TriggerPush_Touch(pPush, pOther);
		return;
	}

	MoveType_t movetype = pOther->m_nActualMoveType();

	// VPhysics handling doesn't need any changes
	if (movetype == MOVETYPE_VPHYSICS)
	{
		TriggerPush_Touch(pPush, pOther);
		return;
	}

	if (movetype == MOVETYPE_NONE || movetype == MOVETYPE_PUSH || movetype == MOVETYPE_NOCLIP)
		return;

	CCollisionProperty* collisionProp = pOther->m_pCollision();
	if (!IsSolid(collisionProp->m_nSolidType(), collisionProp->m_usSolidFlags()))
		return;

	if (!pPush->PassesTriggerFilters(pOther))
		return;

	if (pOther->m_CBodyComponent()->m_pSceneNode()->m_pParent())
		return;

	Vector vecAbsDir;

	matrix3x4_t mat = pPush->m_CBodyComponent()->m_pSceneNode()->EntityToWorldTransform();
	
	Vector pushDir = pPush->m_vecPushDirEntitySpace();

	// i had issues with vectorrotate on linux so i did it here
	vecAbsDir.x = pushDir.x * mat[0][0] + pushDir.y * mat[0][1] + pushDir.z * mat[0][2];
	vecAbsDir.y = pushDir.x * mat[1][0] + pushDir.y * mat[1][1] + pushDir.z * mat[1][2];
	vecAbsDir.z = pushDir.x * mat[2][0] + pushDir.y * mat[2][1] + pushDir.z * mat[2][2];

	Vector vecPush = vecAbsDir * pPush->m_flSpeed();

	uint32 flags = pOther->m_fFlags();

	if (flags & (FL_BASEVELOCITY))
	{
		vecPush = vecPush + pOther->m_vecBaseVelocity();
	}

	if (vecPush.z > 0 && (flags & FL_ONGROUND))
	{
		addresses::SetGroundEntity(pOther, nullptr);
		Vector origin = pOther->GetAbsOrigin();
		origin.z += 1.0f;

		pOther->Teleport(&origin, nullptr, nullptr);
	}

	if (g_bLogPushes)
	{
		Vector vecEntBaseVelocity = pOther->m_vecBaseVelocity;
		Vector vecOrigPush = vecAbsDir * pPush->m_flSpeed();

		Message("Pushing entity %i | frametime = %.3f | entity basevelocity = %.2f %.2f %.2f | original push velocity = %.2f %.2f %.2f | final push velocity = %.2f %.2f %.2f\n",
			pOther->GetEntityIndex(),
			gpGlobals->frametime,
			vecEntBaseVelocity.x, vecEntBaseVelocity.y, vecEntBaseVelocity.z,
			vecOrigPush.x, vecOrigPush.y, vecOrigPush.z,
			vecPush.x, vecPush.y, vecPush.z);
	}

	pOther->m_vecBaseVelocity(vecPush);

	flags |= (FL_BASEVELOCITY);
	pOther->m_fFlags(flags);
}

bool FASTCALL Detour_IsHearingClient(void* serverClient, int index)
{
	ZEPlayer* player = g_playerManager->GetPlayer(index);
	if (player && player->IsMuted())
		return false;

	return IsHearingClient(serverClient, index);
}

void SayChatMessageWithTimer(IRecipientFilter &filter, const char *pText, CCSPlayerController *pPlayer, uint64 eMessageType)
{
	char buf[256];

	// Filter console message - remove non-alphanumeric chars and convert to lowercase
	uint32 uiTextLength = strlen(pText);
	uint32 uiFilteredTextLength = 0;
	char filteredText[256];

	for (uint32 i = 0; i < uiTextLength; i++)
	{
		if (pText[i] >= 'A' && pText[i] <= 'Z')
			filteredText[uiFilteredTextLength++] = pText[i] + 32;
		if (pText[i] == ' ' || (pText[i] >= '0' && pText[i] <= '9') || (pText[i] >= 'a' && pText[i] <= 'z'))
			filteredText[uiFilteredTextLength++] = pText[i];
	}
	filteredText[uiFilteredTextLength] = '\0';

	// Split console message into words seperated by the space character
	CUtlVector<char*, CUtlMemory<char*, int>> words;
	V_SplitString(filteredText, " ", words);

	//Word count includes the first word "Console:" at index 0, first relevant word is at index 1
	int iWordCount = words.Count();
	uint32 uiTriggerTimerLength = 0;

	if (iWordCount == 2)
		uiTriggerTimerLength = V_StringToUint32(words.Element(1), 0, NULL, NULL, PARSING_FLAG_SKIP_WARNING);

	for (int i = 1; i < iWordCount && uiTriggerTimerLength == 0; i++)
	{
		uint32 uiCurrentValue = V_StringToUint32(words.Element(i), 0, NULL, NULL, PARSING_FLAG_SKIP_WARNING);
		uint32 uiNextWordLength = 0;
		char* pNextWord = NULL;

		if (i + 1 < iWordCount)
		{
			pNextWord = words.Element(i + 1);
			uiNextWordLength = strlen(pNextWord);
		}

		// Case: ... X sec(onds) ... or ... X min(utes) ...
		if (pNextWord != NULL && uiNextWordLength > 2 && uiCurrentValue > 0)
		{
			if (pNextWord[0] == 's' && pNextWord[1] == 'e' && pNextWord[2] == 'c')
				uiTriggerTimerLength = uiCurrentValue;
			if (pNextWord[0] == 'm' && pNextWord[1] == 'i' && pNextWord[2] == 'n')
				uiTriggerTimerLength = uiCurrentValue * 60;
		}

		// Case: ... Xs - only support up to 3 digit numbers (in seconds) for this timer parse method
		if (uiCurrentValue == 0)
		{
			char* pCurrentWord = words.Element(i);
			uint32 uiCurrentScanLength = MIN(strlen(pCurrentWord), 4);

			for (uint32 j = 0; j < uiCurrentScanLength; j++)
			{
				if (pCurrentWord[j] >= '0' && pCurrentWord[j] <= '9')
					continue;
				
				if (pCurrentWord[j] == 's')
				{
					pCurrentWord[j] = '\0';
					uiTriggerTimerLength = V_StringToUint32(pCurrentWord, 0, NULL, NULL, PARSING_FLAG_SKIP_WARNING);
				}
				break;
			}
		}
	}
	words.PurgeAndDeleteElements();

	float fCurrentRoundClock = g_pGameRules->m_iRoundTime - (gpGlobals->curtime - g_pGameRules->m_fRoundStartTime.Get().m_Value);

	// Only display trigger time if the timer is greater than 4 seconds, and time expires within the round
	if ((uiTriggerTimerLength > 4) && (fCurrentRoundClock > uiTriggerTimerLength))
	{
		int iTriggerTime = fCurrentRoundClock - uiTriggerTimerLength;

		// Round timer to nearest whole second
		if ((int)(fCurrentRoundClock - 0.5f) == (int)fCurrentRoundClock)
			iTriggerTime++;

		int mins = iTriggerTime / 60;
		int secs = iTriggerTime % 60;

		V_snprintf(buf, sizeof(buf), "%s %s %s %2d:%02d", " \7CONSOLE:\4", pText + sizeof("Console:"), "\x10- @", mins, secs);
	}
	else
		V_snprintf(buf, sizeof(buf), "%s %s", " \7CONSOLE:\4", pText + sizeof("Console:"));

	UTIL_SayTextFilter(filter, buf, pPlayer, eMessageType);
}

bool g_bEnableTriggerTimer = false;

FAKE_BOOL_CVAR(cs2f_trigger_timer_enable, "Whether to process countdown messages said by Console (e.g. Hold for 10 seconds) and append the round time where the countdown resolves", g_bEnableTriggerTimer, false, false)

void FASTCALL Detour_UTIL_SayTextFilter(IRecipientFilter &filter, const char *pText, CCSPlayerController *pPlayer, uint64 eMessageType)
{
	if (pPlayer)
		return UTIL_SayTextFilter(filter, pText, pPlayer, eMessageType);

	if (g_bEnableTriggerTimer)
		return SayChatMessageWithTimer(filter, pText, pPlayer, eMessageType);

	char buf[256];
	V_snprintf(buf, sizeof(buf), "%s %s", " \7CONSOLE:\4", pText + sizeof("Console:"));

	UTIL_SayTextFilter(filter, buf, pPlayer, eMessageType);
}

void FASTCALL Detour_UTIL_SayText2Filter(
	IRecipientFilter &filter,
	CCSPlayerController *pEntity,
	uint64 eMessageType,
	const char *msg_name,
	const char *param1,
	const char *param2,
	const char *param3,
	const char *param4)
{
#ifdef _DEBUG
    CPlayerSlot slot = filter.GetRecipientIndex(0);
	CCSPlayerController* target = CCSPlayerController::FromSlot(slot);

	if (target)
		Message("Chat from %s to %s: %s\n", param1, target->GetPlayerName(), param2);
#endif

	UTIL_SayText2Filter(filter, pEntity, eMessageType, msg_name, param1, param2, param3, param4);
}

bool FASTCALL Detour_CCSPlayer_WeaponServices_CanUse(CCSPlayer_WeaponServices *pWeaponServices, CBasePlayerWeapon* pPlayerWeapon)
{
	if (g_bEnableZR && !ZR_Detour_CCSPlayer_WeaponServices_CanUse(pWeaponServices, pPlayerWeapon))
	{
		return false;
	}

	return CCSPlayer_WeaponServices_CanUse(pWeaponServices, pPlayerWeapon);
}

bool FASTCALL Detour_CEntityIdentity_AcceptInput(CEntityIdentity* pThis, CUtlSymbolLarge* pInputName, CEntityInstance* pActivator, CEntityInstance* pCaller, variant_t* value, int nOutputID)
{
	if (g_bEnableZR)
		ZR_Detour_CEntityIdentity_AcceptInput(pThis, pInputName, pActivator, pCaller, value, nOutputID);

	// Handle KeyValue(s)
	if (!V_strnicmp(pInputName->String(), "KeyValue", 8))
	{
		if ((value->m_type == FIELD_CSTRING || value->m_type == FIELD_STRING) && value->m_pszString)
		{
			// always const char*, even if it's FIELD_STRING (that is bug string from lua 'EntFire')
			return CustomIO_HandleInput(pThis->m_pInstance, value->m_pszString, pActivator, pCaller);
		}
		Message("Invalid value type for input %s\n", pInputName->String());
		return false;
	}
	if (!V_strnicmp(pInputName->String(), "IgniteL", 7)) // Override IgniteLifetime
	{
		float flDuration = 0.f;

		if ((value->m_type == FIELD_CSTRING || value->m_type == FIELD_STRING) && value->m_pszString)
			flDuration = V_StringToFloat32(value->m_pszString, 0.f);
		else
			flDuration = value->m_float;

		CCSPlayerPawn *pPawn = reinterpret_cast<CCSPlayerPawn*>(pThis->m_pInstance);

		if (pPawn->IsPawn() && IgnitePawn(pPawn, flDuration, pPawn, pPawn))
			return true;
	}
	else if (const auto pGameUI = reinterpret_cast<Z_CBaseEntity*>(pThis->m_pInstance)->AsGameUI())
	{
		if (!V_strcasecmp(pInputName->String(), "Activate"))
			return CGameUIHandler::OnActivate(pGameUI, reinterpret_cast<Z_CBaseEntity*>(pActivator));
		if (!V_strcasecmp(pInputName->String(), "Deactivate"))
			return CGameUIHandler::OnDeactivate(pGameUI, reinterpret_cast<Z_CBaseEntity*>(pActivator));
	}

    return CEntityIdentity_AcceptInput(pThis, pInputName, pActivator, pCaller, value, nOutputID);
}

bool g_bBlockNavLookup = false;

FAKE_BOOL_CVAR(cs2f_block_nav_lookup, "Whether to block navigation mesh lookup, improves server performance but breaks bot navigation", g_bBlockNavLookup, false, false)

void* FASTCALL Detour_CNavMesh_GetNearestNavArea(int64_t unk1, float* unk2, unsigned int* unk3, unsigned int unk4, int64_t unk5, int64_t unk6, float unk7, int64_t unk8)
{
	if (g_bBlockNavLookup)
		return nullptr;

	return CNavMesh_GetNearestNavArea(unk1, unk2, unk3, unk4, unk5, unk6, unk7, unk8);
}

bool g_bBlockEntityStrings = false;
FAKE_BOOL_CVAR(cs2f_block_entity_strings, "Whether to block adding entries in the EntityNames stringtable", g_bBlockEntityStrings, false, false);

int64 FASTCALL Detour_AddString(void *pStringTable, bool bServer, const char *pszString, void *a4)
{
	if (!g_bBlockEntityStrings)
		return CNetworkStringTable_AddString(pStringTable, bServer, pszString, a4);

	static int offset = g_GameConfig->GetOffset("CNetworkStringTable_GetTableName");
	const char *pszStringTableName = CALL_VIRTUAL(const char *, offset, pStringTable);

	// The whole name is "EntityNames" so do the bare minimum comparison, since no other table starts with "Ent"
	if (!V_strncmp(pszStringTableName, "Ent", 3))
		return -1;

	return CNetworkStringTable_AddString(pStringTable, bServer, pszString, a4);
}

void FASTCALL Detour_ProcessMovement(CCSPlayer_MovementServices *pThis, void *pMove)
{
	CCSPlayerPawn *pPawn = pThis->GetPawn();

	ZEPlayer *player = g_playerManager->GetPlayer(pPawn->m_hController()->GetPlayerSlot());
	if(!player) return ProcessMovement(pThis, pMove);
	
	player->currentMoveData = static_cast<CMoveData*>(pMove);
	player->didTPM = false;
	player->processingMovement = true;

	if (!pPawn->IsAlive())
	{
		ProcessMovement(pThis, pMove);
		if(!player->didTPM)
			player->lastValidPlane = vec3_origin;
		player->processingMovement = false;
		return;
	}

	CCSPlayerController *pController = pPawn->GetOriginalController();

	if (!pController || !pController->IsConnected())
	{
		ProcessMovement(pThis, pMove);
		if(!player->didTPM)
			player->lastValidPlane = vec3_origin;
		player->processingMovement = false;
		return;
	}

	float flSpeedMod = pController->GetZEPlayer()->GetSpeedMod();

	if (flSpeedMod == 1.f)
	{
		ProcessMovement(pThis, pMove);
		if(!player->didTPM)
			player->lastValidPlane = vec3_origin;
		player->processingMovement = false;
		return;
	}


	// Yes, this is what source1 does to scale player speed
	// Scale frametime during the entire movement processing step and revert right after
	float flStoreFrametime = gpGlobals->frametime;

	gpGlobals->frametime *= flSpeedMod;

	ProcessMovement(pThis, pMove);
	
	if(!player->didTPM)
		player->lastValidPlane = vec3_origin;

	gpGlobals->frametime = flStoreFrametime;
	
	player->processingMovement = false;
}

#define f32 float32
#define i32 int32_t
#define u32 uint32_t

void ClipVelocity(Vector &in, Vector &normal, Vector &out)
{
	// Determine how far along plane to slide based on incoming direction.
	f32 backoff = DotProduct(in, normal);

	for (i32 i = 0; i < 3; i++)
	{
		f32 change = normal[i] * backoff;
		out[i] = in[i] - change;
	}
	float adjust = DotProduct(out, normal);
	if (adjust < 0.0f)
	{
		adjust = MIN(adjust, -1 / 128);
		out -= (normal * adjust);
	}
}

bool IsValidMovementTrace(trace_t_s2 &tr, bbox_t bounds, CTraceFilterPlayerMovementCS *filter)
{
	trace_t_s2 stuck;
	// Maybe we don't need this one.
	// if (tr.fraction < FLT_EPSILON)
	//{
	//	return false;
	//}

	if (tr.startsolid)
	{
		return false;
	}

	// We hit something but no valid plane data?
	if (tr.fraction < 1.0f && fabs(tr.planeNormal.x) < FLT_EPSILON && fabs(tr.planeNormal.y) < FLT_EPSILON && fabs(tr.planeNormal.z) < FLT_EPSILON)
	{
		return false;
	}

	// Is the plane deformed?
	if (fabs(tr.planeNormal.x) > 1.0f || fabs(tr.planeNormal.y) > 1.0f || fabs(tr.planeNormal.z) > 1.0f)
	{
		return false;
	}

	// Do an unswept trace and a backward trace just to be sure.
	addresses::TracePlayerBBox(tr.endpos, tr.endpos, bounds, filter, stuck);
	if (stuck.startsolid || stuck.fraction < 1.0f - FLT_EPSILON)
	{
		return false;
	}

	addresses::TracePlayerBBox(tr.endpos, tr.startpos, bounds, filter, stuck);
	// For whatever reason if you can hit something in only one direction and not the other way around.
	// Only happens since Call to Arms update, so this fraction check is commented out until it is fixed.
	if (stuck.startsolid /*|| stuck.fraction < 1.0f - FLT_EPSILON*/)
	{
		return false;
	}

	return true;
}


#define MAX_BUMPS 4
#define RAMP_PIERCE_DISTANCE 1.0f
#define RAMP_BUG_THRESHOLD 0.99f
#define NEW_RAMP_THRESHOLD 0.95f
void TryPlayerMovePre(CCSPlayer_MovementServices *ms, Vector *pFirstDest, trace_t_s2 *pFirstTrace)
{
	CCSPlayerPawn *pawn = ms->GetPawn();
	ZEPlayer *player = g_playerManager->GetPlayer(pawn->m_hController()->GetPlayerSlot());
	player->overrideTPM = false;
	player->didTPM = true;

	f32 timeLeft = gpGlobals->frametime;

	Vector start, velocity, end;
	player->GetOrigin(&start);
	player->GetVelocity(&velocity);

	if (velocity.Length() == 0.0f)
	{
		// No move required.
		return;
	}
	Vector primalVelocity = velocity;
	bool validPlane {};

	f32 allFraction {};
	trace_t_s2 pm;
	u32 bumpCount {};
	Vector planes[5];
	u32 numPlanes {};
	trace_t_s2 pierce;

	bbox_t bounds;
	bounds.mins = {-16, -16, 0};
	bounds.maxs = {16, 16, 72};

	if (ms->m_bDucked())
	{
		bounds.maxs.z = 54;
	}

	CTraceFilterPlayerMovementCS filter;
	addresses::InitPlayerMovementTraceFilter(filter, pawn, pawn->m_Collision().m_collisionAttribute().m_nInteractsWith(),
											  COLLISION_GROUP_PLAYER_MOVEMENT);
	for (bumpCount = 0; bumpCount < MAX_BUMPS; bumpCount++)
	{
		// Assume we can move all the way from the current origin to the end point.
		VectorMA(start, timeLeft, velocity, end);
		// See if we can make it from origin to end point.
		// If their velocity Z is 0, then we can avoid an extra trace here during WalkMove.
		if (pFirstDest && end == *pFirstDest)
		{
			pm = *pFirstTrace;
		}
		else
		{
			addresses::TracePlayerBBox(start, end, bounds, &filter, pm);
			if (end == start)
			{
				continue;
			}
			if (IsValidMovementTrace(pm, bounds, &filter) && pm.fraction == 1.0f)
			{
				// Player won't hit anything, nothing to do.
				break;
			}
			if (player->lastValidPlane.Length() > FLT_EPSILON
				&& (!IsValidMovementTrace(pm, bounds, &filter) || pm.planeNormal.Dot(player->lastValidPlane) < RAMP_BUG_THRESHOLD))
			{
				// We hit a plane that will significantly change our velocity. Make sure that this plane is significant
				// enough.
				Vector direction = velocity.Normalized();
				Vector offsetDirection;
				f32 offsets[] = {0.0f, -1.0f, 1.0f};
				bool success {};
				for (u32 i = 0; i < 3 && !success; i++)
				{
					for (u32 j = 0; j < 3 && !success; j++)
					{
						for (u32 k = 0; k < 3 && !success; k++)
						{
							if (i == 0 && j == 0 && k == 0)
							{
								offsetDirection = player->lastValidPlane;
							}
							else
							{
								offsetDirection = {offsets[i], offsets[j], offsets[k]};
								// Check if this random offset is even valid.
								if (player->lastValidPlane.Dot(offsetDirection) <= 0.0f)
								{
									continue;
								}
								trace_t_s2 test;
								addresses::TracePlayerBBox(start + offsetDirection * RAMP_PIERCE_DISTANCE, start, bounds, &filter, test);
								if (!IsValidMovementTrace(test, bounds, &filter))
								{
									continue;
								}
							}
							bool goodTrace {};
							f32 ratio {};
							bool hitNewPlane {};
							for (ratio = 0.025f; ratio <= 1.0f; ratio += 0.025f)
							{
								addresses::TracePlayerBBox(start + offsetDirection * RAMP_PIERCE_DISTANCE * ratio,
															end + offsetDirection * RAMP_PIERCE_DISTANCE * ratio, bounds, &filter, pierce);
								if (!IsValidMovementTrace(pierce, bounds, &filter))
								{
									continue;
								}
								// Try until we hit a similar plane.
								// clang-format off
								validPlane = pierce.fraction < 1.0f && pierce.fraction > 0.1f 
											 && pierce.planeNormal.Dot(player->lastValidPlane) >= RAMP_BUG_THRESHOLD;

								hitNewPlane = pm.planeNormal.Dot(pierce.planeNormal) < NEW_RAMP_THRESHOLD 
											  && player->lastValidPlane.Dot(pierce.planeNormal) > NEW_RAMP_THRESHOLD;
								// clang-format on
								goodTrace = CloseEnough(pierce.fraction, 1.0f, FLT_EPSILON) || validPlane;
								if (goodTrace)
								{
									break;
								}
							}
							if (goodTrace || hitNewPlane)
							{
								// Trace back to the original end point to find its normal.
								trace_t_s2 test;
								addresses::TracePlayerBBox(pierce.endpos, end, bounds, &filter, test);
								pm = pierce;
								pm.startpos = start;
								pm.fraction = Clamp((pierce.endpos - pierce.startpos).Length() / (end - start).Length(), 0.0f, 1.0f);
								pm.endpos = test.endpos;
								if (pierce.planeNormal.Length() > 0.0f)
								{
									pm.planeNormal = pierce.planeNormal;
									player->lastValidPlane = pierce.planeNormal;
								}
								else
								{
									pm.planeNormal = test.planeNormal;
									player->lastValidPlane = test.planeNormal;
								}
								success = true;
								player->overrideTPM = true;
							}
						}
					}
				}
			}
			if (pm.planeNormal.Length() > 0.99f)
			{
				player->lastValidPlane = pm.planeNormal;
			}
		}

		if (pm.fraction * velocity.Length() > 0.03125f)
		{
			allFraction += pm.fraction;
			start = pm.endpos;
			numPlanes = 0;
		}
		if (allFraction == 1.0f)
		{
			break;
		}
		timeLeft -= gpGlobals->frametime * pm.fraction;
		planes[numPlanes] = pm.planeNormal;
		numPlanes++;
		if (numPlanes == 1 && pawn->m_MoveType() == MOVETYPE_WALK && pawn->m_hGroundEntity().Get() == nullptr)
		{
			ClipVelocity(velocity, planes[0], velocity);
		}
		else
		{
			u32 i, j;
			for (i = 0; i < numPlanes; i++)
			{
				ClipVelocity(velocity, planes[i], velocity);
				for (j = 0; j < numPlanes; j++)
				{
					if (j != i)
					{
						// Are we now moving against this plane?
						if (velocity.Dot(planes[j]) < 0)
						{
							break; // not ok
						}
					}
				}

				if (j == numPlanes) // Didn't have to clip, so we're ok
				{
					break;
				}
			}
			// Did we go all the way through plane set
			if (i != numPlanes)
			{ // go along this plane
				// pmove.velocity is set in clipping call, no need to set again.
				;
			}
			else
			{ // go along the crease
				if (numPlanes != 2)
				{
					VectorCopy(vec3_origin, velocity);
					break;
				}
				Vector dir;
				f32 d;
				CrossProduct(planes[0], planes[1], dir);
				dir.NormalizeInPlace();
				d = dir.Dot(velocity);
				VectorScale(dir, d, velocity);

				if (velocity.Dot(primalVelocity) <= 0)
				{
					velocity = vec3_origin;
					break;
				}
			}
		}
	}
	player->tpmOrigin = pm.endpos;
	player->tpmVelocity = velocity;
}

void TryPlayerMovePost(CCSPlayer_MovementServices *ms, Vector *pFirstDest, trace_t_s2 *pFirstTrace)
{
	ZEPlayer *player = g_playerManager->GetPlayer(ms->GetPawn()->m_hController()->GetPlayerSlot());
	if(!player)
		return;
	if (player->overrideTPM)
	{
		player->SetOrigin(player->tpmOrigin);
		player->SetVelocity(player->tpmVelocity);
	}
}

void FASTCALL Detour_TryPlayerMove(CCSPlayer_MovementServices *ms, CMoveData *mv, Vector *pFirstDest, trace_t_s2 *pFirstTrace)
{
	TryPlayerMovePre(ms, pFirstDest, pFirstTrace);
	TryPlayerMove(ms, mv, pFirstDest, pFirstTrace);
	TryPlayerMovePost(ms, pFirstDest, pFirstTrace);
}

void CategorizePositionPre(CCSPlayer_MovementServices *ms,bool bStayOnGround)
{
	CCSPlayerPawn *pawn = ms->GetPawn();
	ZEPlayer *player = g_playerManager->GetPlayer(pawn->m_hController()->GetPlayerSlot());
	// Already on the ground?
	// If we are already colliding on a standable valid plane, we don't want to do the check.
	if (bStayOnGround || player->lastValidPlane.Length() < 0.000001f|| player->lastValidPlane.z > 0.7f)
	{
		return;
	}
	Vector velocity;
	player->GetVelocity(&velocity);
	//Only attempt to fix rampbugs while going down significantly enough.
	if (velocity.z > -64.0f)
	{
		return;
	}
	bbox_t bounds;
	bounds.mins = {-16, -16, 0};
	bounds.maxs = {16, 16, 72};
                  
	if (ms->m_bDucked)
	{
		bounds.maxs.z = 54;
	}

	CTraceFilterPlayerMovementCS filter;
	addresses::InitPlayerMovementTraceFilter(filter, pawn,
											  pawn->m_Collision().m_collisionAttribute().m_nInteractsWith(),
											  COLLISION_GROUP_PLAYER_MOVEMENT);

	trace_t_s2 trace;
	addresses::InitGameTrace(&trace);

	Vector origin, groundOrigin;
	player->GetOrigin(&origin);
	groundOrigin = origin;
	groundOrigin.z -= 2.0f;

	addresses::TracePlayerBBox(origin, groundOrigin, bounds, &filter, trace);

	if (trace.fraction == 1.0f)
	{
		return;
	}
	// Is this something that you should be able to actually stand on?
	if (trace.fraction < 0.95f && trace.planeNormal.z > 0.7f && player->lastValidPlane.Dot(trace.planeNormal) < RAMP_BUG_THRESHOLD)
	{
		origin += player->lastValidPlane * 0.0625f;
		groundOrigin = origin;
		groundOrigin.z -= 2.0f;
		addresses::TracePlayerBBox(origin, groundOrigin, bounds, &filter, trace);
		if (trace.startsolid)
		{
			return;
		}
		if (trace.fraction == 1.0f || player->lastValidPlane.Dot(trace.planeNormal) >= RAMP_BUG_THRESHOLD)
		{
			player->SetOrigin(origin);
		}
	}
}

void FASTCALL Detour_CategorizePosition(CCSPlayer_MovementServices *ms, CMoveData *mv, bool bStayOnGround)
{
	CategorizePositionPre(ms, bStayOnGround);
	CategorizePosition(ms, mv, bStayOnGround);
}

static bool g_bDisableSubtick = false;
FAKE_BOOL_CVAR(cs2f_disable_subtick_move, "Whether to disable subtick movement", g_bDisableSubtick, false, false)

class CUserCmd
{
public:
	CSGOUserCmdPB cmd;
	[[maybe_unused]] char pad1[0x38];
#ifdef PLATFORM_WINDOWS
	[[maybe_unused]] char pad2[0x8];
#endif
};

void* FASTCALL Detour_ProcessUsercmds(CBasePlayerPawn *pPawn, CUserCmd *cmds, int numcmds, bool paused, float margin)
{
	if (!g_bDisableSubtick)
		return ProcessUsercmds(pPawn, cmds, numcmds, paused, margin);

	static int offset = g_GameConfig->GetOffset("UsercmdOffset");

	for (int i = 0; i < numcmds; i++)
	{
		CSGOUserCmdPB *pUserCmd = &cmds[i].cmd;

		for (int j = 0; j < pUserCmd->mutable_base()->subtick_moves_size(); j++)
			pUserCmd->mutable_base()->mutable_subtick_moves(j)->set_when(0.f);
	}

	return ProcessUsercmds(pPawn, cmds, numcmds, paused, margin);
}

void FASTCALL Detour_CGamePlayerEquip_InputTriggerForAllPlayers(CGamePlayerEquip* pEntity, InputData_t* pInput)
{
    CGamePlayerEquipHandler::TriggerForAllPlayers(pEntity, pInput);
    CGamePlayerEquip_InputTriggerForAllPlayers(pEntity, pInput);
}
void FASTCALL Detour_CGamePlayerEquip_InputTriggerForActivatedPlayer(CGamePlayerEquip* pEntity, InputData_t* pInput)
{
    CGamePlayerEquipHandler::TriggerForActivatedPlayer(pEntity, pInput);
}

int64_t* FASTCALL Detour_CCSGameRules_GoToIntermission(int64_t unk1, char unk2)
{
	if (!g_pMapVoteSystem->IsIntermissionAllowed())
		return nullptr;

	return CCSGameRules_GoToIntermission(unk1, unk2);
}

bool InitDetours(CGameConfig *gameConfig)
{
	bool success = true;

	FOR_EACH_VEC(g_vecDetours, i)
	{
		if (!g_vecDetours[i]->CreateDetour(gameConfig))
			success = false;
		
		g_vecDetours[i]->EnableDetour();
	}

	return success;
}

void FlushAllDetours()
{
	g_vecDetours.Purge();
}
