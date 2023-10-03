#include "playermanager.h"
#include "entity/ccsplayercontroller.h"

void CPlayerManager::OnBotConnected(CPlayerSlot slot)
{
	m_vecPlayers[slot.Get()] = new ZEPlayer(true);
}

void CPlayerManager::OnClientConnected(CPlayerSlot slot)
{
	Assert(m_vecPlayers[slot.Get()] == nullptr);

	Message("%d connected\n", slot.Get());
	m_vecPlayers[slot.Get()] = new ZEPlayer();
}

void CPlayerManager::OnClientDisconnect(CPlayerSlot slot)
{
	Message("%d disconnected\n", slot.Get());
	m_vecPlayers[slot.Get()] = nullptr;
}

void CPlayerManager::TryAuthenticate()
{
	for (size_t i = 0; i < sizeof(m_vecPlayers) / sizeof(*m_vecPlayers); i++)
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
		}
	}
}


ETargetType CPlayerManager::TargetPlayerString(const char* target, int& iNumClients, CPlayerSlot* clients)
{
	ETargetType targetType = ETargetType::NONE;
	if (!V_stricmp(target, "@all"))
		targetType = ETargetType::ALL;
	else if (!V_stricmp(target, "@t"))
		targetType = ETargetType::T;
	else if (!V_stricmp(target, "@ct"))
		targetType = ETargetType::CT;
	
	if (targetType == ETargetType::ALL)
	{
		for (size_t i = 0; i < sizeof(m_vecPlayers) / sizeof(*m_vecPlayers); i++)
		{
			if (m_vecPlayers[i] == nullptr)
				continue;

			clients[iNumClients++] = i;
		}
	}
	else if (targetType == ETargetType::T || targetType == ETargetType::CT)
	{
		for (size_t i = 0; i < sizeof(m_vecPlayers) / sizeof(*m_vecPlayers); i++)
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
	else
	{
		for (size_t i = 0; i < sizeof(m_vecPlayers) / sizeof(*m_vecPlayers); i++)
		{
			if (m_vecPlayers[i] == nullptr)
				continue;

			CBasePlayerController* player = (CBasePlayerController*)g_pEntitySystem->GetBaseEntity((CEntityIndex)(i + 1));

			if (!player)
				continue;

			if (V_stristr(const_cast<const char*>(&player->m_iszPlayerName()), const_cast<const char*>(target)))
			{
				targetType = ETargetType::PLAYER;
				clients[iNumClients++] = i;
			}
		}
	}

	return targetType;
}