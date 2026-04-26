#pragma once

#undef snprintf
#include "vendor/nlohmann/json_fwd.hpp"
#include "wchartypes.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

class CModule;
using ordered_json = nlohmann::ordered_json;

class CGameConfig
{
public:
	bool Init(char* conf_error, int conf_error_size);
	const char* GetLibrary(const std::string& name);
	const char* GetSignature(const std::string& name);
	const char* GetSymbol(const char* name);
	const char* GetPatch(const std::string& name);
	int GetOffset(const std::string& name);
	void* GetAddress(const std::string& name, void* engine, void* server, char* error, int maxlen);
	CModule** GetModule(const char* name);
	bool IsSymbol(const char* name);
	void* ResolveSignature(const char* name);
	int ParseHexNibble(char c);
	bool ParsePatternBytes(const char* pattern, std::vector<uint8_t>& bytes);
	byte* IDASigToUint8Array(const char* signature, size_t& length);

private:
	std::unordered_map<std::string, int> m_umOffsets;
	std::unordered_map<std::string, std::string> m_umSignatures;
	std::unordered_map<std::string, void*> m_umAddresses;
	std::unordered_map<std::string, std::string> m_umLibraries;
	std::unordered_map<std::string, std::string> m_umPatches;
};

extern CGameConfig* g_GameConfig;
