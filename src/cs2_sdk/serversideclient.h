#ifndef SERVERSIDECLIENT_H
#define SERVERSIDECLIENT_H

#if COMPILER_MSVC
	#pragma once
#endif

#include "clientframe.h"
#include "netmessages.h"

#include <inetchannel.h>
#include <playerslot.h>
// #include <protocol.h> // @Wend4r: use <netmessages.pb.h> instead.
#include "circularbuffer.h"
#include "networksystem/inetworksystem.h"
#include "threadtools.h"
#include "tier1/netadr.h"
#include <entity2/entityidentity.h>
#include <networksystem/inetworksystem.h>
#include <steam/steamclientpublic.h>
#include <tier1/utlstring.h>

#include <netmessages.pb.h>
#include <network_connection.pb.h>
#undef min
#undef max

class CHLTVServer;
class INetMessage;
class CNetworkGameServerBase;
class CNetworkGameServer;
class CUtlSlot;

struct HltvReplayStats_t
{
	enum FailEnum_t
	{
		FAILURE_ALREADY_IN_REPLAY,
		FAILURE_TOO_FREQUENT,
		FAILURE_NO_FRAME,
		FAILURE_NO_FRAME2,
		FAILURE_CANNOT_MATCH_DELAY,
		FAILURE_FRAME_NOT_READY,
		NUM_FAILURES
	};

	uint nClients;
	uint nStartRequests;
	uint nSuccessfulStarts;
	uint nStopRequests;
	uint nAbortStopRequests;
	uint nUserCancels;
	uint nFullReplays;
	uint nNetAbortReplays;
	uint nFailedReplays[NUM_FAILURES];
}; // sizeof 56

struct Spike_t
{
public:
	CUtlString m_szDesc;
	int m_nBits;
}; // sizeof 16

class CNetworkStatTrace
{
public:
	CUtlVector<Spike_t> m_Records;
	int m_nMinWarningBytes;
	int m_nStartBit;
	int m_nCurBit;
}; // sizeof 40

enum CopiedLockState_t : int32
{
	CLS_NOCOPY = 0,
	CLS_UNLOCKED = 1,
	CLS_LOCKED_BY_COPYING_THREAD = 2,
};

template <class MUTEX, CopiedLockState_t L = CLS_UNLOCKED>
class CCopyableLock : public MUTEX
{
	typedef MUTEX BaseClass;

public:
	// ...
};

class CUtlSignaller_Base
{
public:
	using Delegate_t = CUtlDelegate<void(CUtlSlot*)>;

	CUtlSignaller_Base(const Delegate_t& other) :
		m_SlotDeletionDelegate(other) {}
	CUtlSignaller_Base(Delegate_t&& other) :
		m_SlotDeletionDelegate(Move(other)) {}

private:
	Delegate_t m_SlotDeletionDelegate;
};

class CUtlSlot
{
public:
	using MTElement_t = CUtlSignaller_Base*;

	CUtlSlot() :
		m_ConnectedSignallers(0, 1) {}

private:
	CCopyableLock<CThreadFastMutex> m_Mutex;
	CUtlVector<MTElement_t> m_ConnectedSignallers;
};

class CServerSideClientBase : public CUtlSlot, public INetworkChannelNotify, public INetworkMessageProcessingPreFilter
{
public:
	virtual ~CServerSideClientBase() = 0;

public:
	CPlayerSlot GetPlayerSlot() const { return m_nClientSlot; }
	CPlayerUserId GetUserID() const { return m_UserID; }
	CEntityIndex GetEntityIndex() const { return m_nEntityIndex; }
	CSteamID GetClientSteamID() const { return m_SteamID; }
	const char* GetClientName() const { return m_Name; }
	INetChannel* GetNetChannel() const { return m_NetChannel; }
	const netadr_t* GetRemoteAddress() const { return &m_nAddr.GetAddress(); }
	CNetworkGameServerBase* GetServer() const { return m_Server; }

	virtual void Connect(int socket, const char* pszName, int nUserID, INetChannel* pNetChannel, bool bFakePlayer, bool bSplitClient, int iClientPlatform) = 0;
	virtual void Inactivate() = 0;
	virtual void Reactivate(CPlayerSlot nSlot) = 0;
	virtual void SetServer(CNetworkGameServer* pNetServer) = 0;
	virtual void Reconnect() = 0;
	virtual void Disconnect(ENetworkDisconnectionReason reason) = 0;
	virtual bool CheckConnect() = 0;

private:
	virtual void unk_10() = 0;

public:
	virtual void SetRate(int nRate) = 0;
	virtual void SetUpdateRate(float fUpdateRate) = 0;
	virtual int GetRate() = 0;

	virtual void Clear() = 0;

	virtual bool ExecuteStringCommand(const CNETMsg_StringCmd_t& msg) = 0; // "false" trigger an anti spam counter to kick a client.
	virtual void SendNetMessage(const CNetMessage* pData, NetChannelBufType_t bufType) = 0;

#ifdef LINUX
private:
	virtual void unk_17() = 0;
#endif

public:
	virtual void ClientPrintf(PRINTF_FORMAT_STRING const char*, ...) = 0;

	bool IsConnected() const { return m_nSignonState >= SIGNONSTATE_CONNECTED; }
	bool IsInGame() const { return m_nSignonState == SIGNONSTATE_FULL; }
	bool IsSpawned() const { return m_nSignonState >= SIGNONSTATE_NEW; }
	bool IsActive() const { return m_nSignonState == SIGNONSTATE_FULL; }
	virtual bool IsFakeClient() const { return m_bFakePlayer; }
	virtual bool IsHLTV() = 0;

	// Is an actual human player or splitscreen player (not a bot and not a HLTV slot)
	virtual bool IsHumanPlayer() const { return false; }
	virtual bool IsHearingClient(CPlayerSlot nSlot) const { return false; }
	virtual bool IsLowViolenceClient() const { return m_bLowViolence; }

	virtual bool IsSplitScreenUser() const { return m_bSplitScreenUser; }

public: // Message Handlers
	virtual bool ProcessTick(const CNETMsg_Tick_t& msg) = 0;
	virtual bool ProcessStringCmd(const CNETMsg_StringCmd_t& msg) = 0;

private:
	virtual bool unk_27() = 0;
	virtual bool unk_28() = 0;

public:
	virtual bool ProcessSpawnGroup_LoadCompleted(const CNETMsg_SpawnGroup_LoadCompleted_t& msg) = 0;
	virtual bool ProcessClientInfo(const CCLCMsg_ClientInfo_t& msg) = 0;
	virtual bool ProcessBaselineAck(const CCLCMsg_BaselineAck_t& msg) = 0;
	virtual bool ProcessLoadingProgress(const CCLCMsg_LoadingProgress_t& msg) = 0;
	virtual bool ProcessSplitPlayerConnect(const CCLCMsg_SplitPlayerConnect_t& msg) = 0;
	virtual bool ProcessSplitPlayerDisconnect(const CCLCMsg_SplitPlayerDisconnect_t& msg) = 0;
	virtual bool ProcessCmdKeyValues(const CCLCMsg_CmdKeyValues_t& msg) = 0;

private:
	virtual bool unk_36() = 0;
	virtual bool unk_37() = 0;

public:
	virtual bool ProcessMove(const CCLCMsg_Move_t& msg) = 0;
	virtual bool ProcessVoiceData(const CCLCMsg_VoiceData_t& msg) = 0;
	virtual bool ProcessRespondCvarValue(const CCLCMsg_RespondCvarValue_t& msg) = 0;

	virtual bool ProcessPacketStart(const NetMessagePacketStart_t& msg) = 0;
	virtual bool ProcessPacketEnd(const NetMessagePacketEnd_t& msg) = 0;
	virtual bool ProcessConnectionClosed(const NetMessageConnectionClosed_t& msg) = 0;
	virtual bool ProcessConnectionCrashed(const NetMessageConnectionCrashed_t& msg) = 0;

public:
	virtual bool ProcessChangeSplitscreenUser(const NetMessageSplitscreenUserChanged_t& msg) = 0;

private:
	virtual bool unk_47() = 0;
	virtual bool unk_48() = 0;
	virtual bool unk_49() = 0;

public:
	virtual void ConnectionStart(INetChannel* pNetChannel) = 0;

private: // SpawnGroup something.
	virtual void unk_51() = 0;
	virtual void unk_52() = 0;

public:
	virtual void ExecuteDelayedCall(void*) = 0;

	virtual bool UpdateAcknowledgedFramecount(int tick) = 0;
	void ForceFullUpdate()
	{
		// This seems to be wrong and crashes linux, plus I can't be bothered to check the vtable
		// UpdateAcknowledgedFramecount(-1);
		m_nDeltaTick = -1;
	}

	virtual bool ShouldSendMessages() = 0;
	virtual void UpdateSendState() = 0;

	virtual const CMsgPlayerInfo& GetPlayerInfo() const { return m_playerInfo; }

	virtual void UpdateUserSettings() = 0;
	virtual void ResetUserSettings() = 0;

private:
	virtual void unk_60() = 0;

public:
	virtual void SendSignonData() = 0;
	virtual void SpawnPlayer() = 0;
	virtual void ActivatePlayer() = 0;

	virtual void SetName(const char* name) = 0;
	virtual void SetUserCVar(const char* cvar, const char* value) = 0;

	SignonState_t GetSignonState() const { return m_nSignonState; }

	virtual void FreeBaselines() = 0;

	bool IsFullyAuthenticated(void) { return m_bFullyAuthenticated; }
	void SetFullyAuthenticated(void) { m_bFullyAuthenticated = true; }

	virtual CServerSideClientBase* GetSplitScreenOwner() { return m_pAttachedTo; }

	virtual int GetNumPlayers() = 0;

	virtual void ShouldReceiveStringTableUserData() = 0;

private:
	virtual void unk_70(CPlayerSlot nSlot) = 0;
	virtual void unk_71() = 0;
	virtual void unk_72() = 0;

public:
	virtual int GetHltvLastSendTick() = 0;

private:
	virtual void unk_74() = 0;
	virtual void unk_75() = 0;
	virtual void unk_76() = 0;

public:
	virtual void Await() = 0;

	virtual void MarkToKick() = 0;
	virtual void UnmarkToKick() = 0;

	virtual bool ProcessSignonStateMsg(int state) = 0;
	virtual void PerformDisconnection(ENetworkDisconnectionReason reason) = 0;

public:																				 // INetworkMessageProcessingPreFilter
	virtual bool FilterMessage(const CNetMessage* pData, INetChannel* pChannel) = 0; // "Client %d(%s) tried to send a RebroadcastSourceId msg.\n"

public:
	CUtlString m_UserIDString;					  // 72
	CUtlString m_Name;							  // 80
	CPlayerSlot m_nClientSlot;					  // 88
	CEntityIndex m_nEntityIndex;				  // 92
	CNetworkGameServerBase* m_Server;			  // 96
	INetChannel* m_NetChannel;					  // 104
	uint16 m_nConnectionTypeFlags;				  // 112
	bool m_bMarkedToKick;						  // 113
	SignonState_t m_nSignonState;				  // 116
	bool m_bSplitScreenUser;					  // 120
	bool m_bSplitAllowFastDisconnect;			  // 121
	int m_nSplitScreenPlayerSlot;				  // 124
	CServerSideClientBase* m_SplitScreenUsers[4]; // 128
	CServerSideClientBase* m_pAttachedTo;		  // 160
	bool m_bSplitPlayerDisconnecting;			  // 168
	int m_UnkVariable172;						  // 172
	bool m_bFakePlayer;							  // 176
	bool m_bSendingSnapshot;					  // 177
	[[maybe_unused]] char pad6[0x5];
	CPlayerUserId m_UserID = -1; // 184
	bool m_bReceivedPacket;		 // true, if client received a packet after the last send packet
	CSteamID m_SteamID;			 // 187
	CSteamID m_UnkSteamID;		 // 195
	CSteamID m_UnkSteamID2;		 // 203 from auth ticket
	CSteamID m_nFriendsID;		 // 211
	ns_address m_nAddr;			 // 220
	ns_address m_nAddr2;		 // 252
	KeyValues* m_ConVars;		 // 288
	bool m_bUnk0;

private:
	[[maybe_unused]] char pad273[0x28];

public:
	bool m_bConVarsChanged; // 296
	bool m_bIsHLTV;			// 298

private:
	[[maybe_unused]] char pad29[0xB];

public:
	uint32 m_nSendtableCRC; // 312
	uint32 m_uChallengeNumber;
	int m_nSignonTick;									   // 320
	int m_nDeltaTick;									   // 324
	int m_UnkVariable3;									   // 328
	int m_nStringTableAckTick;							   // 332
	int m_UnkVariable4;									   // 336
	CFrameSnapshot* m_pLastSnapshot;					   // last send snapshot
	CUtlVector<SpawnGroupHandle_t> m_vecLoadedSpawnGroups; // 352
	CMsgPlayerInfo m_playerInfo;						   // 376
	CFrameSnapshot* m_pBaseline;						   // 432
	int m_nBaselineUpdateTick;							   // 440
	CBitVec<MAX_EDICTS> m_BaselinesSent;				   // 444
	int m_nBaselineUsed;								   // 0/1 toggling flag, singaling client what baseline to use
	int m_nLoadingProgress;								   // 0..100 progress, only valid during loading

	// This is used when we send out a nodelta packet to put the client in a state where we wait
	// until we get an ack from them on this packet.
	// This is for 3 reasons:
	// 1. A client requesting a nodelta packet means they're screwed so no point in deluging them with data.
	//    Better to send the uncompressed data at a slow rate until we hear back from them (if at all).
	// 2. Since the nodelta packet deletes all client entities, we can't ever delta from a packet previous to it.
	// 3. It can eat up a lot of CPU on the server to keep building nodelta packets while waiting for
	//    a client to get back on its feet.
	int m_nForceWaitForTick = -1;

	CCircularBuffer m_UnkBuffer = {1024};	 // 2504 (24 bytes)
	bool m_bLowViolence = false;			 // true if client is in low-violence mode (L4D server needs to know)
	bool m_bSomethingWithAddressType = true; // 2529
	bool m_bFullyAuthenticated = false;		 // 2530
	bool m_bUnk1 = false;					 // 2531
	int m_nUnk;

	// The datagram is written to after every frame, but only cleared
	// when it is sent out to the client.  overflow is tolerated.

	// Time when we should send next world state update ( datagram )
	float m_fNextMessageTime = 0.0f;
	float m_fAuthenticatedTime = -1.0f;

	// Default time to wait for next message
	float m_fSnapshotInterval = 0.0f;

private:
	// CSVCMsg_PacketEntities_t m_packetmsg;  // 2552
	[[maybe_unused]] char pad2552[0x138]; // 2552

public:
	CNetworkStatTrace m_Trace;			// 2864
	int m_spamCommandsCount = 0;		// 2904 if the value is greater than 16, the player will be kicked with reason 39
	int m_unknown = 0;					// 2908
	double m_lastExecutedCommand = 0.0; // 2912 if command executed more than once per second, ++m_spamCommandCount

private:
	[[maybe_unused]] char pad2920[0x20]; // 2920
};

class CServerSideClient : public CServerSideClientBase
{
public:
	virtual ~CServerSideClient() = 0;

public:
	CPlayerBitVec m_VoiceStreams;		// 2952
	CPlayerBitVec m_VoiceProximity;		// 2960
	CCheckTransmitInfo m_PackInfo;		// 2968
	CClientFrameManager m_FrameManager; // 3568

private:
	[[maybe_unused]] char pad3856[8]; // 3856

public:
	float m_flLastClientCommandQuotaStart = 0.0f;	  // 3864
	float m_flTimeClientBecameFullyConnected = -1.0f; // 3868
	bool m_bVoiceLoopback = false;					  // 3872
	bool m_bUnk10 = false;							  // 3873
	int m_nHltvReplayDelay = 0;						  // 3876
	CHLTVServer* m_pHltvReplayServer;				  // 3880
	int m_nHltvReplayStopAt;						  // 3888
	int m_nHltvReplayStartAt;						  // 3892
	int m_nHltvReplaySlowdownBeginAt;				  // 3896
	int m_nHltvReplaySlowdownEndAt;					  // 3900
	float m_flHltvReplaySlowdownRate;				  // 3904
	int m_nHltvLastSendTick;						  // 3908
	float m_flHltvLastReplayRequestTime;			  // 3912
	CUtlVector<INetMessage*> m_HltvQueuedMessages;	  // 3920
	HltvReplayStats_t m_HltvReplayStats;			  // 3944
	void* m_pLastJob;								  // 4000

private:
	[[maybe_unused]] char pad3984[8]; // 4008
};

// not full class reversed
class CHLTVClient : public CServerSideClientBase
{
public:
	virtual ~CHLTVClient() = 0;

public:
	CNetworkGameServerBase* m_pHLTV; // 2960
	CUtlString m_szPassword;		 // 2968
	CUtlString m_szChatGroup;		 // 2976 // "all" or "group%d"
	double m_fLastSendTime = 0.0;	 // 2984
	double m_flLastChatTime = 0.0;	 // 2992
	int m_nLastSendTick = 0;		 // 2996
	int m_unknown2 = 0;				 // 3000
	int m_nFullFrameTime = 0;		 // 3008
	int m_unknown3 = 0;				 // 3012

public:
	bool m_bNoChat = false;	  // 3016
	bool m_bUnkBool = false;  // 3017
	bool m_bUnkBool2 = false; // 3018
	bool m_bUnkBool3 = false; // 3019

private:
	[[maybe_unused]] char pad3976[0x24]; // 3020
};

#endif // SERVERSIDECLIENT_H
