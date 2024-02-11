#pragma once

#include "playerslot.h"
#include "steam/steamclientpublic.h"
#include "utlstring.h"

struct netadr
{
	int type;
	uint8 ip[4];
	uint16 port;
};

class CNetChan
{
public:
#ifndef WIN32
	virtual void unk_00() = 0;
#endif
	virtual void unk_01() = 0;
	virtual void unk_02() = 0;
	virtual void unk_03() = 0;
	virtual void unk_04() = 0;
	virtual void unk_05() = 0;
	virtual void unk_06() = 0;
	virtual void unk_07() = 0;
	virtual void unk_08() = 0;
	virtual void unk_09() = 0;
	virtual void unk_010() = 0;
	virtual void unk_011() = 0;
	virtual void unk_012() = 0;
	virtual void unk_013() = 0;
	virtual void unk_014() = 0;
	virtual void unk_015() = 0;
	virtual void unk_016() = 0;
	virtual void unk_017() = 0;
	virtual void unk_018() = 0;
	virtual void unk_019() = 0;
	virtual void unk_020() = 0;
	virtual void unk_021() = 0;
	virtual void unk_022() = 0;
	virtual void unk_023() = 0;
	virtual void unk_024() = 0;
	virtual void unk_025() = 0;
	virtual void unk_026() = 0;
	virtual void unk_027() = 0;
	virtual void unk_028() = 0;
	virtual void unk_029() = 0;
	virtual void unk_030() = 0;
	virtual void unk_031() = 0;
	virtual void unk_032() = 0;
	virtual void unk_033() = 0;
	virtual void unk_034() = 0;
	virtual void SendNetMessage(INetworkSerializable *a2, void *pData, int a4) = 0;
	virtual void unk_036() = 0;
	virtual void unk_037() = 0;
	virtual void unk_038() = 0;
	virtual void unk_039() = 0;
	virtual void unk_040() = 0;
	virtual netadr *GetRemoteAddress() = 0;
};

class CServerSideClient
{
public:
	virtual ~CServerSideClient() = 0;

	CNetChan *GetNetChannel() const { return m_NetChannel; }
	CPlayerSlot GetPlayerSlot() const { return m_nClientSlot; }
	CPlayerUserId GetUserID() const { return m_UserID; }
	int GetSignonState() const { return m_nSignonState; }
	CSteamID *GetClientSteamID() const { return (CSteamID*)&m_SteamID; }
	const char *GetClientName() const { return m_Name; }
	netadr *GetRemoteAddress() const { return (netadr*)&m_NetAdr; }

private:
	[[maybe_unused]] void *m_pVT1; // INetworkMessageProcessingPreFilter
	[[maybe_unused]] char pad1[0x40];
#ifdef __linux__
	[[maybe_unused]] char pad2[0x10];
#endif
	CNetChan *m_NetChannel; // 80 | 96
	[[maybe_unused]] char pad3[0x4];
	int32 m_nSignonState; // 92 | 108
	[[maybe_unused]] char pad4[0x38];
	CPlayerUserId m_UserID; // 152 | 168
	[[maybe_unused]] char pad5[0x1];
	CSteamID m_SteamID; // 155 | 171
	[[maybe_unused]] char pad6[0x15];
	CPlayerSlot m_nClientSlot; // 184 | 200
	[[maybe_unused]] char pad7[0x4];
	CUtlString m_Name; // 192 | 208
	[[maybe_unused]] char pad8[0x28];
	netadr m_NetAdr;
};
