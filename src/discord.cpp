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

#include "discord.h"
#include "KeyValues.h"
#include "common.h"
#include "httpmanager.h"
#include "interfaces/interfaces.h"
#include "utlstring.h"
#include <fstream>

#include "vendor/nlohmann/json.hpp"

using json = nlohmann::json;
using ordered_json = nlohmann::ordered_json;

CDiscordBotManager* g_pDiscordBotManager = nullptr;

CConVar<bool> g_cvarDebugDiscordRequests("cs2f_debug_discord_messages", FCVAR_NONE, "Whether to include debug information for Discord requests", false);

void DiscordHttpCallback(HTTPRequestHandle request, json response)
{
	if (g_cvarDebugDiscordRequests.Get())
		Message("Discord post received response: %s\n", response.dump().c_str());
}

void CDiscordBotManager::PostDiscordMessage(const char* pszDiscordBotName, const char* pszMessage)
{
	for (auto pBot : m_vecDiscordBots)
	{
		if (g_cvarDebugDiscordRequests.Get())
			Message("The bot is %s with %s webhook and %s avatar.\n", pBot->GetName(), pBot->GetWebhookUrl(), pBot->GetAvatarUrl());

		if (!V_stricmp(pszDiscordBotName, pBot->GetName()))
			pBot->PostMessage(pszMessage);
	}
}

void CDiscordBot::PostMessage(std::string strMessage)
{
	json jRequestBody;

	// Fill up the Json fields
	jRequestBody["content"] = strMessage;

	if (m_bOverrideName)
		jRequestBody["username"] = m_strName;

	if (V_strcmp(GetAvatarUrl(), ""))
		jRequestBody["avatar_url"] = m_strAvatarUrl;

	// Send the request
	std::string sRequestBody = jRequestBody.dump();
	if (g_cvarDebugDiscordRequests.Get())
		Message("Sending '%s' to %s.\n", sRequestBody.c_str(), GetWebhookUrl());
	g_HTTPManager.Post(m_strWebhookUrl, sRequestBody, &DiscordHttpCallback);
}

bool CDiscordBotManager::LoadDiscordBotsConfig()
{
	m_vecDiscordBots.clear();

	const char* pszJsonPath = "addons/cs2fixes/configs/discordbots.jsonc";
	char szPath[MAX_PATH];
	V_snprintf(szPath, sizeof(szPath), "%s%s%s", Plat_GetGameDirectory(), "/csgo/", pszJsonPath);
	std::ifstream jsonFile(szPath);

	if (!jsonFile.is_open())
	{
		if (!ConvertDiscordBotsKVToJSON())
		{
			Panic("Failed to open %s and convert KV1 discordbots.cfg to JSON format, discord bots are not loaded!\n", pszJsonPath);
			return false;
		}

		jsonFile.open(szPath);
	}

	ordered_json jDiscordBots = ordered_json::parse(jsonFile, nullptr, false, true);

	if (jDiscordBots.is_discarded())
	{
		Panic("Failed parsing JSON from %s, discord bots are not loaded!\n", pszJsonPath);
		return false;
	}

	for (auto it = jDiscordBots.cbegin(); it != jDiscordBots.cend(); ++it)
	{
		const json& jDiscordBot = it.value();

		if (!jDiscordBot.contains("webhook"))
		{
			Panic("Discord bot entry %s is missing 'webhook' key\n", it.key().c_str());
			return false;
		}

		std::string strWebhookUrl = jDiscordBot.value("webhook", "");
		std::string strAvatarUrl = jDiscordBot.value("avatar", "");
		bool bOverrideName = jDiscordBot.value("override_name", false);

		// We just append the bots as-is
		std::shared_ptr<CDiscordBot> pBot = std::make_shared<CDiscordBot>(it.key(), strWebhookUrl, strAvatarUrl, bOverrideName);
		Message("Loaded DiscordBot config %s\n", pBot->GetName());
		Message(" - Webhook URL: %s\n", pBot->GetWebhookUrl());
		Message(" - Avatar URL: %s\n", pBot->GetAvatarUrl());
		m_vecDiscordBots.push_back(pBot);
	}

	return true;
}

// TODO: Remove this once servers have been given a few months to update cs2fixes
bool CDiscordBotManager::ConvertDiscordBotsKVToJSON()
{
	KeyValues* pKV = new KeyValues("discordbots");
	KeyValues::AutoDelete autoDelete(pKV);

	const char* pszPath = "addons/cs2fixes/configs/discordbots.cfg";
	if (!pKV->LoadFromFile(g_pFullFileSystem, pszPath))
	{
		Panic("Failed to load %s\n", pszPath);
		return false;
	}

	ordered_json jDiscordBots;

	for (KeyValues* pKey = pKV->GetFirstSubKey(); pKey; pKey = pKey->GetNextKey())
	{
		const char* pszWebhookUrl = pKey->GetString("webhook", nullptr);

		if (!pszWebhookUrl)
		{
			Panic("Discord bot entry %s is missing 'webhook' key\n", pKey->GetName());
			return false;
		}

		ordered_json jDiscordBot;
		jDiscordBot["webhook"] = pszWebhookUrl;

		if (pKey->FindKey("avatar"))
			jDiscordBot["avatar"] = pKey->GetString("avatar", "");

		jDiscordBot["override_name"] = pKey->GetBool("override_name", false);
		jDiscordBots[pKey->GetName()] = jDiscordBot;
	}

	const char* pszJsonPath = "addons/cs2fixes/configs/discordbots.jsonc";
	const char* pszKVConfigRenamePath = "addons/cs2fixes/configs/discordbots_old.cfg";
	char szPath[MAX_PATH];
	V_snprintf(szPath, sizeof(szPath), "%s%s%s", Plat_GetGameDirectory(), "/csgo/", pszJsonPath);
	std::ofstream jsonFile(szPath);

	if (!jsonFile.is_open())
	{
		Panic("Failed to open %s\n", pszJsonPath);
		return false;
	}

	jsonFile << std::setfill('\t') << std::setw(1) << jDiscordBots << std::endl;

	char szKVRenamePath[MAX_PATH];
	V_snprintf(szPath, sizeof(szPath), "%s%s%s", Plat_GetGameDirectory(), "/csgo/", pszPath);
	V_snprintf(szKVRenamePath, sizeof(szPath), "%s%s%s", Plat_GetGameDirectory(), "/csgo/", pszKVConfigRenamePath);

	std::rename(szPath, szKVRenamePath);

	// remove old cfg example if it exists
	const char* pszKVExamplePath = "addons/cs2fixes/configs/discordbots.cfg.example";
	V_snprintf(szPath, sizeof(szPath), "%s%s%s", Plat_GetGameDirectory(), "/csgo/", pszKVExamplePath);
	std::remove(szPath);

	Message("Successfully converted KV1 discordbots.cfg to JSON format at %s\n", pszJsonPath);
	return true;
}
