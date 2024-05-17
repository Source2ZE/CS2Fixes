/**
 * =============================================================================
 * CS2Fixes
 * Copyright (C) 2023-2024 Source2ZE
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

#include <ISmmPlugin.h>
#include "igameevents.h"
#include <iplayerinfo.h>
#include <sh_vector.h>
#include "networksystem/inetworkserializer.h"
#include <iserver.h>

class CS2Fixes : public ISmmPlugin, public IMetamodListener
{
public:
	bool Load(PluginId id, ISmmAPI *ismm, char *error, size_t maxlen, bool late);
	bool Unload(char *error, size_t maxlen);
	bool Pause(char *error, size_t maxlen);
	bool Unpause(char *error, size_t maxlen);
	void AllPluginsLoaded();
public: //hooks
	//void Hook_ClientDisconnect( CPlayerSlot slot, ENetworkDisconnectionReason reason, const char *pszName, uint64 xuid, const char *pszNetworkID );
	//void Hook_ClientPutInServer( CPlayerSlot slot, char const *pszName, int type, uint64 xuid );
	//void Hook_OnClientConnected( CPlayerSlot slot, const char *pszName, uint64 xuid, const char *pszNetworkID, const char *pszAddress, bool bFakePlayer );
	//bool Hook_ClientConnect( CPlayerSlot slot, const char *pszName, uint64 xuid, const char *pszNetworkID, bool unk1, CBufferString *pRejectReason );
	void CS2Fixes::Hook_ClientActive( CPlayerSlot slot, bool bLoadGame, const char *pszName, uint64 xuid );
	void Hook_StartupServer(const GameSessionConfiguration_t& config, ISource2WorldSession *pSession, const char *pszMapName);
	

public:
	const char *GetAuthor();
	const char *GetName();
	const char *GetDescription();
	const char *GetURL();
	const char *GetLicense();
	const char *GetVersion();
	const char *GetDate();
	const char *GetLogTag();
};

extern CS2Fixes g_CS2Fixes;

PLUGIN_GLOBALVARS();
