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

#pragma once

#include "../playermanager.h"
#include <string>
#include <vector>

class CHudMessage
{
public:
	CHudMessage(std::string sMessage, int iDuration, int iPriority)
	{
		m_strMessage = sMessage;
		m_iDuration = iDuration;
		m_iPriority = iPriority;
	}

	const char* GetMessage() { return m_strMessage.c_str(); };
	int GetDuration() { return m_iDuration; };
	int GetPriority() { return m_iPriority; };
	std::vector<ZEPlayerHandle> GetRecipients() { return m_vecRecipients; };
	bool HasRecipient(ZEPlayerHandle hPlayer) { return std::find(m_vecRecipients.begin(), m_vecRecipients.end(), hPlayer) != m_vecRecipients.end(); };
	void AddRecipient(ZEPlayerHandle hPlayer) { m_vecRecipients.push_back(hPlayer); };

private:
	std::string m_strMessage;
	int m_iDuration;
	int m_iPriority;
	std::vector<ZEPlayerHandle> m_vecRecipients;
};

// When multiple hud messages are active, whichever one has highest priority one will display
// Note this is a basic implementation (TODO: is this worth expanding?), so e.g. a previously sent lower priority message will not display once a higher priority one expires
void SendHudMessage(ZEPlayer* pPlayer, int iDuration, int iPriority, const char* pszMessage, ...);
void SendHudMessageAll(int iDuration, int iPriority, const char* pszMessage, ...);

void StartFlashingFixTimer();
std::string EscapeHTMLSpecialCharacters(std::string strMsg);

extern CConVar<bool> g_cvarFixHudFlashing;