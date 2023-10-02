#include "playermanager.h"

void CPlayerManager::OnClientConnected(CPlayerSlot slot)
{
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

		if (m_vecPlayers[i]->IsAuthenticated())
			continue;

		if (g_pEngineServer2->IsClientFullyAuthenticated(i))
		{
			m_vecPlayers[i]->SetAuthenticated();
			m_vecPlayers[i]->SetSteamId(g_pEngineServer2->GetClientSteamID(i)->ConvertToUint64());
			Message("%lli authenticated %d\n", m_vecPlayers[i]->GetSteamId(), i);
		}
	}
}