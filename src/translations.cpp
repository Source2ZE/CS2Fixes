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

#include "translations.h"
#include "commands.h"
#include "entity/ccsplayercontroller.h"
#include "networksystem/inetworkmessages.h"
#include "playermanager.h"
#include "recipientfilters.h"
#include "strtools.h"
#include "user_preferences.h"
#include "usermessages.pb.h"
#include <../cs2fixes.h>
#include <fstream>

#undef snprintf
#include "vendor/nlohmann/json.hpp"

#include "tier0/memdbgon.h"

using json = nlohmann::json;

CTranslations* g_pTranslations = nullptr;

CConVar<CUtlString> g_cvarDefaultLanguage("cs2f_default_language", FCVAR_NONE, "Default language code for translations, used when per-player language is disabled or not set", "en");
CConVar<bool> g_cvarTranslationsEnable("cs2f_translations_enable", FCVAR_NONE, "Whether to enable per-player language selection via !lang", false);

struct ColorToken
{
	const char* pszName;
	const char* pszCode;
};

static const ColorToken s_ColorTokens[] = {
	{"{default}", "\x01"},
	{"{darkred}", "\x02"},
	{"{team}", "\x03"},
	{"{green}", "\x04"},
	{"{olive}", "\x05"},
	{"{lime}", "\x06"},
	{"{lightred}", "\x07"},
	{"{gray}", "\x08"},
	{"{yellow}", "\x09"},
	{"{gold}", "\x10"},
	{"{blue}", "\x0B"},
	{"{purple}", "\x0C"},
	{"{pink}", "\x0D"},
	{"{red}", "\x0E"},
	{"{orange}", "\x0F"},
};

struct LanguageInfo
{
	const char* pszCode;
	const char* pszDisplayName;
};

static const LanguageInfo s_LanguageNames[] = {
	{"en", "English"},
	{"es", "Español"},
	{"pt", "Português"},
	{"fr", "Français"},
	{"de", "Deutsch"},
	{"ru", "Русский"},
	{"zh", "中文"},
	{"ja", "日本語"},
	{"ko", "한국어"},
	{"pl", "Polski"},
	{"tr", "Türkçe"},
	{"it", "Italiano"},
};

CTranslations::CTranslations()
{
	for (int i = 0; i < MAXPLAYERS; i++)
		m_strPlayerLanguages[i] = "";
}

std::string CTranslations::ProcessColorTokens(const std::string& strText)
{
	std::string strResult = strText;
	for (const auto& token : s_ColorTokens)
	{
		size_t pos = 0;
		while ((pos = strResult.find(token.pszName, pos)) != std::string::npos)
		{
			strResult.replace(pos, strlen(token.pszName), token.pszCode);
			pos += strlen(token.pszCode);
		}
	}
	return strResult;
}

static std::vector<char> ExtractFormatSpecifiers(const std::string& str)
{
	std::vector<char> vecSpecs;
	for (size_t i = 0; i < str.size(); i++)
	{
		if (str[i] != '%')
			continue;

		i++;
		if (i >= str.size())
			break;

		if (str[i] == '%')
			continue;

		while (i < str.size() && (str[i] == '-' || str[i] == '+' || str[i] == '0' || str[i] == ' ' || str[i] == '#'))
			i++;

		while (i < str.size() && (isdigit(str[i]) || str[i] == '*'))
			i++;

		if (i < str.size() && str[i] == '.')
		{
			i++;
			while (i < str.size() && (isdigit(str[i]) || str[i] == '*'))
				i++;
		}

		while (i < str.size() && (str[i] == 'h' || str[i] == 'l' || str[i] == 'L' || str[i] == 'z' || str[i] == 'j' || str[i] == 't'))
			i++;

		if (i < str.size())
			vecSpecs.push_back(str[i]);
	}
	return vecSpecs;
}

void CTranslations::RegisterLanguage(const char* pszLangCode)
{
	for (const auto& lang : m_vecLanguages)
	{
		if (lang == pszLangCode)
			return;
	}
	m_vecLanguages.push_back(pszLangCode);
}

bool CTranslations::LoadPhraseFile(const char* pszFilename)
{
	char szFullPath[MAX_PATH];
	V_snprintf(szFullPath, sizeof(szFullPath), "%s/csgo/addons/cs2fixes/configs/translations/%s", Plat_GetGameDirectory(), pszFilename);

	std::ifstream file(szFullPath);
	if (!file.is_open())
	{
		Panic("[Translations] Failed to load translation file: %s\n", pszFilename);
		return false;
	}

	json jRoot = json::parse(file, nullptr, false, true);
	if (jRoot.is_discarded() || !jRoot.is_object())
	{
		Panic("[Translations] Failed to parse translation file: %s\n", pszFilename);
		return false;
	}

	int iPhraseCount = 0;

	for (auto it = jRoot.begin(); it != jRoot.end(); ++it)
	{
		const std::string& strPhraseKey = it.key();
		const json& jTranslations = it.value();

		if (!jTranslations.is_object())
			continue;

		uint32 uHash = hash_32_fnv1a_const(strPhraseKey.c_str());
		CPhrase& phrase = m_mapPhrases[uHash];

		for (auto langIt = jTranslations.begin(); langIt != jTranslations.end(); ++langIt)
		{
			if (!langIt.value().is_string())
				continue;

			const std::string& strLangCode = langIt.key();
			std::string strProcessed = ProcessColorTokens(langIt.value().get<std::string>());
			phrase.m_mapTranslations[strLangCode] = strProcessed;
			RegisterLanguage(strLangCode.c_str());
		}

		iPhraseCount++;
	}

	Message("[Translations] Loaded %d phrases from %s\n", iPhraseCount, pszFilename);

	// Validate format specifiers against the reference language
	const char* pszRefLang = g_cvarDefaultLanguage.Get().String();
	if (!pszRefLang || !*pszRefLang)
		pszRefLang = "en";

	int iIncompleteCount = 0;
	int iFormatErrors = 0;

	for (auto& pair : m_mapPhrases)
	{
		CPhrase& phrase = pair.second;

		bool bIncomplete = false;
		for (const auto& lang : m_vecLanguages)
		{
			if (phrase.m_mapTranslations.find(lang) == phrase.m_mapTranslations.end())
			{
				if (!bIncomplete)
				{
					bIncomplete = true;
					iIncompleteCount++;
				}
			}
		}

		auto refIt = phrase.m_mapTranslations.find(pszRefLang);
		if (refIt == phrase.m_mapTranslations.end())
			refIt = phrase.m_mapTranslations.find("en");
		if (refIt == phrase.m_mapTranslations.end())
			continue;

		std::vector<char> vecRefSpecs = ExtractFormatSpecifiers(refIt->second);

		std::vector<std::string> vecBadLangs;
		for (auto& trans : phrase.m_mapTranslations)
		{
			if (trans.first == refIt->first)
				continue;

			std::vector<char> vecSpecs = ExtractFormatSpecifiers(trans.second);
			if (vecSpecs != vecRefSpecs)
			{
				Panic("[Translations] Translation format mismatch for phrase hash %u, lang \"%s\"\n", pair.first, trans.first.c_str());
				vecBadLangs.push_back(trans.first);
				iFormatErrors++;
			}
		}

		for (const auto& badLang : vecBadLangs)
			phrase.m_mapTranslations[badLang] = refIt->second;
	}

	std::string strLangList;
	for (size_t i = 0; i < m_vecLanguages.size(); i++)
	{
		if (i > 0)
			strLangList += ", ";
		strLangList += m_vecLanguages[i];
	}
	Message("[Translations] %d languages loaded: %s\n", (int)m_vecLanguages.size(), strLangList.c_str());

	bool bDefaultLangFound = false;
	for (const auto& lang : m_vecLanguages)
	{
		if (lang == pszRefLang)
		{
			bDefaultLangFound = true;
			break;
		}
	}

	if (!bDefaultLangFound)
		Panic("[Translations] cs2f_default_language \"%s\" was not found in any loaded translation file\n", pszRefLang);

	if (iIncompleteCount > 0)
		Panic("[Translations] %d phrase(s) have incomplete translations for %s\n", iIncompleteCount, pszFilename);

	if (iFormatErrors > 0)
		Panic("[Translations] %d translation(s) had format specifier mismatches in %s\n", iFormatErrors, pszFilename);

	return true;
}

const char* CTranslations::Translate(const char* pszPhraseKey, int iSlot)
{
	uint32 uHash = hash_32_fnv1a_const(pszPhraseKey);

	auto it = m_mapPhrases.find(uHash);
	if (it == m_mapPhrases.end())
		return pszPhraseKey;

	const CPhrase& phrase = it->second;

	if (g_cvarTranslationsEnable.Get())
	{
		const char* pszPlayerLang = GetPlayerLanguage(iSlot);
		if (pszPlayerLang && *pszPlayerLang)
		{
			auto langIt = phrase.m_mapTranslations.find(pszPlayerLang);
			if (langIt != phrase.m_mapTranslations.end())
				return langIt->second.c_str();
		}
	}

	const char* pszDefaultLang = g_cvarDefaultLanguage.Get().String();
	if (pszDefaultLang && *pszDefaultLang)
	{
		auto langIt = phrase.m_mapTranslations.find(pszDefaultLang);
		if (langIt != phrase.m_mapTranslations.end())
			return langIt->second.c_str();
	}

	auto langIt = phrase.m_mapTranslations.find("en");
	if (langIt != phrase.m_mapTranslations.end())
		return langIt->second.c_str();

	if (!phrase.m_mapTranslations.empty())
		return phrase.m_mapTranslations.begin()->second.c_str();

	return pszPhraseKey;
}

void CTranslations::SetPlayerLanguage(int iSlot, const char* pszLangCode)
{
	if (iSlot < 0 || iSlot >= MAXPLAYERS)
		return;

	m_strPlayerLanguages[iSlot] = pszLangCode ? pszLangCode : "";
}

const char* CTranslations::GetPlayerLanguage(int iSlot)
{
	if (iSlot < 0 || iSlot >= MAXPLAYERS)
		return g_cvarDefaultLanguage.Get().String();

	if (m_strPlayerLanguages[iSlot].empty())
		return g_cvarDefaultLanguage.Get().String();

	return m_strPlayerLanguages[iSlot].c_str();
}

void CTranslations::ResetPlayerLanguage(int iSlot)
{
	if (iSlot < 0 || iSlot >= MAXPLAYERS)
		return;

	m_strPlayerLanguages[iSlot] = "";
}

const char* CTranslations::GetLanguageDisplayName(const char* pszLangCode)
{
	for (const auto& info : s_LanguageNames)
	{
		if (!V_stricmp(info.pszCode, pszLangCode))
			return info.pszDisplayName;
	}
	return pszLangCode;
}

static std::string TranslatePlaceholders(const char* pszMessage, int iSlot)
{
	std::string strMessage = pszMessage;
	size_t pos = 0;

	while ((pos = strMessage.find('{', pos)) != std::string::npos)
	{
		size_t endPos = strMessage.find('}', pos);
		if (endPos == std::string::npos)
			break;

		std::string key = strMessage.substr(pos + 1, endPos - pos - 1);
		if (g_pTranslations)
		{
			const char* pszTranslated = g_pTranslations->Translate(key.c_str(), iSlot);
			if (V_strcmp(pszTranslated, key.c_str()) != 0)
			{
				strMessage.replace(pos, endPos - pos + 1, pszTranslated);
				pos += strlen(pszTranslated);
				continue;
			}
		}

		pos = endPos + 1;
	}

	if (g_pTranslations)
		strMessage = g_pTranslations->ProcessColorTokens(strMessage);

	return strMessage;
}

void ClientPrintT(CCSPlayerController* player, int hud_dest, const char* msg, ...)
{
	int iSlot = player ? player->GetPlayerSlot() : -1;
	std::string strMessage = TranslatePlaceholders(msg, iSlot);

	va_list args;
	va_start(args, msg);

	char buf[2048];
	V_vsnprintf(buf, sizeof(buf), strMessage.c_str(), args);
	va_end(args);

	if (!player || !player->IsConnected() || player->IsBot())
	{
		ConMsg("%s\n", buf);
		return;
	}

	INetworkMessageInternal* pNetMsg = g_pNetworkMessages->FindNetworkMessagePartial("TextMsg");
	auto data = pNetMsg->AllocateMessage()->ToPB<CUserMessageTextMsg>();

	data->set_dest(hud_dest);
	data->add_param(buf);

	CSingleRecipientFilter filter(player->GetPlayerSlot());
	g_gameEventSystem->PostEventAbstract(-1, false, &filter, pNetMsg, data, 0);

	delete data;
}

void ClientPrintAllT(int hud_dest, const char* msg, ...)
{
	if (!GetGlobals())
		return;

	for (int i = 0; i < GetGlobals()->maxClients; i++)
	{
		CCSPlayerController* pController = CCSPlayerController::FromSlot(i);
		if (!pController || !pController->IsConnected() || pController->IsBot())
			continue;

		std::string strMessage = TranslatePlaceholders(msg, i);

		va_list args;
		va_start(args, msg);

		char buf[2048];
		V_vsnprintf(buf, sizeof(buf), strMessage.c_str(), args);
		va_end(args);

		INetworkMessageInternal* pNetMsg = g_pNetworkMessages->FindNetworkMessagePartial("TextMsg");
		auto data = pNetMsg->AllocateMessage()->ToPB<CUserMessageTextMsg>();

		data->set_dest(hud_dest);
		data->add_param(buf);

		CSingleRecipientFilter filter(pController->GetPlayerSlot());
		g_gameEventSystem->PostEventAbstract(-1, false, &filter, pNetMsg, data, 0);

		delete data;
	}

	std::string ServerConsoleMsg = TranslatePlaceholders(msg, -1);
	
	va_list args;
	va_start(args, msg);

	char ConsoleBuf[2048];
	V_vsnprintf(ConsoleBuf, sizeof(ConsoleBuf), ServerConsoleMsg.c_str(), args);
	va_end(args);

	ConMsg("%s\n", ConsoleBuf);
}

CON_COMMAND_CHAT(lang, "[code] - Change your language or view current language")
{
	if (!g_cvarTranslationsEnable.Get())
		return;

	if (!player)
	{
		ClientPrintT(player, HUD_PRINTCONSOLE, CHAT_PREFIX "{General.NoServerConsole}");
		return;
	}

	int iSlot = player->GetPlayerSlot();

	if (args.ArgC() < 2)
	{
		const char* pszCurrentLang = g_pTranslations->GetPlayerLanguage(iSlot);
		const char* pszDisplayName = g_pTranslations->GetLanguageDisplayName(pszCurrentLang);

		ClientPrintT(player, HUD_PRINTTALK, CHAT_PREFIX "{Lang.Current}", pszDisplayName, pszCurrentLang);
		return;
	}

	const char* pszNewLang = args[1];

	bool bFound = false;
	const auto& languages = g_pTranslations->GetLanguages();
	for (const auto& lang : languages)
	{
		if (!V_stricmp(lang.c_str(), pszNewLang))
		{
			pszNewLang = lang.c_str();
			bFound = true;
			break;
		}
	}

	if (!bFound)
	{
		ClientPrintT(player, HUD_PRINTTALK, CHAT_PREFIX "{Lang.NotFound}", pszNewLang);

		std::string strLangs;
		for (size_t i = 0; i < languages.size(); i++)
		{
			if (i > 0)
				strLangs += ", ";
			strLangs += languages[i];
		}
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "%s", strLangs.c_str());
		return;
	}

	g_pTranslations->SetPlayerLanguage(iSlot, pszNewLang);

	ZEPlayer* pZEPlayer = player->GetZEPlayer();
	if (pZEPlayer && pZEPlayer->IsAuthenticated())
		g_pUserPreferencesSystem->SetPreference(iSlot, LANG_PREF_KEY_NAME, pszNewLang);

	const char* pszDisplayName = g_pTranslations->GetLanguageDisplayName(pszNewLang);
	ClientPrintT(player, HUD_PRINTTALK, CHAT_PREFIX "{Lang.Changed}", pszDisplayName);
}

CON_COMMAND_CHAT(langs, "- Show all available languages")
{
	if (!g_cvarTranslationsEnable.Get())
		return;

	if (!player)
	{
		ClientPrintT(player, HUD_PRINTCONSOLE, "{Lang.AvailableConsole}");
		const auto& languages = g_pTranslations->GetLanguages();
		for (const auto& lang : languages)
			ClientPrint(player, HUD_PRINTCONSOLE, "  %s - %s", lang.c_str(), g_pTranslations->GetLanguageDisplayName(lang.c_str()));
		return;
	}

	const auto& languages = g_pTranslations->GetLanguages();
	int iSlot = player->GetPlayerSlot();
	const char* pszCurrentLang = g_pTranslations->GetPlayerLanguage(iSlot);

	ClientPrintT(player, HUD_PRINTTALK, CHAT_PREFIX "{Lang.List}");

	for (const auto& lang : languages)
	{
		const char* pszDisplay = g_pTranslations->GetLanguageDisplayName(lang.c_str());
		bool bCurrent = !V_stricmp(lang.c_str(), pszCurrentLang);

		if (bCurrent)
			ClientPrintT(player, HUD_PRINTTALK, CHAT_PREFIX "{Lang.ListItemCurrent}", lang.c_str(), pszDisplay);
		else
			ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX " %s - %s", lang.c_str(), pszDisplay);
	}
}
