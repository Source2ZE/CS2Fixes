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

#ifdef _ZOMBIEREBORN

#include "commands.h"
#include "playermanager.h"
#include "ctimer.h"
#include "eventlistener.h"
#include "zombiereborn.h"
#include "entity/cgamerules.h"

#include "tier0/memdbgon.h"

extern CEntitySystem* g_pEntitySystem;
extern IVEngineServer2* g_pEngineServer2;
extern CGlobalVars* gpGlobals;
extern CCSGameRules* g_pGameRules;

EZRRoundState g_ZRRoundState = EZRRoundState::ROUND_START;

void SetUpAllHumanClasses()
{
    for (int i = 0; i < gpGlobals->maxClients; i++)
    {
        ZEPlayer* pPlayer = g_playerManager->GetPlayer(i);

        //if (pPlayer)
        //    InjectPlayerClass(PickRandomHumanDefaultClass(), pPlayer);
    }
}

void ZR_OnStartupServer()
{
    g_ZRRoundState = EZRRoundState::ROUND_START;
    // CONVAR_TODO
    // --Here we force some cvars that are essential for the scripts to work
    g_pEngineServer2->ServerCommand("mp_weapons_allow_pistols 3");
    g_pEngineServer2->ServerCommand("mp_weapons_allow_smgs 3");
    g_pEngineServer2->ServerCommand("mp_weapons_allow_heavy 3");
    g_pEngineServer2->ServerCommand("mp_weapons_allow_rifles 3");
    g_pEngineServer2->ServerCommand("mp_give_player_c4 0");
    g_pEngineServer2->ServerCommand("mp_friendlyfire 0");
    g_pEngineServer2->ServerCommand("mp_respawn_on_death_t 1");
    g_pEngineServer2->ServerCommand("mp_respawn_on_death_ct 3");
    g_pEngineServer2->ServerCommand("bot_quota_mode fill");
    //--Convars : SetInt('mp_ignore_round_win_conditions', 1)
}

void ZR_OnRoundPrestart(IGameEvent* pEvent)
{
    g_ZRRoundState = EZRRoundState::ROUND_START;
}

void ZR_OnRoundStart(IGameEvent* pEvent)
{
    ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX "The game is \x05Humans vs. Zombies\x01, the goal for zombies is to infect all humans by knifing them.");

    new CTimer(1.0f, false, []()
        {
            g_ZRRoundState = EZRRoundState::POST_INFECTION;
            return 1.0f;
        });
   
    // SetUpAllHumanClasses();
    // SetupRespawnToggler();
    // SetupAmmoReplenish();



    // g_ZR_ROUND_STARTED = true;
}

void ZR_OnPlayerSpawn(IGameEvent* pEvent)
{
    CCSPlayerController* pController = (CCSPlayerController*)pEvent->GetPlayerController("userid");

    if (g_ZRRoundState == EZRRoundState::POST_INFECTION && pController->m_iTeamNum == CS_TEAM_CT)
        pController->SwitchTeam(CS_TEAM_T);
}

void ZR_OnPlayerHurt(IGameEvent* pEvent)
{
    // Infection/Knockback to be done in TakeDamageOld detour

    //CCSPlayerController* pAttacker = (CCSPlayerController*)pEvent->GetPlayerController("attacker");
    //CCSPlayerController* pVictim = (CCSPlayerController*)pEvent->GetPlayerController("userid");
    //const char* szWeapon = pEvent->GetString("weapon");
    //int iDmgHealth = pEvent->GetInt("dmg_health");
    //int iHealth = pEvent->GetInt("health");

    //if (szWeapon == "" || !pAttacker || !pVictim)
    //    return;

    //if (pAttacker->m_iTeamNum == CS_TEAM_CT && pVictim->m_iTeamNum == CS_TEAM_T)
    //    Message("lol");
    //    //Knockback_Apply(pAttacker, pVictim, iDmgHealth, szWeapon);
    //else if (szWeapon == "knife" && pAttacker->m_iTeamNum == CS_TEAM_T && pVictim->m_iTeamNum == CS_TEAM_CT)
    //    Message("lol");
    //    //Infect(pAttacker, pVictim, true, iHealth == 0);
}

void ZR_OnPlayerDeath(IGameEvent* pEvent)
{
    // To T TeamSwitch happening in ZR_OnPlayerSpawn

    /*ZEPlayer* pPlayer = g_playerManager->GetPlayerFromUserId(pEvent->GetInt("userid"));

    if (pPlayer && !pPlayer->IsInfected())
    {
        CCSPlayerController* pController = (CCSPlayerController*)pEvent->GetPlayerController("userid");
        pController->SwitchTeam(CS_TEAM_T);
    }*/
}

#endif //_ZOMBIEREBORN