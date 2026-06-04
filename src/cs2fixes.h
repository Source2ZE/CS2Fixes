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

#include "engine/igameeventsystem.h"
#include "entity/cgamerules.h"
#include "gamesystems/spawngroup_manager.h"
#include "igameevents.h"
#include "khook.hpp"
#include "networksystem/inetworkserializer.h"
#include "public/ics2fixes.h"
#include "steam/isteamhttp.h"
#include <ISmmPlugin.h>
#include <iplayerinfo.h>
#include <iserver.h>

#ifdef AMBUILD
	#include "version_gen.h"
#else
	#include "version_gen_placeholder.h"
#endif

extern IGameEventSystem* g_gameEventSystem;
extern IGameEventManager2* g_gameEventManager;
extern CGameEntitySystem* g_pEntitySystem;
extern IVEngineServer2* g_pEngineServer2;
extern CCSGameRules* g_pGameRules;
extern CSpawnGroupMgrGameSystem* g_pSpawnGroupMgr;
extern double g_flUniversalTime;
extern bool g_bRequiredInitLoaded;
extern INetworkGameServer* GetNetworkGameServer();
extern CGlobalVars* GetGlobals();
extern CConVar<bool> g_cvarDropMapWeapons;

class CS2Fixes : public ISmmPlugin, public IMetamodListener, public ICS2Fixes
{
public:
	bool Load(PluginId id, ISmmAPI* ismm, char* error, size_t maxlen, bool late);
	bool Unload(char* error, size_t maxlen);
	bool Pause(char* error, size_t maxlen);
	bool Unpause(char* error, size_t maxlen);
	void AllPluginsLoaded();
	void OnLevelInit(char const* pMapName,
					 char const* pMapEntities,
					 char const* pOldLevel,
					 char const* pLandmarkName,
					 bool loadGame,
					 bool background);
	void OnLevelShutdown();

public: // MetaMod API
	void* OnMetamodQuery(const char* iface, int* ret);
	std::uint64_t GetAdminFlags(std::uint64_t iSteam64ID) const override;
	bool SetAdminFlags(std::uint64_t iSteam64ID, std::uint64_t iFlags) override;
	int GetAdminImmunity(std::uint64_t iSteam64ID) const override;
	bool SetAdminImmunity(std::uint64_t iSteam64ID, std::uint32_t iImmunity) override;

public:
	const char* GetAuthor() { return PLUGIN_AUTHOR; }
	const char* GetName() { return PLUGIN_DISPLAY_NAME; }
	const char* GetDescription() { return PLUGIN_DESCRIPTION; }
	const char* GetURL() { return PLUGIN_URL; }
	const char* GetLicense() { return PLUGIN_LICENSE; }
	const char* GetVersion() { return PLUGIN_FULL_VERSION; }
	const char* GetDate() { return __DATE__; }
	const char* GetLogTag() { return PLUGIN_LOGTAG; }
};

extern CS2Fixes g_CS2Fixes;

PLUGIN_GLOBALVARS();
