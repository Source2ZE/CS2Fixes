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

#include "discord.h"
#include "KeyValues.h"
#include "common.h"
#include "httpmanager.h"
#include "interfaces/interfaces.h"
#include "utlstring.h"

#include "vendor/nlohmann/json.hpp"

using json = nlohmann::json;

CDiscordBotManager* g_pDiscordBotManager = nullptr;

CConVar<bool> g_cvarDebugDiscordRequests("cs2f_debug_discord_messages", FCVAR_NONE, "Whether to include debug information for Discord requests", false);

void DiscordHttpCallback(HTTPRequestHandle request, json response)
{
	if (g_cvarDebugDiscordRequests.Get())
		Message("Discord post received response: %s\n", response.dump().c_str());
}

void CDiscordBotManager::PostDiscordMessage(const char* sDiscordBotName, const char* sMessage)
{
	FOR_EACH_VEC(m_vecDiscordBots, i)
	{
		CDiscordBot bot = m_vecDiscordBots[i];

		if (g_cvarDebugDiscordRequests.Get())
			Message("The bot at %i is %s with %s webhook and %s avatar.\n", i, bot.GetName(), bot.GetWebhookUrl(), bot.GetAvatarUrl());

		if (!V_stricmp(sDiscordBotName, bot.GetName()))
			bot.PostMessage(sMessage);
	}
}

void CDiscordBot::PostMessage(const char* sMessage)
{
	json jRequestBody;

	// Fill up the Json fields
	jRequestBody["content"] = sMessage;

	if (m_bOverrideName)
		jRequestBody["username"] = m_pszName;

	if (V_strcmp(m_pszAvatarUrl, ""))
		jRequestBody["avatar_url"] = m_pszAvatarUrl;

	// Send the request
	std::string sRequestBody = jRequestBody.dump();
	if (g_cvarDebugDiscordRequests.Get())
		Message("Sending '%s' to %s.\n", sRequestBody.c_str(), GetWebhookUrl());
	g_HTTPManager.Post(m_pszWebhookUrl, sRequestBody.c_str(), &DiscordHttpCallback);
}

bool CDiscordBotManager::LoadDiscordBotsConfig()
{
	m_vecDiscordBots.Purge();
	KeyValues* pKV = new KeyValues("discordbots");
	KeyValues::AutoDelete autoDelete(pKV);

	const char* pszPath = "addons/cs2fixes/configs/discordbots.cfg";
	if (!pKV->LoadFromFile(g_pFullFileSystem, pszPath))
	{
		Warning("Failed to load %s\n", pszPath);
		return false;
	}
	for (KeyValues* pKey = pKV->GetFirstSubKey(); pKey; pKey = pKey->GetNextKey())
	{
		const char* pszName = pKey->GetName();
		const char* pszWebhookUrl = pKey->GetString("webhook", nullptr);
		const char* pszAvatarUrl = pKey->GetString("avatar", nullptr);
		bool bOverrideName = pKey->GetBool("override_name", false);

		if (!pszWebhookUrl)
		{
			Warning("Discord bot entry %s is missing 'webhook' key\n", pszName);
			return false;
		}

		if (!pszAvatarUrl)
			pszAvatarUrl = "";

		// We just append the bots as-is
		CDiscordBot bot = CDiscordBot(pszName, pszWebhookUrl, pszAvatarUrl, bOverrideName);
		ConMsg("Loaded DiscordBot config %s\n", bot.GetName());
		ConMsg(" - Webhook URL: %s\n", bot.GetWebhookUrl());
		ConMsg(" - Avatar URL: %s\n", bot.GetAvatarUrl());
		m_vecDiscordBots.AddToTail(bot);
	}

	return true;
}
