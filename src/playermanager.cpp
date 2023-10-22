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

#include <../cs2fixes.h>
#include "utlstring.h"
#include "playermanager.h"
#include "adminsystem.h"
#include "entity/ccsplayercontroller.h"
#include "ctime"


extern IVEngineServer2 *g_pEngineServer2;
extern CEntitySystem *g_pEntitySystem;

void ZEPlayer::OnAuthenticated()
{
	CheckAdmin();
	CheckInfractions();
}

void ZEPlayer::CheckInfractions()
{
	g_pAdminSystem->ApplyInfractions(this);
}

void ZEPlayer::CheckAdmin()
{
	if (IsFakeClient())
		return;

	auto admin = g_pAdminSystem->FindAdmin(GetSteamId64());
	if (!admin)
	{
		SetAdminFlags(0);
		return;
	}

	SetAdminFlags(admin->GetFlags());

	Message("%lli authenticated as an admin\n", GetSteamId64());
}

bool ZEPlayer::IsAdminFlagSet(uint64 iFlag)
{
	return m_iAdminFlags & iFlag;
}

void CPlayerManager::OnBotConnected(CPlayerSlot slot)
{
	m_vecPlayers[slot.Get()] = new ZEPlayer(slot, true);
}

bool CPlayerManager::OnClientConnected(CPlayerSlot slot)
{
	Assert(m_vecPlayers[slot.Get()] == nullptr);

	Message("%d connected\n", slot.Get());

	ZEPlayer *pPlayer = new ZEPlayer(slot);

	if (!g_pAdminSystem->ApplyInfractions(pPlayer))
	{
		// Player is banned
		delete pPlayer;
		return false;
	}

	pPlayer->SetConnected();
	m_vecPlayers[slot.Get()] = pPlayer;

	ResetPlayerFlags(slot.Get());
	
	return true;
}

void CPlayerManager::OnClientDisconnect(CPlayerSlot slot)
{
	Message("%d disconnected\n", slot.Get());

	delete m_vecPlayers[slot.Get()];
	m_vecPlayers[slot.Get()] = nullptr;

	ResetPlayerFlags(slot.Get());
}

void CPlayerManager::OnLateLoad()
{
	for (int i = 0; i < GetMaxPlayers(); i++)
	{
		CCSPlayerController *pController = (CCSPlayerController *)g_pEntitySystem->GetBaseEntity(CEntityIndex(i + 1));

		if (!pController || !pController->IsConnected())
			continue;

		OnClientConnected(i);
	}
}

void CPlayerManager::TryAuthenticate()
{
	for (int i = 0; i < sizeof(m_vecPlayers) / sizeof(*m_vecPlayers); i++)
	{
		if (m_vecPlayers[i] == nullptr)
			continue;

		if (m_vecPlayers[i]->IsAuthenticated() || m_vecPlayers[i]->IsFakeClient())
			continue;

		if (g_pEngineServer2->IsClientFullyAuthenticated(i))
		{
			m_vecPlayers[i]->SetAuthenticated();
			m_vecPlayers[i]->SetSteamId(g_pEngineServer2->GetClientSteamID(i));
			Message("%lli authenticated %d\n", m_vecPlayers[i]->GetSteamId()->ConvertToUint64(), i);
			m_vecPlayers[i]->OnAuthenticated();
		}
	}
}

void CPlayerManager::CheckInfractions()
{
	for (int i = 0; i < sizeof(m_vecPlayers) / sizeof(*m_vecPlayers); i++)
	{
		if (m_vecPlayers[i] == nullptr || m_vecPlayers[i]->IsFakeClient())
			continue;

		m_vecPlayers[i]->CheckInfractions();
	}

	g_pAdminSystem->SaveInfractions();
}

void CPlayerManager::CheckHideDistances()
{
	if (!g_pEntitySystem)
		return;

	for (int i = 0; i < GetMaxPlayers(); i++)
	{
		auto player = GetPlayer(i);

		if (!player)
			continue;

		player->ClearTransmit();
		auto hideDistance = player->GetHideDistance();

		if (!hideDistance)
			continue;

		auto pController = (CCSPlayerController *)g_pEntitySystem->GetBaseEntity((CEntityIndex)(i + 1));

		if (!pController)
			continue;

		auto pPawn = pController->GetPawn();

		if (!pPawn || !pPawn->IsAlive())
			continue;

		auto vecPosition = pPawn->GetAbsOrigin();
		int team = pController->m_iTeamNum;

		for (int j = 1; j < GetMaxPlayers() + 1; j++)
		{
			if (j - 1 == i)
				continue;

			auto pTargetController = (CCSPlayerController *)g_pEntitySystem->GetBaseEntity((CEntityIndex)j);

			if (pTargetController)
			{
				auto pTargetPawn = pTargetController->GetPawn();

				if (pTargetPawn && pTargetPawn->IsAlive() && pTargetController->m_iTeamNum == team)
				{
					player->SetTransmit(j - 1, pTargetPawn->GetAbsOrigin().DistToSqr(vecPosition) <= hideDistance * hideDistance);
				}
			}
		}
	}
}

ETargetType CPlayerManager::TargetPlayerString(int iCommandClient, const char* target, int& iNumClients, int *clients)
{
	ETargetType targetType = ETargetType::NONE;
	if (!V_stricmp(target, "@me"))
		targetType = ETargetType::SELF;
	else if (!V_stricmp(target, "@all"))
		targetType = ETargetType::ALL;
	else if (!V_stricmp(target, "@t"))
		targetType = ETargetType::T;
	else if (!V_stricmp(target, "@ct"))
		targetType = ETargetType::CT;
	else if (!V_stricmp(target, "@random"))
		targetType = ETargetType::RANDOM;
	else if (!V_stricmp(target, "@randomt"))
		targetType = ETargetType::RANDOM_T;
	else if (!V_stricmp(target, "@randomct"))
		targetType = ETargetType::RANDOM_CT;
	
	if (targetType == ETargetType::SELF && iCommandClient != -1)
	{
		clients[iNumClients++] = iCommandClient;
	}
	else if (targetType == ETargetType::ALL)
	{
		for (int i = 0; i < sizeof(m_vecPlayers) / sizeof(*m_vecPlayers); i++)
		{
			if (m_vecPlayers[i] == nullptr)
				continue;

			clients[iNumClients++] = i;
		}
	}
	else if (targetType == ETargetType::T || targetType == ETargetType::CT)
	{
		for (int i = 0; i < sizeof(m_vecPlayers) / sizeof(*m_vecPlayers); i++)
		{
			if (m_vecPlayers[i] == nullptr)
				continue;

			CBasePlayerController* player = (CBasePlayerController*)g_pEntitySystem->GetBaseEntity((CEntityIndex)(i + 1));

			if (!player)
				continue;

			if (player->m_iTeamNum() != (targetType == ETargetType::T ? CS_TEAM_T : CS_TEAM_CT))
				continue;

			clients[iNumClients++] = i;
		}
	}
	else if (targetType >= ETargetType::RANDOM)
	{
		int attempts = 0;

		while (iNumClients == 0 && attempts < 10000)
		{
			int slot = rand() % ((sizeof(m_vecPlayers) / sizeof(*m_vecPlayers)) - 1);

			// Prevent infinite loop
			attempts++;

			if (m_vecPlayers[slot] == nullptr)
				continue;

			CBasePlayerController* player = (CBasePlayerController*)g_pEntitySystem->GetBaseEntity((CEntityIndex)(slot + 1));

			if (!player)
				continue;

			if (targetType >= ETargetType::RANDOM_T && (player->m_iTeamNum() != (targetType == ETargetType::RANDOM_T ? CS_TEAM_T : CS_TEAM_CT)))
				continue;

			clients[iNumClients++] = slot;
		}
	}
	else if (*target == '#')
	{
		int userid = V_StringToUint16(target + 1, -1);

		if (userid != -1)
		{
			targetType = ETargetType::PLAYER;
			clients[iNumClients++] = GetSlotFromUserId(userid).Get();
		}
	}
	else
	{
		for (int i = 0; i < sizeof(m_vecPlayers) / sizeof(*m_vecPlayers); i++)
		{
			if (m_vecPlayers[i] == nullptr)
				continue;

			CBasePlayerController* player = (CBasePlayerController*)g_pEntitySystem->GetBaseEntity((CEntityIndex)(i + 1));

			if (!player)
				continue;

			if (V_stristr(player->GetPlayerName(), target))
			{
				targetType = ETargetType::PLAYER;
				clients[iNumClients++] = i;
				break;
			}
		}
	}

	return targetType;
}

// In userids, the lower byte is always the player slot
CPlayerSlot CPlayerManager::GetSlotFromUserId(uint16 userid)
{
	return CPlayerSlot(userid & 0xFF);
}

ZEPlayer *CPlayerManager::GetPlayerFromUserId(uint16 userid)
{
	uint8 index = userid & 0xFF;

	if (index >= GetMaxPlayers())
		return nullptr;

	return m_vecPlayers[index];
}

int CPlayerManager::GetMaxPlayers()
{
	// quick and dirty fix for now until we can get maxplayers properly
	static int iMaxPlayers = CommandLine()->ParmValue("-maxplayers", 64);

	return iMaxPlayers;
}

void CPlayerManager::SetPlayerStopSound(int slot, bool set)
{
	if (set)
		m_nUsingStopSound |= ((uint64)1 << slot);
	else
		m_nUsingStopSound &= ~((uint64)1 << slot);
}

void CPlayerManager::SetPlayerSilenceSound(int slot, bool set)
{
	if (set)
		m_nUsingSilenceSound |= ((uint64)1 << slot);
	else
		m_nUsingSilenceSound &= ~((uint64)1 << slot);
}

void CPlayerManager::SetPlayerStopDecals(int slot, bool set)
{
	if (set)
		m_nUsingStopDecals |= ((uint64)1 << slot);
	else
		m_nUsingStopDecals &= ~((uint64)1 << slot);
}

void CPlayerManager::ResetPlayerFlags(int slot)
{
	SetPlayerStopSound(slot, false);
	SetPlayerSilenceSound(slot, true);
	SetPlayerStopDecals(slot, true);
}
