#pragma once

#include "playerslot.h"
#include "steam/steamclientpublic.h"
#include "utlstring.h"
#include "inetchannel.h"
#include "gameconfig.h"

class CServerSideClient
{
public:
	virtual ~CServerSideClient() = 0;

	INetChannel *GetNetChannel() const { return m_NetChannel; }
	CPlayerSlot GetPlayerSlot() const { return m_nClientSlot; }
	CPlayerUserId GetUserID() const { return m_UserID; }
	int GetSignonState() const { return m_nSignonState; }
	CSteamID *GetClientSteamID() const { return (CSteamID*)&m_SteamID; }
	const char *GetClientName() const { return m_Name.Get(); }
	netadr_t *GetRemoteAddress() const { return (netadr_t*)&m_NetAdr; }

	void ForceFullUpdate()
	{
		Message("Forcing full update on client %s\n", GetClientName());
		m_nDeltaTick = -1;
	}

private:
	[[maybe_unused]] void *m_pVT1; // INetworkMessageProcessingPreFilter
	[[maybe_unused]] char pad1[0x48];
#ifdef __linux__
	[[maybe_unused]] char pad2[0x10];
#endif
	INetChannel *m_NetChannel;		// 80 | 96
	[[maybe_unused]] char pad3[0x4];
	int32 m_nSignonState;			// 92 | 108
	[[maybe_unused]] char pad4[0x38];
	bool m_bFakePlayer;				// 152 | 168
	[[maybe_unused]] char pad5[0x7];
	CPlayerUserId m_UserID;			// 160 | 176
	[[maybe_unused]] char pad6[0x1];
	CSteamID m_SteamID;				// 163 | 179
	[[maybe_unused]] char pad7[0x25];
	CPlayerSlot m_nClientSlot;		// 208 | 224
	CEntityIndex m_nEntityIndex;	// 212 | 228
	CUtlString m_Name;				// 216 | 232
	[[maybe_unused]] char pad8[0x20];
	netadr_t m_NetAdr;				// 256 | 272
	[[maybe_unused]] char pad9[0x40];
	int m_nDeltaTick;				// 332 | 348
};

CServerSideClient *GetClientBySlot(CPlayerSlot slot);
