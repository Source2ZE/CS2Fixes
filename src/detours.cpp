/**
 * =============================================================================
 * CS2Fixes
 * Copyright (C) 2023-2026 Source2ZE
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

#include "cs_usercmd.pb.h"
#include "networkbasetypes.pb.h"
#include "usercmd.pb.h"

#include "addresses.h"
#include "buttonwatch.h"
#include "commands.h"
#include "common.h"
#include "ctimer.h"
#include "customio.h"
#include "detours.h"
#include "entities.h"
#include "entity/cbasemodelentity.h"
#include "entity/ccsplayercontroller.h"
#include "entity/ccsplayerpawn.h"
#include "entity/ccsweaponbase.h"
#include "entity/cenvhudhint.h"
#include "entity/cgamerules.h"
#include "entity/cpointviewcontrol.h"
#include "entity/ctakedamageinfo.h"
#include "entity/ctriggerpush.h"
#include "entity/services.h"
#include "entwatch.h"
#include "gameconfig.h"
#include "igameevents.h"
#include "irecipientfilter.h"
#include "map_votes.h"
#include "mapmigrations.h"
#include "module.h"
#include "networksystem/inetworkserializer.h"
#include "playermanager.h"
#include "serversideclient.h"
#include "tier0/vprof.h"
#include "votemanager.h"
#include "zombiereborn.h"

#include "tier0/memdbgon.h"

KHook::Member<CBaseEntity, int64, CTakeDamageInfo*, CTakeDamageResult*> takeDamageOldHook(Detour_CBaseEntity_TakeDamageOld, Detour_CBaseEntity_TakeDamageOld_Post);
KHook::Member<CTriggerPush, void, CBaseEntity*> triggerPushTouchHook(Detour_TriggerPush_Touch, nullptr);
KHook::Function<bool, void*, int> isHearingClientHook(Detour_IsHearingClient, nullptr);
KHook::Function<void, IRecipientFilter&, const char*, CCSPlayerController*, uint64> sayTextFilterHook(Detour_UTIL_SayTextFilter, nullptr);
KHook::Function<void, IRecipientFilter&, CCSPlayerController*, uint64, const char*, const char*, const char*, const char*, const char*> sayText2FilterHook(Detour_UTIL_SayText2Filter, nullptr);
KHook::Member<CCSPlayer_WeaponServices, bool, CBasePlayerWeapon*> canUseHook(Detour_CCSPlayer_WeaponServices_CanUse, nullptr);
KHook::Member<CCSPlayer_WeaponServices, void, CBasePlayerWeapon*> equipWeaponHook(Detour_CCSPlayer_WeaponServices_EquipWeapon, nullptr);
KHook::Member<CEntityIdentity, bool, CUtlSymbolLarge*, CEntityInstance*, CEntityInstance*, variant_t*, int, void*, void*> acceptInputHook(Detour_CEntityIdentity_AcceptInput, nullptr);
KHook::Member<CNavMesh, void*, float*, unsigned int*, unsigned int, int64_t, float, int64_t> getNearestNavAreaHook(Detour_CNavMesh_GetNearestNavArea, nullptr);
KHook::Member<CCSPlayer_MovementServices, void, void*> processMovementHook(Detour_ProcessMovement, Detour_ProcessMovement_Post);
KHook::Member<CCSPlayerController, void*, CUserCmd*, int, bool, float> processUsercmdsHook(Detour_ProcessUsercmds, nullptr);
KHook::Member<CGamePlayerEquip, void, InputData_t*> inputTriggerForAllPlayersHook(Detour_CGamePlayerEquip_InputTriggerForAllPlayers, nullptr);
KHook::Member<CGamePlayerEquip, void, InputData_t*> inputTriggerForActivatedPlayerHook(Detour_CGamePlayerEquip_InputTriggerForActivatedPlayer, nullptr);
KHook::Member<CTriggerGravity, void, CBaseEntity*> gravityTouchHook(Detour_CTriggerGravity_GravityTouch, nullptr);
KHook::Function<CServerSideClient*, int64_t, const __m128i*, unsigned int, int64_t, char, void*> getFreeClientHook(Detour_GetFreeClient, nullptr);
KHook::Member<CCSPlayerPawn, float> getMaxSpeedHook(Detour_CCSPlayerPawn_GetMaxSpeed, nullptr);
KHook::Member<CCSPlayer_UseServices, int64, float> findUseEntityHook(Detour_FindUseEntity, Detour_FindUseEntity_Post);
KHook::Function<bool, int64*, int*, float*, uint64> traceFuncHook(Detour_TraceFunc, nullptr);
KHook::Function<bool, int64*, int64, int64, int64, CTraceFilter*, int64> traceShapeHook(Detour_TraceShape, nullptr);
KHook::Member<CEntityIOOutput, void, CEntityInstance*, CEntityInstance*, const CVariant*, float, void*, void*> fireOutputInternalHook(Detour_CEntityIOOutput_FireOutputInternal, nullptr);
#ifdef PLATFORM_WINDOWS
KHook::Member<CBasePlayerPawn, Vector*, Vector*> getEyePositionHook(Detour_CBasePlayerPawn_GetEyePosition, nullptr);
KHook::Member<CBasePlayerPawn, QAngle*, QAngle*> getEyeAnglesHook(Detour_CBasePlayerPawn_GetEyeAngles, nullptr);
#else
KHook::Member<CBasePlayerPawn, Vector> getEyePositionHook(Detour_CBasePlayerPawn_GetEyePosition, nullptr);
KHook::Member<CBasePlayerPawn, QAngle> getEyeAnglesHook(Detour_CBasePlayerPawn_GetEyeAngles, nullptr);
#endif
KHook::Member<CBaseFilter, void, InputData_t&> inputTestActivatorHook(Detour_CBaseFilter_InputTestActivator, nullptr);
KHook::Function<void> checkSteamBanHook(nullptr, Detour_GameSystem_Think_CheckSteamBan_Post);
KHook::Member<CCSPlayer_ItemServices, AcquireResult, CEconItemView*, AcquireMethod, uint64_t> canAcquireHook(Detour_CCSPlayer_ItemServices_CanAcquire, nullptr);
KHook::Function<void, uint64_t> scriptSetModelHook(Detour_CS_Script_SetModel, Detour_CS_Script_SetModel_Post);
KHook::Member<CBaseModelEntity, void, const char*> setModelHook(Detour_CBaseModelEntity_SetModel, nullptr);
KHook::Member<CCSGameRules, void, bool> goToIntermissionHook(Detour_CCSGameRules_GoToIntermission, nullptr);

template <typename RETURN, typename... ARGS>
void SetupDetour(CGameConfig* gameConfig, KHook::Function<RETURN, ARGS...>& hook, const char* name)
{
	auto pfnFunc = reinterpret_cast<RETURN (*)(ARGS...)>(gameConfig->ResolveSignature(name));

	if (!pfnFunc)
	{
		g_bRequiredInitLoaded = false;
		return;
	}

	hook.Configure(pfnFunc);
	Message("Detoured %s at 0x%p\n", name, pfnFunc);
}

template <typename CLASS, typename RETURN, typename... ARGS>
void SetupDetour(CGameConfig* gameConfig, KHook::Member<CLASS, RETURN, ARGS...>& hook, const char* name)
{
	void* pfnFunc = gameConfig->ResolveSignature(name);

	if (!pfnFunc)
	{
		g_bRequiredInitLoaded = false;
		return;
	}

	hook.Configure(pfnFunc);
	Message("Detoured %s at 0x%p\n", name, pfnFunc);
}

void InitDetours(CGameConfig* gameConfig)
{
	SetupDetour(gameConfig, takeDamageOldHook, "CBaseEntity_TakeDamageOld");
	SetupDetour(gameConfig, triggerPushTouchHook, "TriggerPush_Touch");
	SetupDetour(gameConfig, isHearingClientHook, "IsHearingClient");
	SetupDetour(gameConfig, sayTextFilterHook, "UTIL_SayTextFilter");
	SetupDetour(gameConfig, sayText2FilterHook, "UTIL_SayText2Filter");
	SetupDetour(gameConfig, canUseHook, "CCSPlayer_WeaponServices_CanUse");
	SetupDetour(gameConfig, equipWeaponHook, "CCSPlayer_WeaponServices_EquipWeapon");
	SetupDetour(gameConfig, acceptInputHook, "CEntityIdentity_AcceptInput");
	SetupDetour(gameConfig, getNearestNavAreaHook, "CNavMesh_GetNearestNavArea");
	SetupDetour(gameConfig, processMovementHook, "ProcessMovement");
	SetupDetour(gameConfig, processUsercmdsHook, "ProcessUsercmds");
	SetupDetour(gameConfig, inputTriggerForAllPlayersHook, "CGamePlayerEquip_InputTriggerForAllPlayers");
	SetupDetour(gameConfig, inputTriggerForActivatedPlayerHook, "CGamePlayerEquip_InputTriggerForActivatedPlayer");
	SetupDetour(gameConfig, gravityTouchHook, "CTriggerGravity_GravityTouch");
	SetupDetour(gameConfig, getFreeClientHook, "GetFreeClient");
#ifdef __linux__
	// Inlined by MSVC as of 2025-07-28 CS2 update
	// TODO: Find some alternative that supports Windows
	SetupDetour(gameConfig, getMaxSpeedHook, "CCSPlayerPawn_GetMaxSpeed");
#endif
	SetupDetour(gameConfig, findUseEntityHook, "FindUseEntity");
	SetupDetour(gameConfig, traceFuncHook, "TraceFunc");
	SetupDetour(gameConfig, traceShapeHook, "TraceShape");
	SetupDetour(gameConfig, fireOutputInternalHook, "CEntityIOOutput_FireOutputInternal");
	SetupDetour(gameConfig, getEyePositionHook, "CBasePlayerPawn_GetEyePosition");
	SetupDetour(gameConfig, getEyeAnglesHook, "CBasePlayerPawn_GetEyeAngles");
	SetupDetour(gameConfig, inputTestActivatorHook, "CBaseFilter_InputTestActivator");
	SetupDetour(gameConfig, checkSteamBanHook, "GameSystem_Think_CheckSteamBan");
	SetupDetour(gameConfig, canAcquireHook, "CCSPlayer_ItemServices_CanAcquire");
	SetupDetour(gameConfig, scriptSetModelHook, "CS_Script_SetModel");
	SetupDetour(gameConfig, setModelHook, "CBaseModelEntity_SetModel");
	SetupDetour(gameConfig, goToIntermissionHook, "CCSGameRules_GoToIntermission");
}

CConVar<bool> g_cvarBlockMolotovSelfDmg("cs2f_block_molotov_self_dmg", FCVAR_NONE, "Whether to block self-damage from molotovs", false);
CConVar<bool> g_cvarBlockAllDamage("cs2f_block_all_dmg", FCVAR_NONE, "Whether to block all damage to players", false);
CConVar<bool> g_cvarFixBlockDamage("cs2f_fix_block_dmg", FCVAR_NONE, "Whether to fix block-damage on players", false);

KHook::Return<int64> Detour_CBaseEntity_TakeDamageOld(CBaseEntity* pThis, CTakeDamageInfo* pInfo, CTakeDamageResult* pResult)
{
	// NOTE valve always return 1 here, since 2025/10/15 update.

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
			pInfo->m_hAttacker.Get() ? pInfo->m_hAttacker.Get()->GetClassname() : "NULL",
			pInfo->m_hInflictor.Get() ? pInfo->m_hInflictor.Get()->GetClassname() : "NULL",
			pInfo->m_hAbility.Get() ? pInfo->m_hAbility.Get()->GetClassname() : "NULL",
			pInfo->m_flDamage,
			pInfo->m_bitsDamageType);
#endif

	// Block all player damage if desired
	if (g_cvarBlockAllDamage.Get() && pThis->IsPawn())
		return {KHook::Action::Supersede, 1};

	CEntityInstance* pInflictor = pInfo->m_hInflictor.Get();
	const char* pszInflictorClass = pInflictor ? pInflictor->GetClassname() : "";

	// After Armory update, activator became attacker on block damage, which broke it..
	if (g_cvarFixBlockDamage.Get() && pInfo->m_AttackerInfo.m_bIsPawn && pInfo->m_bitsDamageType ^ DMG_BULLET && pInfo->m_hAttacker != pThis->GetHandle())
	{
		if (V_strcasecmp(pszInflictorClass, "func_movelinear") == 0
			|| V_strcasecmp(pszInflictorClass, "func_mover") == 0
			|| V_strcasecmp(pszInflictorClass, "func_door") == 0
			|| V_strcasecmp(pszInflictorClass, "func_door_rotating") == 0
			|| V_strcasecmp(pszInflictorClass, "func_rotating") == 0
			|| V_strcasecmp(pszInflictorClass, "point_hurt") == 0)
		{
			pInfo->m_AttackerInfo.m_bIsPawn = false;
			pInfo->m_AttackerInfo.m_bIsWorld = true;
			pInfo->m_hAttacker = pInfo->m_hInflictor;

			pInfo->m_AttackerInfo.m_hAttackerPawn = CHandle<CCSPlayerPawn>(~0u);
			pInfo->m_AttackerInfo.m_nAttackerPlayerSlot = ~0;
		}
	}

	// Prevent molly on self
	if (g_cvarBlockMolotovSelfDmg.Get() && pInfo->m_hAttacker == pThis && !V_strncmp(pszInflictorClass, "inferno", 7))
		return {KHook::Action::Supersede, 1};

	// Fix disconnected players grenades being able to damage teammates
	if (!V_strcasecmp(pszInflictorClass, "hegrenade_projectile") && pInfo->m_AttackerInfo.m_bIsPawn && pInfo->m_AttackerInfo.m_nTeam == 0)
		return {KHook::Action::Supersede, 1};

	// maybe call in flow
	CTakeDamageResult damageResult(0);

	if (pResult == nullptr)
	{
		damageResult.CopyFrom(pInfo);
		return KHook::Recall<int64 (CBaseEntity::*)(CTakeDamageInfo*, CTakeDamageResult*)>(nullptr, {KHook::Action::Ignore}, pThis, pInfo, &damageResult);
	}

	return {KHook::Action::Ignore};
}

KHook::Return<int64> Detour_CBaseEntity_TakeDamageOld_Post(CBaseEntity* pThis, CTakeDamageInfo* pInfo, CTakeDamageResult* pResult)
{
	if (pResult->m_flDamageDealt > 0.0f && !pResult->m_bWasDamageSuppressed && g_cvarEnableZR.Get() && pThis->IsPawn())
		ZR_OnPlayerTakeDamage(reinterpret_cast<CCSPlayerPawn*>(pThis), pInfo, pResult->m_flDamageDealt);

	return {KHook::Action::Ignore};
}

CConVar<bool> g_cvarUseOldPush("cs2f_use_old_push", FCVAR_NONE, "Whether to use the old CSGO trigger_push behavior", false);
CConVar<bool> g_cvarLogPushes("cs2f_log_pushes", FCVAR_NONE, "Whether to log pushes (cs2f_use_old_push must be enabled)", false);

KHook::Return<void> Detour_TriggerPush_Touch(CTriggerPush* pPush, CBaseEntity* pOther)
{
	// This trigger pushes only once (and kills itself) or pushes only on StartTouch, both of which are fine already
	if (!g_cvarUseOldPush.Get() || pPush->m_spawnflags() & SF_TRIG_PUSH_ONCE || pPush->m_bTriggerOnStartTouch())
		return {KHook::Action::Ignore};

	MoveType_t movetype = pOther->m_nActualMoveType();

	// VPhysics handling doesn't need any changes
	if (movetype == MOVETYPE_VPHYSICS)
		return {KHook::Action::Ignore};

	if (movetype == MOVETYPE_NONE || movetype == MOVETYPE_PUSH || movetype == MOVETYPE_NOCLIP)
		return {KHook::Action::Supersede};

	CCollisionProperty* collisionProp = pOther->m_pCollision();
	if (!IsSolid(collisionProp->m_nSolidType(), collisionProp->m_usSolidFlags()))
		return {KHook::Action::Supersede};

	if (!pPush->PassesTriggerFilters(pOther))
		return {KHook::Action::Supersede};

	if (pOther->m_CBodyComponent()->m_pSceneNode()->m_pParent())
		return {KHook::Action::Supersede};

	Vector vecAbsDir;
	matrix3x4_t matTransform = pPush->m_CBodyComponent()->m_pSceneNode()->EntityToWorldTransform();

	Vector vecPushDir = pPush->m_vecPushDirEntitySpace();
	VectorRotate(vecPushDir, matTransform, vecAbsDir);

	Vector vecPush = vecAbsDir * pPush->m_flSpeed();

	uint32 flags = pOther->m_fFlags();

	if (flags & (1 << 23)) // TODO: is FL_BASEVELOCITY really gone?
		vecPush = vecPush + pOther->m_vecBaseVelocity();

	if (vecPush.z > 0 && (flags & FL_ONGROUND))
	{
		pOther->SetGroundEntity(nullptr);
		Vector origin = pOther->GetAbsOrigin();
		origin.z += 1.0f;

		pOther->Teleport(&origin, nullptr, nullptr);
	}

	if (g_cvarLogPushes.Get() && GetGlobals())
	{
		Vector vecEntBaseVelocity = pOther->m_vecBaseVelocity;
		Vector vecOrigPush = vecAbsDir * pPush->m_flSpeed();

		Message("Pushing entity %i | frame = %i | tick = %i | entity basevelocity %s = %.2f %.2f %.2f | original push velocity = %.2f %.2f %.2f | final push velocity = %.2f %.2f %.2f\n",
				pOther->GetEntityIndex(),
				GetGlobals()->framecount,
				GetGlobals()->tickcount,
				(flags & (1 << 23)) ? "WITH FLAG" : "",
				vecEntBaseVelocity.x, vecEntBaseVelocity.y, vecEntBaseVelocity.z,
				vecOrigPush.x, vecOrigPush.y, vecOrigPush.z,
				vecPush.x, vecPush.y, vecPush.z);
	}

	pOther->m_vecBaseVelocity(vecPush);

	flags |= (1 << 23); // TODO: is FL_BASEVELOCITY really gone?
	pOther->m_fFlags(flags);

	return {KHook::Action::Supersede};
}

KHook::Return<bool> Detour_IsHearingClient(void* serverClient, int index)
{
	ZEPlayer* player = g_playerManager->GetPlayer(index);
	if (player && player->IsMuted())
		return {KHook::Action::Supersede, false};

	return {KHook::Action::Ignore};
}

KHook::Return<void> SayChatMessageWithTimer(IRecipientFilter& filter, const char* pText, CCSPlayerController* pPlayer, uint64 eMessageType)
{
	VPROF("SayChatMessageWithTimer");

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
	CSplitString words(filteredText, " ");

	// Word count includes the first word "Console:" at index 0, first relevant word is at index 1
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

		// Case: ... X sec(onds) ... or ... X s ... or ... X min(utes) ...
		if (pNextWord != NULL && uiCurrentValue > 0)
		{
			if (uiNextWordLength == 1)
			{
				if (pNextWord[0] == 's')
					uiTriggerTimerLength = uiCurrentValue;
			}
			else if (uiNextWordLength > 2)
			{
				if (pNextWord[0] == 's' && pNextWord[1] == 'e' && pNextWord[2] == 'c')
					uiTriggerTimerLength = uiCurrentValue;
				if (pNextWord[0] == 'm' && pNextWord[1] == 'i' && pNextWord[2] == 'n')
					uiTriggerTimerLength = uiCurrentValue * 60;
			}
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

	float fCurrentRoundClock = g_pGameRules->m_iRoundTime - (GetGlobals()->curtime - g_pGameRules->m_fRoundStartTime.Get().GetTime());

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

	return KHook::Recall<void (*)(IRecipientFilter&, const char*, CCSPlayerController*, uint64)>(nullptr, {KHook::Action::Ignore}, filter, buf, pPlayer, eMessageType);
}

CConVar<bool> g_cvarEnableTriggerTimer("cs2f_trigger_timer_enable", FCVAR_NONE, "Whether to process countdown messages said by Console (e.g. Hold for 10 seconds) and append the round time where the countdown resolves", false);

KHook::Return<void> Detour_UTIL_SayTextFilter(IRecipientFilter& filter, const char* pText, CCSPlayerController* pPlayer, uint64 eMessageType)
{
	if (pPlayer)
		return {KHook::Action::Ignore};

	if (g_cvarEnableTriggerTimer.Get() && GetGlobals() && g_pGameRules)
		return SayChatMessageWithTimer(filter, pText, pPlayer, eMessageType);

	char buf[256];
	V_snprintf(buf, sizeof(buf), "%s %s", " \7CONSOLE:\4", pText + sizeof("Console:"));

	return KHook::Recall<void (*)(IRecipientFilter&, const char*, CCSPlayerController*, uint64)>(nullptr, {KHook::Action::Ignore}, filter, buf, pPlayer, eMessageType);
}

KHook::Return<void> Detour_UTIL_SayText2Filter(
	IRecipientFilter& filter,
	CCSPlayerController* pEntity,
	uint64 eMessageType,
	const char* msg_name,
	const char* param1,
	const char* param2,
	const char* param3,
	const char* param4)
{
#ifdef _DEBUG
	CPlayerSlot slot = filter.GetRecipientIndex(0);
	CCSPlayerController* target = CCSPlayerController::FromSlot(slot);

	if (target)
		Message("Chat from %s to %s: %s\n", param1, target->GetPlayerName().c_str(), param2);
#endif

	return KHook::Recall<void (*)(IRecipientFilter&, CCSPlayerController*, uint64, const char*, const char*, const char*, const char*, const char*)>(nullptr, {KHook::Action::Ignore}, filter, pEntity, eMessageType, msg_name, pEntity->GetPlayerName().c_str(), param2, param3, param4);
}

KHook::Return<bool> Detour_CCSPlayer_WeaponServices_CanUse(CCSPlayer_WeaponServices* pWeaponServices, CBasePlayerWeapon* pPlayerWeapon)
{
	if (g_cvarEnableEntWatch.Get() && !EW_Detour_CCSPlayer_WeaponServices_CanUse(pWeaponServices, pPlayerWeapon))
		return {KHook::Action::Supersede, false};

	return {KHook::Action::Ignore};
}

KHook::Return<void> Detour_CCSPlayer_WeaponServices_EquipWeapon(CCSPlayer_WeaponServices* pWeaponServices, CBasePlayerWeapon* pPlayerWeapon)
{
	if (g_cvarEnableEntWatch.Get())
		EW_Detour_CCSPlayer_WeaponServices_EquipWeapon(pWeaponServices, pPlayerWeapon);

	g_pMapMigrations->OnEquipWeapon(pPlayerWeapon);

	return {KHook::Action::Ignore};
}

CConVar<bool> g_cvarDisableSetModel("cs2f_disable_setmodel", FCVAR_NONE, "Whether to disable SetModel usage from maps (custom input, cs_script function)", false);

bool PrepareMapSetModel(CBaseModelEntity* pModel)
{
	if (!pModel->IsPawn())
		return true;

	if (g_cvarDisableSetModel.Get() || g_pMapMigrations->Migrations20260420Enabled())
		return false;

	// Player color may have been changed by zclass/server customization, so reset it first
	// This also means if maps want to change player color, it needs to be done after the SetModel call
	int originalAlpha = pModel->m_clrRender().a();
	pModel->m_clrRender = Color(255, 255, 255, originalAlpha);

	return true;
}

KHook::Return<bool> Detour_CEntityIdentity_AcceptInput(CEntityIdentity* pThis, CUtlSymbolLarge* pInputName, CEntityInstance* pActivator, CEntityInstance* pCaller, variant_t* value, int nOutputID, void* a7, void* a8)
{
	VPROF_SCOPE_BEGIN("Detour_CEntityIdentity_AcceptInput");

	if (g_cvarEnableZR.Get())
	{
		bool result = ZR_Detour_CEntityIdentity_AcceptInput(pThis, pInputName, pActivator, pCaller, value, nOutputID);

		if (!result)
			return {KHook::Action::Supersede, result};
	}

	// Handle KeyValue(s)
	if (!V_strnicmp(pInputName->String(), "KeyValue", 8))
	{
		if ((value->m_type == FIELD_CSTRING || value->m_type == FIELD_STRING) && value->m_pszString)
		{
			// always const char*, even if it's FIELD_STRING (that is bug string from lua 'EntFire')
			return {KHook::Action::Supersede, CustomIO_HandleInput(pThis->m_pInstance, value->m_pszString, pActivator, pCaller)};
		}
		Message("Invalid value type for input %s\n", pInputName->String());
		return {KHook::Action::Supersede, false};
	}

	if (!V_strnicmp(pInputName->String(), "IgniteL", 7)) // Override IgniteLifetime
	{
		float flDuration = 0.f;

		if ((value->m_type == FIELD_CSTRING || value->m_type == FIELD_STRING) && value->m_pszString)
			flDuration = V_StringToFloat32(value->m_pszString, 0.f);
		else
			flDuration = value->m_float32;

		CCSPlayerPawn* pPawn = reinterpret_cast<CCSPlayerPawn*>(pThis->m_pInstance);

		if (pPawn->IsPawn() && IgnitePawn(pPawn, flDuration, pPawn, pPawn))
			return {KHook::Action::Supersede, true};
	}
	else if (!V_strnicmp(pInputName->String(), "AddScore", 8))
	{
		int iScore = 0;

		if ((value->m_type == FIELD_CSTRING || value->m_type == FIELD_STRING) && value->m_pszString)
			iScore = V_StringToInt32(value->m_pszString, 0);
		else
			iScore = value->m_int32;

		CCSPlayerPawn* pPawn = reinterpret_cast<CCSPlayerPawn*>(pThis->m_pInstance);

		if (pPawn->IsPawn() && pPawn->GetOriginalController())
		{
			pPawn->GetOriginalController()->AddScore(iScore);
			return {KHook::Action::Supersede, true};
		}
	}
	else if (!V_strcasecmp(pInputName->String(), "SetMessage"))
	{
		if (const auto pHudHint = reinterpret_cast<CBaseEntity*>(pThis->m_pInstance)->AsHudHint())
		{
			if ((value->m_type == FIELD_CSTRING || value->m_type == FIELD_STRING) && value->m_pszString)
				pHudHint->m_iszMessage(GameEntitySystem()->AllocPooledString(value->m_pszString));
			return {KHook::Action::Supersede, true};
		}
	}
	else if (!V_strcasecmp(pInputName->String(), "SetModel"))
	{
		if (const auto pModelEntity = reinterpret_cast<CBaseEntity*>(pThis->m_pInstance)->AsBaseModelEntity())
		{
			if ((value->m_type == FIELD_CSTRING || value->m_type == FIELD_STRING) && value->m_pszString && PrepareMapSetModel(pModelEntity))
				pModelEntity->SetModel(value->m_pszString);

			return {KHook::Action::Supersede, true};
		}
	}
	else if (const auto pGameUI = reinterpret_cast<CBaseEntity*>(pThis->m_pInstance)->AsGameUI())
	{
		if (!V_strcasecmp(pInputName->String(), "Activate"))
			return {KHook::Action::Supersede, CGameUIHandler::OnActivate(pGameUI, reinterpret_cast<CBaseEntity*>(pActivator))};
		if (!V_strcasecmp(pInputName->String(), "Deactivate"))
			return {KHook::Action::Supersede, CGameUIHandler::OnDeactivate(pGameUI, reinterpret_cast<CBaseEntity*>(pActivator))};
	}
	else if (const auto pViewControl = reinterpret_cast<CPointViewControl*>(pThis->m_pInstance)->AsPointViewControl())
	{
		if (!V_strcasecmp(pInputName->String(), "EnableCamera"))
			return {KHook::Action::Supersede, CPointViewControlHandler::OnEnable(pViewControl, reinterpret_cast<CBaseEntity*>(pActivator))};
		if (!V_strcasecmp(pInputName->String(), "DisableCamera"))
			return {KHook::Action::Supersede, CPointViewControlHandler::OnDisable(pViewControl, reinterpret_cast<CBaseEntity*>(pActivator))};
		if (!V_strcasecmp(pInputName->String(), "EnableCameraAll"))
			return {KHook::Action::Supersede, CPointViewControlHandler::OnEnableAll(pViewControl)};
		if (!V_strcasecmp(pInputName->String(), "DisableCameraAll"))
			return {KHook::Action::Supersede, CPointViewControlHandler::OnDisableAll(pViewControl)};
	}

	VPROF_SCOPE_END();

	return {KHook::Action::Ignore};
}

CConVar<bool> g_cvarBlockNavLookup("cs2f_block_nav_lookup", FCVAR_NONE, "Whether to block navigation mesh lookup, improves server performance but breaks bot navigation", false);

KHook::Return<void*> Detour_CNavMesh_GetNearestNavArea(CNavMesh* pNavMesh, float* unk2, unsigned int* unk3, unsigned int unk4, int64_t unk5, float unk6, int64_t unk7)
{
	if (g_cvarBlockNavLookup.Get())
		return {KHook::Action::Supersede, nullptr};

	return {KHook::Action::Ignore};
}

float g_flStoreFrametime = 0.0f;

KHook::Return<void> Detour_ProcessMovement(CCSPlayer_MovementServices* pThis, void* pMove)
{
	CCSPlayerPawn* pPawn = pThis->GetPawn();

	if (!pPawn->IsAlive() || !GetGlobals())
		return {KHook::Action::Ignore};

	CCSPlayerController* pController = pPawn->GetOriginalController();

	if (!pController || !pController->IsConnected())
		return {KHook::Action::Ignore};

	float flSpeedMod = pController->GetZEPlayer()->GetSpeedMod();

	if (flSpeedMod == 1.f)
		return {KHook::Action::Ignore};

	// Yes, this is what source1 does to scale player speed
	// Scale frametime during the entire movement processing step and revert right after
	g_flStoreFrametime = GetGlobals()->frametime;
	GetGlobals()->frametime *= flSpeedMod;

	return {KHook::Action::Ignore};
}

KHook::Return<void> Detour_ProcessMovement_Post(CCSPlayer_MovementServices* pThis, void* pMove)
{
	GetGlobals()->frametime = g_flStoreFrametime;
	return {KHook::Action::Ignore};
}

CConVar<bool> g_cvarDisableSubtickMovement("cs2f_disable_subtick_move", FCVAR_NONE, "Whether to disable subtick movement", false);
CConVar<bool> g_cvarDisableSubtickShooting("cs2f_disable_subtick_shooting", FCVAR_NONE, "Whether to disable subtick shooting, experimental (WARNING: add \"log_flags Shooting + DoNotEcho\" to your cfg to prevent console spam on every shot fired)", false);

class CUserCmd
{
public:
	[[maybe_unused]] char pad0[0x10];
	CSGOUserCmdPB cmd;
	[[maybe_unused]] char pad1[0x38];
#ifdef PLATFORM_WINDOWS
	[[maybe_unused]] char pad2[0x8];
#endif
};

KHook::Return<void*> Detour_ProcessUsercmds(CCSPlayerController* pController, CUserCmd* cmds, int numcmds, bool paused, float margin)
{
	VPROF_SCOPE_BEGIN("Detour_ProcessUsercmds");

	for (int i = 0; i < numcmds; i++)
	{
		// Push fix only works properly if subtick movement is also disabled
		if (g_cvarDisableSubtickMovement.Get() || g_cvarUseOldPush.Get())
		{
			auto subtickMoves = cmds[i].cmd.mutable_base()->mutable_subtick_moves();
			auto iterator = subtickMoves->begin();

			while (iterator != subtickMoves->end())
			{
				uint64 button = iterator->button();

				// Remove normal subtick movement inputs by button
				// Unfortunately, we also need to ignore IN_JUMP, because de-subticking jumps somehow conflicts with other subtick inputs pressed at the same time
				if (button >= IN_DUCK && button <= IN_MOVERIGHT && button != IN_USE)
				{
					subtickMoves->erase(iterator);
				}
				else
				{
					// Remove subtick movement viewangles by pitch/yaw
					if (iterator->pitch_delta() != 0.0f)
						iterator->set_pitch_delta(0.0f);

					if (iterator->yaw_delta() != 0.0f)
						iterator->set_yaw_delta(0.0f);

					iterator++;
				}
			}
		}

		if (g_cvarDisableSubtickShooting.Get())
		{
			cmds[i].cmd.set_attack1_start_history_index(-1);
			cmds[i].cmd.set_attack2_start_history_index(-1);
			cmds[i].cmd.mutable_input_history()->Clear();
		}
	}

	VPROF_SCOPE_END();

	return {KHook::Action::Ignore};
}

KHook::Return<void> Detour_CGamePlayerEquip_InputTriggerForAllPlayers(CGamePlayerEquip* pEntity, InputData_t* pInput)
{
	CGamePlayerEquipHandler::TriggerForAllPlayers(pEntity, pInput);
	return {KHook::Action::Ignore};
}
KHook::Return<void> Detour_CGamePlayerEquip_InputTriggerForActivatedPlayer(CGamePlayerEquip* pEntity, InputData_t* pInput)
{
	if (CGamePlayerEquipHandler::TriggerForActivatedPlayer(pEntity, pInput))
		return {KHook::Action::Ignore};

	return {KHook::Action::Supersede};
}

KHook::Return<void> Detour_CTriggerGravity_GravityTouch(CTriggerGravity* pEntity, CBaseEntity* pOther)
{
	// no need to call original function here
	// because original function calls CBaseEntity::SetGravityScale internal
	// but passes the wrong gravity scale value
	if (CTriggerGravityHandler::GravityTouching(pEntity, pOther))
		return {KHook::Action::Supersede};

	return {KHook::Action::Ignore};
}

KHook::Return<CServerSideClient*> Detour_GetFreeClient(int64_t unk1, const __m128i* unk2, unsigned int unk3, int64_t unk4, char unk5, void* unk6)
{
	// Not sure if this function can even be called in this state, but if it is, we can't do shit anyways
	if (!GetClientList() || !GetGlobals())
		return {KHook::Action::Supersede, nullptr};

	// Check if there is still unused slots, this should never break so just fall back to original behaviour for ease (we don't have a CServerSideClient constructor)
	if (GetGlobals()->maxClients != GetClientList()->Count())
		return {KHook::Action::Ignore};

	// Phantom client fix
	for (int i = 0; i < GetClientList()->Count(); i++)
	{
		CServerSideClient* pClient = (*GetClientList())[i];

		if (pClient && pClient->GetSignonState() < SIGNONSTATE_CONNECTED)
			return {KHook::Action::Supersede, pClient};
	}

	// Server is actually full for real
	return {KHook::Action::Supersede, nullptr};
}

KHook::Return<float> Detour_CCSPlayerPawn_GetMaxSpeed(CCSPlayerPawn* pPawn)
{
	auto flMaxSpeed = getMaxSpeedHook.CallOriginal(pPawn);

	const auto pController = reinterpret_cast<CCSPlayerController*>(pPawn->GetController());
	if (const auto pPlayer = pController != nullptr ? pController->GetZEPlayer() : nullptr)
		flMaxSpeed *= pPlayer->GetMaxSpeed();

	return {KHook::Action::Supersede, flMaxSpeed};
}

CConVar<bool> g_cvarPreventUsingPlayers("cs2f_prevent_using_players", FCVAR_NONE, "Whether to prevent +use from hitting players (0=can use players, 1=cannot use players)", false);
bool g_bFindingUseEntity = false;

KHook::Return<int64> Detour_FindUseEntity(CCSPlayer_UseServices* pThis, float a2)
{
	g_bFindingUseEntity = true;
	return {KHook::Action::Ignore};
}

KHook::Return<int64> Detour_FindUseEntity_Post(CCSPlayer_UseServices* pThis, float a2)
{
	g_bFindingUseEntity = false;
	return {KHook::Action::Ignore};
}

KHook::Return<bool> Detour_TraceFunc(int64* a1, int* a2, float* a3, uint64 traceMask)
{
	if (g_cvarPreventUsingPlayers.Get() && g_bFindingUseEntity)
	{
		uint64 newMask = traceMask & (~(CONTENTS_PLAYER & CONTENTS_NPC));
		KHook::Recall<void (*)(int64*, int*, float*, uint64)>(nullptr, {KHook::Action::Ignore}, a1, a2, a3, newMask);
	}

	return {KHook::Action::Ignore};
}

KHook::Return<bool> Detour_TraceShape(int64* a1, int64 a2, int64 a3, int64 a4, CTraceFilter* filter, int64 a6)
{
	if (g_cvarPreventUsingPlayers.Get() && g_bFindingUseEntity)
	{
		filter->DisableInteractsWithLayer(LAYER_INDEX_CONTENTS_PLAYER);
		filter->DisableInteractsWithLayer(LAYER_INDEX_CONTENTS_NPC);
	}

	return {KHook::Action::Ignore};
}

KHook::Return<void> Detour_CEntityIOOutput_FireOutputInternal(const CEntityIOOutput* pThis, CEntityInstance* pActivator, CEntityInstance* pCaller, const CVariant* value, float flDelay, void* a6, void* a7)
{
	if (g_cvarEnableButtonWatch.Get())
		ButtonWatch(pThis, pActivator, pCaller, value, flDelay);

	if (g_cvarEnableEntWatch.Get())
		EW_FireOutput(pThis, pActivator, pCaller, value, flDelay);

	return {KHook::Action::Ignore};
}

#ifdef PLATFORM_WINDOWS
KHook::Return<Vector*> Detour_CBasePlayerPawn_GetEyePosition(CBasePlayerPawn* pPawn, Vector* pRet)
{
	if (pPawn->IsAlive() && CPointViewControlHandler::IsViewControl(reinterpret_cast<CCSPlayerPawn*>(pPawn)))
	{
		const auto& origin = pPawn->GetEyePosition();
		pRet->Init(origin.x, origin.y, origin.z);
		return {KHook::Action::Supersede, pRet};
	}

	return {KHook::Action::Ignore};
}
KHook::Return<QAngle*> Detour_CBasePlayerPawn_GetEyeAngles(CBasePlayerPawn* pPawn, QAngle* pRet)
{
	if (pPawn->IsAlive() && CPointViewControlHandler::IsViewControl(reinterpret_cast<CCSPlayerPawn*>(pPawn)))
	{
		const auto& angles = pPawn->v_angle();
		pRet->Init(angles.x, angles.y, angles.z);
		return {KHook::Action::Supersede, pRet};
	}

	return {KHook::Action::Ignore};
}
#else
KHook::Return<Vector> Detour_CBasePlayerPawn_GetEyePosition(CBasePlayerPawn* pPawn)
{
	if (pPawn->IsAlive() && CPointViewControlHandler::IsViewControl(reinterpret_cast<CCSPlayerPawn*>(pPawn)))
	{
		const auto& origin = pPawn->GetEyePosition();
		return {KHook::Action::Supersede, origin};
	}

	return {KHook::Action::Ignore};
}
KHook::Return<QAngle> Detour_CBasePlayerPawn_GetEyeAngles(CBasePlayerPawn* pPawn)
{
	if (pPawn->IsAlive() && CPointViewControlHandler::IsViewControl(reinterpret_cast<CCSPlayerPawn*>(pPawn)))
	{
		const auto& angles = pPawn->v_angle();
		return {KHook::Action::Supersede, angles};
	}

	return {KHook::Action::Ignore};
}
#endif

KHook::Return<void> Detour_CBaseFilter_InputTestActivator(CBaseFilter* pThis, InputData_t& inputdata)
{
	// If null activator (player disconnected & pawn removed), block the real function from executing and crashing the server
	if (!inputdata.pActivator)
		return {KHook::Action::Supersede};

	return {KHook::Action::Ignore};
}

CConVar<bool> g_cvarFixGameBans("cs2f_fix_game_bans", FCVAR_NONE, "Whether to fix CS2 game bans spreading to all new joining players", false);

KHook::Return<void> Detour_GameSystem_Think_CheckSteamBan_Post()
{
	// Implementation shared by @aiolos1045
	if (!g_cvarFixGameBans.Get())
		return {KHook::Action::Ignore};

	auto pMap = addresses::sm_mapGcBanInformation;

	// After player has been kicked, remove any ban entries, to prevent spreading to all new joining players
	if (pMap->Count() > 0)
		pMap->RemoveAll();

	return {KHook::Action::Ignore};
}

KHook::Return<AcquireResult> Detour_CCSPlayer_ItemServices_CanAcquire(CCSPlayer_ItemServices* pItemServices, CEconItemView* pEconItem, AcquireMethod iAcquireMethod, uint64_t unk4)
{
	if (g_cvarEnableZR.Get())
	{
		AcquireResult zrResult = ZR_Detour_CCSPlayer_ItemServices_CanAcquire(pItemServices, pEconItem);

		if (zrResult != AcquireResult::Allowed)
			return {KHook::Action::Supersede, zrResult};
	}

	return {KHook::Action::Ignore};
}

bool g_bInScriptSetModel = false;

KHook::Return<void> Detour_CS_Script_SetModel(uint64_t unk1)
{
	g_bInScriptSetModel = true;
	return {KHook::Action::Ignore};
}

KHook::Return<void> Detour_CS_Script_SetModel_Post(uint64_t unk1)
{
	g_bInScriptSetModel = false;
	return {KHook::Action::Ignore};
}

KHook::Return<void> Detour_CBaseModelEntity_SetModel(CBaseModelEntity* pModel, const char* pszModel)
{
	if (!g_bInScriptSetModel)
		return {KHook::Action::Ignore};

	if (PrepareMapSetModel(pModel))
		return {KHook::Action::Ignore};

	return {KHook::Action::Supersede};
}

KHook::Return<void> Detour_CCSGameRules_GoToIntermission(CCSGameRules* pThis, bool bAbortedMatch)
{
	if (!g_pMapVoteSystem->IsIntermissionAllowed(false) && g_cvarVoteManagerEnable.Get())
		return {KHook::Action::Supersede};

	if (g_cvarVoteManagerEnable.Get())
		g_pVoteManager->OnIntermission();

	return {KHook::Action::Ignore};
}
