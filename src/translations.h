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

#pragma once
#include "common.h"
#include "convar.h"
#include "utlstring.h"
#include <map>
#include <string>
#include <vector>

#define LANG_PREF_KEY_NAME "language"

extern CConVar<CUtlString> g_cvarDefaultLanguage;
extern CConVar<bool> g_cvarTranslationsEnable;

class CCSPlayerController;

struct CPhrase
{
	std::map<std::string, std::string> m_mapTranslations;
};

class CTranslations
{
public:
	CTranslations();

	bool LoadPhraseFile(const char* pszFilename);

	const char* Translate(const char* pszPhraseKey, int iSlot);

	void SetPlayerLanguage(int iSlot, const char* pszLangCode);
	const char* GetPlayerLanguage(int iSlot);
	void ResetPlayerLanguage(int iSlot);

	const std::vector<std::string>& GetLanguages() const { return m_vecLanguages; }
	const char* GetLanguageDisplayName(const char* pszLangCode);

	static std::string ProcessColorTokens(const std::string& strText);

private:
	std::map<uint32, CPhrase> m_mapPhrases;
	std::string m_strPlayerLanguages[MAXPLAYERS];
	std::vector<std::string> m_vecLanguages;

	void RegisterLanguage(const char* pszLangCode);
};

extern CTranslations* g_pTranslations;
 
void ClientPrintT(CCSPlayerController* player, int hud_dest, const char* msg, ...);
void ClientPrintAllT(int hud_dest, const char* msg, ...);