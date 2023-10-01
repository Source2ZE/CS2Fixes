/**
 * vim: set ts=4 sw=4 tw=99 noet :
 * ======================================================
 * CS2Fixes
 * Written by xen and poggu
 * ======================================================
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * In no event will the authors be held liable for any damages arising from 
 * the use of this software.
 */

#pragma once

#include <ISmmPlugin.h>
#include <igameevents.h>
#include <iplayerinfo.h>
#include <sh_vector.h>
#include "networksystem/inetworkserializer.h"
#include <iserver.h>

class CS2Fixes : public ISmmPlugin, public IMetamodListener
{
public:
	bool Load(PluginId id, ISmmAPI *ismm, char *error, size_t maxlen, bool late);
	void Hook_PostEvent(CSplitScreenSlot nSlot, bool bLocalOnly, int nClientCount, const uint64* clients,
		INetworkSerializable* pEvent, const void* pData, unsigned long nSize, NetChannelBufType_t bufType);
	bool Unload(char *error, size_t maxlen);
	void Hook_StartupServer(const GameSessionConfiguration_t& config, ISource2WorldSession*, const char*);
	bool Pause(char *error, size_t maxlen);
	bool Unpause(char *error, size_t maxlen);
	void AllPluginsLoaded();
public: //hooks
	void OnLevelInit( char const *pMapName,
				 char const *pMapEntities,
				 char const *pOldLevel,
				 char const *pLandmarkName,
				 bool loadGame,
				 bool background );
	void OnLevelShutdown();
	void Hook_GameFrame( bool simulating, bool bFirstTick, bool bLastTick );
	void Hook_ClientActive( CPlayerSlot slot, bool bLoadGame, const char *pszName, uint64 xuid );
	void Hook_ClientDisconnect( CPlayerSlot slot, int reason, const char *pszName, uint64 xuid, const char *pszNetworkID );
	void Hook_ClientPutInServer( CPlayerSlot slot, char const *pszName, int type, uint64 xuid );
	void Hook_ClientSettingsChanged( CPlayerSlot slot );
	void Hook_OnClientConnected( CPlayerSlot slot, const char *pszName, uint64 xuid, const char *pszNetworkID, const char *pszAddress, bool bFakePlayer );
	bool Hook_ClientConnect( CPlayerSlot slot, const char *pszName, uint64 xuid, const char *pszNetworkID, bool unk1, CBufferString *pRejectReason );
	void Hook_ClientCommand( CPlayerSlot nSlot, const CCommand &_cmd );
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
