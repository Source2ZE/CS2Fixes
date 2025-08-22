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

#include "hud_manager.h"
#include "../cs2fixes.h"
#include "../ctimer.h"
#include "../recipientfilters.h"
#include "engine/igameeventsystem.h"
#include "entity/cgamerules.h"
#include "gameevents.pb.h"
#include "networksystem/inetworkmessages.h"

CConVar<bool> g_cvarFixHudFlashing("cs2f_fix_hud_flashing", FCVAR_NONE, "Whether to fix hud flashing using a workaround, this BREAKS warmup so pick one or the other", false);
CConVar<bool> g_cvarDisableHudOutsideRound("cs2f_disable_hud_outside_round", FCVAR_NONE, "Whether to disable hud messages that would flash when a round is not ongoing, since flashing fix cannot run then", false);
CConVar<int> g_cvarHudDurationLeeway("cs2f_hud_duration_leeway", FCVAR_NONE, "Extra seconds duration to leave hud messages visible (without priority), reduces transition flashes between different priority messages", 2);

static std::vector<std::shared_ptr<CHudMessage>> g_vecHudMessages;

bool ShouldDisplayForPlayer(ZEPlayerHandle hPlayer, EHudPriority ePriority)
{
	for (std::shared_ptr<CHudMessage> pHudMessage : g_vecHudMessages)
	{
		// Require higher priority to block, so if priority matches most recent message takes precedence
		if (pHudMessage->HasRecipient(hPlayer) && pHudMessage->GetPriority() > ePriority)
			return false;
	}

	return true;
}

void CreateHudMessage(std::shared_ptr<CHudMessage> pHudMessage)
{
	if (!g_pGameRules)
		return;

	if (g_cvarDisableHudOutsideRound.Get() && !g_pGameRules->m_bGameRestart())
		return;

	static IGameEvent* pEvent = nullptr;

	if (!pEvent)
		pEvent = g_gameEventManager->CreateEvent("show_survival_respawn_status");

	if (!pEvent)
		return;

	pEvent->SetString("loc_token", pHudMessage->GetMessage());
	pEvent->SetInt("duration", pHudMessage->GetDuration() + g_cvarHudDurationLeeway.Get());
	pEvent->SetInt("userid", -1);

	INetworkMessageInternal* pMsg = g_pNetworkMessages->FindNetworkMessageById(GE_Source1LegacyGameEvent);

	if (!pMsg)
		return;

	CNetMessagePB<CMsgSource1LegacyGameEvent>* data = pMsg->AllocateMessage()->ToPB<CMsgSource1LegacyGameEvent>();
	CRecipientFilter filter;

	for (ZEPlayerHandle hPlayer : pHudMessage->GetRecipients())
		if (ShouldDisplayForPlayer(hPlayer, pHudMessage->GetPriority()))
			filter.AddRecipient(hPlayer.Get()->GetPlayerSlot());

	// Store the new hud message
	g_vecHudMessages.push_back(pHudMessage);

	// Start a timer to remove this hud message after its duration passes
	CTimer::Create(pHudMessage->GetDuration(), TIMERFLAG_NONE, [pHudMessage]() {
		g_vecHudMessages.erase(std::remove(g_vecHudMessages.begin(), g_vecHudMessages.end(), pHudMessage), g_vecHudMessages.end());

		return -1.0f;
	});

	g_gameEventManager->SerializeEvent(pEvent, data);
	g_gameEventSystem->PostEventAbstract(-1, false, &filter, pMsg, data, 0);
	delete data;
}

void SendHudMessage(ZEPlayer* pPlayer, int iDuration, EHudPriority ePriority, const char* pszMessage, ...)
{
	va_list args;
	va_start(args, pszMessage);

	char buf[1024];
	V_vsnprintf(buf, sizeof(buf), pszMessage, args);

	va_end(args);

	std::shared_ptr<CHudMessage> pHudMessage = std::make_shared<CHudMessage>(buf, iDuration, ePriority);

	pHudMessage->AddRecipient(pPlayer->GetHandle());
	CreateHudMessage(pHudMessage);
}

void SendHudMessageAll(int iDuration, EHudPriority ePriority, const char* pszMessage, ...)
{
	if (!GetGlobals())
		return;

	va_list args;
	va_start(args, pszMessage);

	char buf[1024];
	V_vsnprintf(buf, sizeof(buf), pszMessage, args);

	va_end(args);

	std::shared_ptr<CHudMessage> pHudMessage = std::make_shared<CHudMessage>(buf, iDuration, ePriority);

	for (int i = 0; i < GetGlobals()->maxClients; i++)
	{
		ZEPlayer* pPlayer = g_playerManager->GetPlayer(i);

		if (pPlayer)
			pHudMessage->AddRecipient(pPlayer->GetHandle());
	}

	CreateHudMessage(pHudMessage);
}

void StartFlashingFixTimer()
{
	// Timer that fakes m_bGameRestart enabled, to fix flashing with show_survival_respawn_status
	CTimer::Create(0.5f, TIMERFLAG_MAP, []() {
		if (!g_cvarFixHudFlashing.Get() || !g_pGameRules)
			return 0.5f;

		// Faking m_bGameRestart as true close to a round ending causes UI to falsely show game is restarting
		if (g_pGameRules->m_flRestartRoundTime.Get().GetTime() == 0.0f)
			g_pGameRules->m_bGameRestart = true;
		else
			g_pGameRules->m_bGameRestart = false;

		return 0.5f;
	});
}

std::string EscapeHTMLSpecialCharacters(std::string strMsg)
{
	// Always replace & first, as it is used in html escaped characters (so dont want to replace inside an escape)
	for (size_t iPos = 0; (iPos = strMsg.find('&', iPos)) != std::string::npos; iPos += 5)
		strMsg.replace(iPos, 1, "&amp;");

	std::unordered_map<std::string, std::string> mapReplacements{
		{"<",  "&lt;"	},
		{">",  "&gt;"	},
		{"\"", "&quot;"},
		{"\'", "&apos;"}
	};

	for (const auto& [strBadChar, strEscapedChar] : mapReplacements)
		for (size_t iPos = 0; (iPos = strMsg.find(strBadChar, iPos)) != std::string::npos; iPos += strEscapedChar.length())
			strMsg.replace(iPos, strBadChar.length(), strEscapedChar);

	return strMsg;
}