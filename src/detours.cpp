/**
 * =============================================================================
 * CS2Fixes
 * Copyright (C) 2023 Source2ZE
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

#define VPROF_ENABLED
#include "tier0/vprof.h"

#include "tier0/memdbgon.h"

extern CGlobalVars *gpGlobals;
extern CGameEntitySystem *g_pEntitySystem;
extern IGameEventManager2 *g_gameEventManager;
extern CCSGameRules *g_pGameRules;

DECLARE_DETOUR(UTIL_SayTextFilter, Detour_UTIL_SayTextFilter);
DECLARE_DETOUR(UTIL_SayText2Filter, Detour_UTIL_SayText2Filter);
DECLARE_DETOUR(IsHearingClient, Detour_IsHearingClient);
DECLARE_DETOUR(CSoundEmitterSystem_EmitSound, Detour_CSoundEmitterSystem_EmitSound);
DECLARE_DETOUR(TriggerPush_Touch, Detour_TriggerPush_Touch);
DECLARE_DETOUR(CGameRules_Constructor, Detour_CGameRules_Constructor);
DECLARE_DETOUR(CBaseEntity_TakeDamageOld, Detour_CBaseEntity_TakeDamageOld);
DECLARE_DETOUR(CCSPlayer_WeaponServices_CanUse, Detour_CCSPlayer_WeaponServices_CanUse);
DECLARE_DETOUR(CEntityIdentity_AcceptInput, Detour_CEntityIdentity_AcceptInput);
DECLARE_DETOUR(CNavMesh_GetNearestNavArea, Detour_CNavMesh_GetNearestNavArea);
DECLARE_DETOUR(FixLagCompEntityRelationship, Detour_FixLagCompEntityRelationship);

void FASTCALL Detour_CGameRules_Constructor(CGameRules *pThis)
{
	g_pGameRules = (CCSGameRules*)pThis;
	CGameRules_Constructor(pThis);
}

// CONVAR_TODO
static bool g_bBlockMolotoveSelfDmg = false;
static bool g_bBlockAllDamage = false;

CON_COMMAND_F(cs2f_block_molotov_self_dmg, "Whether to block self-damage from molotovs", FCVAR_LINKED_CONCOMMAND | FCVAR_SPONLY)
{
	if (args.ArgC() < 2)
		Msg("%s %i\n", args[0], g_bBlockMolotoveSelfDmg);
	else
		g_bBlockMolotoveSelfDmg = V_StringToBool(args[1], false);
}
CON_COMMAND_F(cs2f_block_all_dmg, "Whether to block all damage to players", FCVAR_LINKED_CONCOMMAND | FCVAR_SPONLY)
{
	if (args.ArgC() < 2)
		Msg("%s %i\n", args[0], g_bBlockAllDamage);
	else
		g_bBlockAllDamage = V_StringToBool(args[1], false);
}

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
	if (g_bBlockMolotoveSelfDmg && inputInfo->m_hAttacker == pThis && !V_strncmp(pszInflictorClass, "inferno", 7))
		return;

	CBaseEntity_TakeDamageOld(pThis, inputInfo);
}

// CONVAR_TODO
static bool g_bUseOldPush = false;

CON_COMMAND_F(cs2f_use_old_push, "Whether to use the old CSGO trigger_push behavior", FCVAR_LINKED_CONCOMMAND | FCVAR_SPONLY)
{
	if (args.ArgC() < 2)
		Msg("%s %i\n", args[0], g_bUseOldPush);
	else
		g_bUseOldPush = V_StringToBool(args[1], false);
}

void FASTCALL Detour_TriggerPush_Touch(CTriggerPush* pPush, Z_CBaseEntity* pOther)
{
	// This trigger pushes only once (and kills itself) or pushes only on StartTouch, both of which are fine already
	if (!g_bUseOldPush || pPush->m_spawnflags() & SF_TRIG_PUSH_ONCE || pPush->m_bTriggerOnStartTouch())
	{
		TriggerPush_Touch(pPush, pOther);
		return;
	}

	MoveType_t movetype = pOther->m_MoveType();

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

	Vector vecPush = vecAbsDir * pPush->m_flPushSpeed();

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

	pOther->m_vecBaseVelocity(vecPush);

	flags |= (FL_BASEVELOCITY);
	pOther->m_fFlags(flags);
}

void FASTCALL Detour_CSoundEmitterSystem_EmitSound(ISoundEmitterSystemBase *pSoundEmitterSystem, CEntityIndex *a2, IRecipientFilter &filter, uint32 a4, void *a5)
{
	//ConMsg("Detour_CSoundEmitterSystem_EmitSound\n");
	CSoundEmitterSystem_EmitSound(pSoundEmitterSystem, a2, filter, a4, a5);
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

// CONVAR_TODO
bool g_bEnableTriggerTimer = false;

CON_COMMAND_F(cs2f_trigger_timer_enable, "Whether to process countdown messages said by Console (e.g. Hold for 10 seconds) and append the round time where the countdown resolves", FCVAR_LINKED_CONCOMMAND | FCVAR_SPONLY)
{
	if (args.ArgC() < 2)
		Msg("%s %i\n", args[0], g_bEnableTriggerTimer);
	else
		g_bEnableTriggerTimer = V_StringToBool(args[1], false);
}

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

void Detour_Log()
{
	return;
}

bool FASTCALL Detour_IsChannelEnabled(LoggingChannelID_t channelID, LoggingSeverity_t severity)
{
	return false;
}

CDetour<decltype(Detour_Log)> g_LoggingDetours[] =
{
	CDetour<decltype(Detour_Log)>( Detour_Log, "Msg" ),
	//CDetour<decltype(Detour_Log)>( Detour_Log, "?ConMsg@@YAXPEBDZZ" ),
	//CDetour<decltype(Detour_Log)>( Detour_Log, "?ConColorMsg@@YAXAEBVColor@@PEBDZZ" ),
	CDetour<decltype(Detour_Log)>( Detour_Log, "ConDMsg" ),
	CDetour<decltype(Detour_Log)>( Detour_Log, "DevMsg" ),
	CDetour<decltype(Detour_Log)>( Detour_Log, "Warning" ),
	CDetour<decltype(Detour_Log)>( Detour_Log, "DevWarning" ),
	//CDetour<decltype(Detour_Log)>( Detour_Log, "?DevWarning@@YAXPEBDZZ" ),
	CDetour<decltype(Detour_Log)>( Detour_Log, "LoggingSystem_Log" ),
	CDetour<decltype(Detour_Log)>( Detour_Log, "LoggingSystem_LogDirect" ),
	CDetour<decltype(Detour_Log)>( Detour_Log, "LoggingSystem_LogAssert" ),
	//CDetour<decltype(Detour_Log)>( Detour_IsChannelEnabled, "LoggingSystem_IsChannelEnabled" ),
};

CON_COMMAND_F(toggle_logs, "Toggle printing most logs and warnings", FCVAR_SPONLY | FCVAR_LINKED_CONCOMMAND)
{
	static bool bBlock = false;

	if (!bBlock)
	{
		Message("Logging is now OFF.\n");

		for (int i = 0; i < sizeof(g_LoggingDetours) / sizeof(*g_LoggingDetours); i++)
			g_LoggingDetours[i].EnableDetour();
	}
	else
	{
		Message("Logging is now ON.\n");

		for (int i = 0; i < sizeof(g_LoggingDetours) / sizeof(*g_LoggingDetours); i++)
			g_LoggingDetours[i].DisableDetour();
	}

	bBlock = !bBlock;
}

bool FASTCALL Detour_CCSPlayer_WeaponServices_CanUse(CCSPlayer_WeaponServices *pWeaponServices, CBasePlayerWeapon* pPlayerWeapon)
{
	if (g_bEnableZR && !ZR_Detour_CCSPlayer_WeaponServices_CanUse(pWeaponServices, pPlayerWeapon))
	{
		return false;
	}

	return CCSPlayer_WeaponServices_CanUse(pWeaponServices, pPlayerWeapon);
}

void FASTCALL Detour_CEntityIdentity_AcceptInput(CEntityIdentity* pThis, CUtlSymbolLarge* pInputName, CEntityInstance* pActivator, CEntityInstance* pCaller, variant_t* value, int nOutputID)
{
	if (g_bEnableZR)
		ZR_Detour_CEntityIdentity_AcceptInput(pThis, pInputName, pActivator, pCaller, value, nOutputID);

	return CEntityIdentity_AcceptInput(pThis, pInputName, pActivator, pCaller, value, nOutputID);
}

// CONVAR_TODO
bool g_bBlockNavLookup = false;

CON_COMMAND_F(cs2f_block_nav_lookup, "Whether to block navigation mesh lookup, improves server performance but breaks bot navigation", FCVAR_LINKED_CONCOMMAND | FCVAR_SPONLY)
{
	if (args.ArgC() < 2)
		Msg("%s %i\n", args[0], g_bBlockNavLookup);
	else
		g_bBlockNavLookup = V_StringToBool(args[1], false);
}

void* FASTCALL Detour_CNavMesh_GetNearestNavArea(int64_t unk1, float* unk2, unsigned int* unk3, unsigned int unk4, int64_t unk5, int64_t unk6, float unk7, int64_t unk8)
{
	if (g_bBlockNavLookup)
		return nullptr;

	return CNavMesh_GetNearestNavArea(unk1, unk2, unk3, unk4, unk5, unk6, unk7, unk8);
}

// CONVAR_TODO
bool g_bFixLagCompCrash = false;

CON_COMMAND_F(cs2f_fix_lag_comp_crash, "Whether to fix lag compensation crash with env_entity_maker", FCVAR_LINKED_CONCOMMAND | FCVAR_SPONLY)
{
	if (args.ArgC() < 2)
		Msg("%s %i\n", args[0], g_bFixLagCompCrash);
	else
		g_bFixLagCompCrash = V_StringToBool(args[1], false);
}

void FASTCALL Detour_FixLagCompEntityRelationship(void *a1, CEntityInstance *pEntity, bool a3)
{
	if (g_bFixLagCompCrash && strcmp(pEntity->GetClassname(), "env_entity_maker") == 0)
		return;

	return FixLagCompEntityRelationship(a1, pEntity, a3);
}

CUtlVector<CDetourBase *> g_vecDetours;

bool InitDetours(CGameConfig *gameConfig)
{
	bool success = true;

	g_vecDetours.PurgeAndDeleteElements();

	for (int i = 0; i < sizeof(g_LoggingDetours) / sizeof(*g_LoggingDetours); i++)
	{
		if (!g_LoggingDetours[i].CreateDetour(gameConfig))
			success = false;
	}

	if (!UTIL_SayTextFilter.CreateDetour(gameConfig))
		success = false;
	UTIL_SayTextFilter.EnableDetour();

	if (!UTIL_SayText2Filter.CreateDetour(gameConfig))
		success = false;
	UTIL_SayText2Filter.EnableDetour();

	if (!IsHearingClient.CreateDetour(gameConfig))
		success = false;
	IsHearingClient.EnableDetour();

	if (!CSoundEmitterSystem_EmitSound.CreateDetour(gameConfig))
		success = false;
	CSoundEmitterSystem_EmitSound.EnableDetour();

	if (!TriggerPush_Touch.CreateDetour(gameConfig))
		success = false;
	TriggerPush_Touch.EnableDetour();

	if (!CGameRules_Constructor.CreateDetour(gameConfig))
		success = false;
	CGameRules_Constructor.EnableDetour();

	if (!CBaseEntity_TakeDamageOld.CreateDetour(gameConfig))
		success = false;
	CBaseEntity_TakeDamageOld.EnableDetour();

	if (!CCSPlayer_WeaponServices_CanUse.CreateDetour(gameConfig))
		success = false;
	CCSPlayer_WeaponServices_CanUse.EnableDetour();
  
	if (!CEntityIdentity_AcceptInput.CreateDetour(gameConfig))
		success = false;
	CEntityIdentity_AcceptInput.EnableDetour();

	if (!CNavMesh_GetNearestNavArea.CreateDetour(gameConfig))
		success = false;
	CNavMesh_GetNearestNavArea.EnableDetour();

	if (!FixLagCompEntityRelationship.CreateDetour(gameConfig))
		success = false;
	FixLagCompEntityRelationship.EnableDetour();

	return success;
}

void FlushAllDetours()
{
	g_vecDetours.Purge();
}
