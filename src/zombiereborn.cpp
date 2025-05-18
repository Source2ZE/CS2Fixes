/**
 * =============================================================================
 * CS2Fixes
 * Copyright (C) 2023-2025 Source2ZE
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
//abcdefggg
#include "usermessages.pb.h"

#include "commands.h"
#include "ctimer.h"
#include "customio.h"
#include "engine/igameeventsystem.h"
#include "entity/cgamerules.h"
#include "entity/cparticlesystem.h"
#include "entity/cteam.h"
#include "entity/services.h"
#include "eventlistener.h"
#include "leader.h"
#include "networksystem/inetworkmessages.h"
#include "playermanager.h"
#include "recipientfilters.h"
#include "serversideclient.h"
#include "tier0/vprof.h"
#include "user_preferences.h"
#include "utils/entity.h"
#include "vendor/nlohmann/json.hpp"
#include "zombiereborn.h"
#include <fstream>
#include <sstream>

#include "tier0/memdbgon.h"

using ordered_json = nlohmann::ordered_json;

extern CGameEntitySystem* g_pEntitySystem;
extern IVEngineServer2* g_pEngineServer2;
extern CGlobalVars* GetGlobals();
extern CCSGameRules* g_pGameRules;
extern IGameEventManager2* g_gameEventManager;
extern IGameEventSystem* g_gameEventSystem;
extern double g_flUniversalTime;


EZRRoundState g_ZRRoundState = EZRRoundState::ROUND_START;
static int g_iInfectionCountDown = 0;
static bool g_bRespawnEnabled = true;
static CHandle<CBaseEntity> g_hRespawnToggler;
static CHandle<CTeam> g_hTeamCT;
static CHandle<CTeam> g_hTeamT;

CZRPlayerClassManager* g_pZRPlayerClassManager = nullptr;
ZRWeaponConfig* g_pZRWeaponConfig = nullptr;
ZRHitgroupConfig* g_pZRHitgroupConfig = nullptr;

CConVar<bool> g_cvarEnableZR("zr_enable", FCVAR_NONE, "Whether to enable ZR features", true);
CConVar<float> g_cvarMaxZteleDistance("zr_ztele_max_distance", FCVAR_NONE, "Maximum distance players are allowed to move after starting ztele", 150.0f, true, 0.0f, false, 0.0f);
CConVar<bool> g_cvarZteleHuman("zr_ztele_allow_humans", FCVAR_NONE, "Whether to allow humans to use ztele", false);
CConVar<int> g_cvarInfectSpawnType("zr_infect_spawn_type", FCVAR_NONE, "Type of Mother Zombies Spawn [0 = MZ spawn where they stand, 1 = MZ get teleported back to spawn on being picked]", (int)EZRSpawnType::RESPAWN, true, 0, true, 1);
CConVar<int> g_cvarInfectSpawnTimeMin("zr_infect_spawn_time_min", FCVAR_NONE, "Minimum time in which Mother Zombies should be picked, after round start", 15, true, 0, false, 0);
CConVar<int> g_cvarInfectSpawnTimeMax("zr_infect_spawn_time_max", FCVAR_NONE, "Maximum time in which Mother Zombies should be picked, after round start", 15, true, 1, false, 0);
CConVar<int> g_cvarInfectSpawnMZRatio("zr_infect_spawn_mz_ratio", FCVAR_NONE, "Ratio of all Players to Mother Zombies to be spawned at round start", 7, true, 1, true, 64);
CConVar<int> g_cvarInfectSpawnMinCount("zr_infect_spawn_mz_min_count", FCVAR_NONE, "Minimum amount of Mother Zombies to be spawned at round start", 1, true, 0, false, 0);
CConVar<bool> g_cvarInfectShake("zr_infect_shake", FCVAR_NONE, "Whether to shake a player's view on infect", true);
CConVar<float> g_cvarInfectShakeAmplitude("zr_infect_shake_amp", FCVAR_NONE, "Amplitude of shaking effect", 15.0f, true, 0.0f, true, 16.0f);
CConVar<float> g_cvarInfectShakeFrequency("zr_infect_shake_frequency", FCVAR_NONE, "Frequency of shaking effect", 2.0f, true, 0.0f, false, 0.0f);
CConVar<float> g_cvarInfectShakeDuration("zr_infect_shake_duration", FCVAR_NONE, "Duration of shaking effect", 5.0f, true, 0.0f, false, 0.0f);

// meant only for offline config validation and can easily cause issues when used on live server
#ifdef _DEBUG
CON_COMMAND_F(zr_reload_classes, "- Reload ZR player classes", FCVAR_SPONLY | FCVAR_LINKED_CONCOMMAND)
{
	g_pZRPlayerClassManager->LoadPlayerClass();

	Message("Reloaded ZR player classes.\n");
}
#endif




void SetupRespawnToggler()
{
	CBaseEntity* relay = CreateEntityByName("logic_relay");
	CEntityKeyValues* pKeyValues = new CEntityKeyValues();

	pKeyValues->SetString("targetname", "zr_toggle_respawn");
	relay->DispatchSpawn(pKeyValues);
	g_hRespawnToggler = relay->GetHandle();
}



void ZR_OnRoundStart(IGameEvent* pEvent)
{
	ClientPrintAll(HUD_PRINTTALK, ZR_PREFIX "ZEROSHE");
	SetupRespawnToggler();

}



void ZR_InfectShake(CCSPlayerController* pController)
{
	if (!pController || !pController->IsConnected() || pController->IsBot() || !g_cvarInfectShake.Get())
		return;

	INetworkMessageInternal* pNetMsg = g_pNetworkMessages->FindNetworkMessagePartial("Shake");

	auto data = pNetMsg->AllocateMessage()->ToPB<CUserMessageShake>();

	data->set_duration(g_cvarInfectShakeDuration.Get());
	data->set_frequency(g_cvarInfectShakeFrequency.Get());
	data->set_amplitude(g_cvarInfectShakeAmplitude.Get());
	data->set_command(0);

	CSingleRecipientFilter filter(pController->GetPlayerSlot());
	g_gameEventSystem->PostEventAbstract(-1, false, &filter, pNetMsg, data, 0);

	delete data;
}










