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

void CPlayerManager::OnClientConnected(CPlayerSlot slot)
{
	Assert(m_vecPlayers[slot.Get()] == nullptr);

	Message("%d connected\n", slot.Get());
	m_vecPlayers[slot.Get()] = new ZEPlayer(slot);
}

void CPlayerManager::OnClientDisconnect(CPlayerSlot slot)
{
	Message("%d disconnected\n", slot.Get());
	m_vecPlayers[slot.Get()] = nullptr;
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
	
	if (targetType == ETargetType::SELF)
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
			}
		}
	}

	return targetType;
}