#include "utlstring.h"
#include "playermanager.h"
#include "adminsystem.h"
#include "entity/ccsplayercontroller.h"

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

ETargetType CPlayerManager::TargetPlayerString(const char* target, int& iNumClients, int *clients)
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
	else
	{
		for (int i = 0; i < sizeof(m_vecPlayers) / sizeof(*m_vecPlayers); i++)
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