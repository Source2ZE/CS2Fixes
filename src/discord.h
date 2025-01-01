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

#include "httpmanager.h"
#include "utlvector.h"

class CDiscordBot
{
public:
	CDiscordBot(const char* pszName, const char* pszWebhookUrl, const char* pszAvatarUrl, bool bOverrideName)
	{
		V_strcpy(m_pszName, pszName);
		V_strcpy(m_pszWebhookUrl, pszWebhookUrl);
		V_strcpy(m_pszAvatarUrl, pszAvatarUrl);
		m_bOverrideName = bOverrideName;
	}

	const char* GetName() { return (const char*)m_pszName; };
	const char* GetWebhookUrl() { return m_pszWebhookUrl; };
	const char* GetAvatarUrl() { return m_pszAvatarUrl; };
	void PostMessage(const char* sMessage);

private:
	char m_pszName[32];
	char m_pszWebhookUrl[256];
	char m_pszAvatarUrl[256];
	bool m_bOverrideName;
};

class CDiscordBotManager
{
public:
	CDiscordBotManager()
	{
		LoadDiscordBotsConfig();
	}

	void PostDiscordMessage(const char* sDiscordBotName, const char* sMessage);
	bool LoadDiscordBotsConfig();

private:
	CUtlVector<CDiscordBot> m_vecDiscordBots;
};

extern CDiscordBotManager* g_pDiscordBotManager;