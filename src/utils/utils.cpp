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

#include "utils.h"
#include "../cs2fixes.h"
#include "../gameconfig.h"
#include "entitysystem.h"
#include "entityclass.h"

void Message(const char* msg, ...)
{
	va_list args;
	va_start(args, msg);

	char buf[1024] = {};
	V_vsnprintf(buf, sizeof(buf) - 1, msg, args);

	ConColorMsg(Color(255, 0, 255, 255), "[CS2Fixes] %s", buf);

	va_end(args);
}

void Panic(const char* msg, ...)
{
	va_list args;
	va_start(args, msg);

	char buf[1024] = {};
	V_vsnprintf(buf, sizeof(buf) - 1, msg, args);

	Warning("[CS2Fixes] %s", buf);

	va_end(args);
}

CUtlVector<CServerSideClient*>* GetClientList()
{
	if (!GetNetworkGameServer())
		return nullptr;

	static int offset = g_GameConfig->GetOffset("CNetworkGameServer_ClientList");
	return (CUtlVector<CServerSideClient*>*)(&GetNetworkGameServer()[offset]);
}

CServerSideClient* GetClientBySlot(CPlayerSlot slot)
{
	CUtlVector<CServerSideClient*>* pClients = GetClientList();

	if (!pClients)
		return nullptr;

	return pClients->Element(slot.Get());
}

uint32 GetSoundEventHash(const char* pszSoundEventName)
{
	return MurmurHash2LowerCase(pszSoundEventName, 0x53524332);
}

std::string StringToLower(std::string strValue)
{
	for (int i = 0; strValue[i]; i++)
		strValue[i] = tolower(strValue[i]);

	return strValue;
}

ISteamUGC* GetSteamUGC()
{
	if (g_pEngineServer2->IsDedicatedServer())
		return SteamGameServerUGC();
	else
		return SteamUGC();
}

ISteamHTTP* GetSteamHTTP()
{
	if (g_pEngineServer2->IsDedicatedServer())
		return SteamGameServerHTTP();
	else
		return SteamHTTP();
}

void* GetScriptFunction(const char* pszClassName, const char* pszFuncName)
{
	auto& entClassesByCPPClassname = g_pEntitySystem->m_entClassesByCPPClassname;
	auto nIndex = entClassesByCPPClassname.Find(pszClassName);
	if (nIndex != entClassesByCPPClassname.InvalidIndex())
	{
		// https://github.com/Wend4r/sourcesdk/blob/758e43823f02fcd40498d33a42cd93243258fe1e/public/vscript/ivscript.h#L280
		struct ScriptFunctionBindingCurrent_t
		{
			ScriptFuncDescriptor_t m_desc;
			ScriptClassDesc_t* m_pClassDesc;
			ScriptBindingFunc_t m_pfnBinding;
			void* m_pFunction;
			ScriptFuncBindingFlags_t m_flags;
		};

		const auto& functionBindings = *reinterpret_cast<CUtlVector<ScriptFunctionBindingCurrent_t>*>(&(reinterpret_cast<ScriptClassDesc_t*>(entClassesByCPPClassname.Element(nIndex)->m_pScriptDesc)->m_FunctionBindings));
		FOR_EACH_VEC(functionBindings, i)
		{
			auto& functionBinding = functionBindings.Element(i);
			if (V_strcmp(functionBinding.m_desc.m_pszScriptName, pszFuncName) != 0)
				continue;

			return functionBinding.m_pFunction;
		}
	}

	return nullptr;
}
