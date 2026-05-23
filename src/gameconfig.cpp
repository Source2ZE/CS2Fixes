#include "gameconfig.h"
#include "addresses.h"
#undef snprintf
#include "vendor/nlohmann/json.hpp"

#include <cctype>
#include <fstream>

CGameConfig* g_GameConfig = nullptr;

bool CGameConfig::Init(char* conf_error, int conf_error_size)
{
	const char* pszGamedataPath = "addons/cs2fixes/gamedata/cs2fixes.jsonc";
	char szPath[MAX_PATH];
	V_snprintf(szPath, sizeof(szPath), "%s%s%s", Plat_GetGameDirectory(), "/csgo/", pszGamedataPath);
	std::ifstream gamedataFile(szPath);

	if (!gamedataFile.is_open())
	{
		snprintf(conf_error, conf_error_size, "Failed to open %s, gamedata not loaded", pszGamedataPath);
		return false;
	}

	ordered_json jsonGamedata = ordered_json::parse(gamedataFile, nullptr, false, true);

	if (jsonGamedata.is_discarded() || !jsonGamedata.is_object())
	{
		snprintf(conf_error, conf_error_size, "Failed parsing gamedata JSON from %s", pszGamedataPath);
		return false;
	}

#if defined _LINUX
	const char* platform = "linux";
#else
	const char* platform = "windows";
#endif

	for (auto& [strSection, jsonSection] : jsonGamedata.items())
	{
		if (!jsonSection.is_object())
		{
			snprintf(conf_error, conf_error_size, "Section '%s' must be an object", strSection.c_str());
			return false;
		}

		for (auto& [strEntry, jsonEntry] : jsonSection.items())
		{
			if (!jsonEntry.is_object())
			{
				snprintf(conf_error, conf_error_size, "Entry '%s' must be an object", strEntry.c_str());
				return false;
			}

			if (strSection == "Offsets")
			{
				const auto platformOffset = jsonEntry.find(platform);
				if (platformOffset == jsonEntry.end())
					continue;

				if (!platformOffset->is_number_integer())
				{
					snprintf(conf_error, conf_error_size, "Offset '%s' '%s' value is not numeric", strEntry.c_str(), platform);
					return false;
				}

				m_umOffsets[strEntry] = platformOffset->get<int>();
			}
			else if (strSection == "Signatures")
			{
				const auto library = jsonEntry.find("library");
				if (library == jsonEntry.end() || !library->is_string())
				{
					snprintf(conf_error, conf_error_size, "Signature '%s' is missing string 'library' value", strEntry.c_str());
					return false;
				}

				m_umLibraries[strEntry] = library->get<std::string>();

				const auto platformValue = jsonEntry.find(platform);
				if (platformValue == jsonEntry.end())
					continue;

				if (!platformValue->is_string())
				{
					snprintf(conf_error, conf_error_size, "Signature '%s' '%s' value is not a string", strEntry.c_str(), platform);
					return false;
				}

				m_umSignatures[strEntry] = platformValue->get<std::string>();
			}
			else if (strSection == "Patches")
			{
				const auto platformValue = jsonEntry.find(platform);
				if (platformValue == jsonEntry.end())
					continue;

				if (!platformValue->is_string())
				{
					snprintf(conf_error, conf_error_size, "Patch '%s' '%s' value is not a string", strEntry.c_str(), platform);
					return false;
				}

				m_umPatches[strEntry] = platformValue->get<std::string>();
			}
		}
	}

	return true;
}

const char* CGameConfig::GetSignature(const std::string& name)
{
	auto it = m_umSignatures.find(name);
	if (it == m_umSignatures.end())
		return nullptr;
	return it->second.c_str();
}

const char* CGameConfig::GetPatch(const std::string& name)
{
	auto it = m_umPatches.find(name);
	if (it == m_umPatches.end())
		return nullptr;
	return it->second.c_str();
}

int CGameConfig::GetOffset(const std::string& name)
{
	auto it = m_umOffsets.find(name);
	if (it == m_umOffsets.end())
		return -1;
	return it->second;
}

const char* CGameConfig::GetLibrary(const std::string& name)
{
	auto it = m_umLibraries.find(name);
	if (it == m_umLibraries.end())
		return nullptr;
	return it->second.c_str();
}

CModule** CGameConfig::GetModule(const char* name)
{
	const char* library = this->GetLibrary(name);
	if (!library)
		return nullptr;

	if (strcmp(library, "engine") == 0)
		return &modules::engine;
	else if (strcmp(library, "server") == 0)
		return &modules::server;
	else if (strcmp(library, "client") == 0)
		return &modules::client;
	else if (strcmp(library, "vscript") == 0)
		return &modules::vscript;
	else if (strcmp(library, "tier0") == 0)
		return &modules::tier0;
	else if (strcmp(library, "networksystem") == 0)
		return &modules::networksystem;
	else if (strcmp(library, "matchmaking") == 0)
		return &modules::matchmaking;
#ifdef _WIN32
	else if (strcmp(library, "hammer") == 0)
		return &modules::hammer;
#endif
	return nullptr;
}

bool CGameConfig::IsSymbol(const char* name)
{
	const char* sigOrSymbol = this->GetSignature(name);
	if (!sigOrSymbol || strlen(sigOrSymbol) <= 0)
	{
		Panic("Missing signature or symbol\n", name);
		return false;
	}
	return sigOrSymbol[0] == '@';
}

const char* CGameConfig::GetSymbol(const char* name)
{
	const char* symbol = this->GetSignature(name);

	if (!symbol || strlen(symbol) <= 1)
	{
		Panic("Missing symbol\n", name);
		return nullptr;
	}
	return symbol + 1;
}

void* CGameConfig::ResolveSignature(const char* name)
{
	CModule** module = this->GetModule(name);
	if (!module || !(*module))
	{
		Panic("Invalid Module %s\n", name);
		return nullptr;
	}

	void* address = nullptr;
	if (this->IsSymbol(name))
	{
		const char* symbol = this->GetSymbol(name);
		if (!symbol)
		{
			Panic("Invalid symbol for %s\n", name);
			return nullptr;
		}
		address = dlsym((*module)->m_hModule, symbol);
	}
	else
	{
		const char* signature = this->GetSignature(name);
		if (!signature)
		{
			Panic("Failed to find signature for %s\n", name);
			return nullptr;
		}

		size_t iLength = 0;
		byte* pSignature = IDASigToUint8Array(signature, iLength);
		if (!pSignature)
			return nullptr;

		int error;

		address = (*module)->FindSignature(pSignature, iLength, error);

		if (error == SIG_FOUND_MULTIPLE)
			Panic("!!!!!!!!!! Signature for %s occurs multiple times! Using first match but this might end up crashing!\n", name);
	}

	if (!address)
	{
		Panic("Failed to find address for %s\n", name);
		return nullptr;
	}
	return address;
}

int CGameConfig::ParseHexNibble(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';

	const char lower = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	if (lower >= 'a' && lower <= 'f')
		return lower - 'a' + 10;

	return -1;
}

bool CGameConfig::ParsePatternBytes(const char* pattern, std::vector<uint8_t>& bytes)
{
	if (!pattern)
		return false;

	const char* cursor = pattern;
	while (*cursor)
	{
		while (*cursor && std::isspace(static_cast<unsigned char>(*cursor)))
			cursor++;

		if (!*cursor)
			break;

		if (*cursor == '?')
		{
			bytes.push_back('\x2A');
			cursor++;
			if (*cursor == '?')
				cursor++;
			continue;
		}

		const int highNibble = ParseHexNibble(cursor[0]);
		const int lowNibble = ParseHexNibble(cursor[1]);
		if (highNibble < 0 || lowNibble < 0)
			return false;

		bytes.push_back(static_cast<uint8_t>((highNibble << 4) | lowNibble));
		cursor += 2;
	}

	return !bytes.empty();
}

byte* CGameConfig::IDASigToUint8Array(const char* signature, size_t& length)
{
	if (!signature || strlen(signature) <= 0)
	{
		Panic("Invalid IDA signature string\n");
		return nullptr;
	}

	std::vector<uint8_t> bytes;
	if (!ParsePatternBytes(signature, bytes))
	{
		Panic("Invalid IDA signature format \"%s\"\n", signature);
		return nullptr;
	}

	length = bytes.size();
	uint8_t* dest = new uint8_t[length];

	for (size_t i = 0; i < length; i++)
		dest[i] = bytes[i];

	return (byte*)dest;
}
