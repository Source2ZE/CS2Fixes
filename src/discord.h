/**
 * =============================================================================
 * CS2Fixes
 * Copyright (C) 2023-2026 Source2ZE
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

class CDiscordBot
{
public:
	CDiscordBot(std::string strName, std::string strWebhookUrl, std::string strAvatarUrl, bool bOverrideName)
	{
		m_strName = strName;
		m_strWebhookUrl = strWebhookUrl;
		m_strAvatarUrl = strAvatarUrl;
		m_bOverrideName = bOverrideName;
	}

	const char* GetName() { return m_strName.c_str(); };
	const char* GetWebhookUrl() { return m_strWebhookUrl.c_str(); };
	const char* GetAvatarUrl() { return m_strAvatarUrl.c_str(); };
	void PostMessage(std::string strMessage);

private:
	std::string m_strName;
	std::string m_strWebhookUrl;
	std::string m_strAvatarUrl;
	bool m_bOverrideName;
};

class CDiscordBotManager
{
public:
	CDiscordBotManager()
	{
		LoadDiscordBotsConfig();
	}

	void PostDiscordMessage(const char* pszDiscordBotName, const char* pszMessage);
	bool LoadDiscordBotsConfig();

private:
	std::vector<std::shared_ptr<CDiscordBot>> m_vecDiscordBots;
};

extern CDiscordBotManager* g_pDiscordBotManager;