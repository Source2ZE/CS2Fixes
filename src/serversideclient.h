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
#include <entity2/entityidentity.h>
#include <networksystem/inetworksystem.h>
#include <steam/steamclientpublic.h>
#include <tier1/utlstring.h>

#include <netmessages.pb.h>
#include <network_connection.pb.h>

class CHLTVServer;
class INetMessage;
class CNetworkGameServerBase;
class CNetworkGameServer;

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

// class CServerSideClientBase: CUtlSlot, INetworkChannelNotify, INetworkMessageProcessingPreFilter;
class CServerSideClientBase
{
	virtual void UnkDestructor() = 0;

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

	virtual void ExecuteStringCommand(const CNETMsg_StringCmd& msg) = 0;
	virtual void SendNetMessage(const CNetMessage* pData, NetChannelBufType_t bufType) = 0;

#ifdef LINUX
private:
	virtual void unk_17() = 0;
#endif

public:
	virtual void ClientPrintf(const char*, ...) = 0;

	bool IsConnected() const
	{
		return m_nSignonState >= SIGNONSTATE_CONNECTED;
	}
	bool IsSpawned() const
	{
		return m_nSignonState >= SIGNONSTATE_NEW;
	}
	bool IsActive() const
	{
		return m_nSignonState == SIGNONSTATE_FULL;
	}
	virtual bool IsFakeClient() const
	{
		return m_bFakePlayer;
	}
	virtual bool IsHLTV() = 0;

	// Is an actual human player or splitscreen player (not a bot and not a HLTV slot)
	virtual bool IsHumanPlayer() const
	{
		return false;
	}
	virtual bool IsHearingClient(CPlayerSlot nSlot) const
	{
		return false;
	}
	virtual bool IsLowViolenceClient() const
	{
		return m_bLowViolence;
	}

	virtual bool IsSplitScreenUser() const
	{
		return m_bSplitScreenUser;
	}
	int GetClientPlatform() const
	{
		return m_ClientPlatform;
	} // CrossPlayPlatform_t

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
	virtual bool ProcessFileCRCCheck(const CCLCMsg_FileCRCCheck_t& msg) = 0;
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
		UpdateAcknowledgedFramecount(-1);
	}

	virtual bool ShouldSendMessages() = 0;
	virtual void UpdateSendState() = 0;

	virtual const CMsgPlayerInfo& GetPlayerInfo() const
	{
		return m_playerInfo;
	}

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

	int GetSignonState() const
	{
		return m_nSignonState;
	}

	virtual void FreeBaselines() = 0;

	bool IsFullyAuthenticated(void)
	{
		return m_bFullyAuthenticated;
	}
	void SetFullyAuthenticated(void)
	{
		m_bFullyAuthenticated = true;
	}

	virtual CServerSideClientBase* GetSplitScreenOwner()
	{
		return m_pAttachedTo;
	}

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

public:
	[[maybe_unused]] void* m_pVT1;	   // INetworkMessageProcessingPreFilter
	CUtlString m_unk16;				   // 16
	[[maybe_unused]] char pad24[0x16]; // 24
#ifdef __linux__
	[[maybe_unused]] char pad46[0x10]; // 46
#endif
	void (*RebroadcastSource)(int msgID);		  // 64
	CUtlString m_UserIDString;					  // 72
	CUtlString m_Name;							  // 80
	CPlayerSlot m_nClientSlot;					  // 88
	CEntityIndex m_nEntityIndex;				  // 92
	CNetworkGameServerBase* m_Server;			  // 96
	INetChannel* m_NetChannel;					  // 104
	uint8 m_nUnkVariable;						  // 112
	bool m_bMarkedToKick;						  // 113
	int32 m_nSignonState;						  // 116
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
	CPlayerUserId m_UserID; // 184
	bool m_bReceivedPacket; // 186
	CSteamID m_SteamID;		// 187
	CSteamID m_UnkSteamID;	// 195
	CSteamID m_UnkSteamID2; // 203 from auth ticket
	CSteamID m_nFriendsID;	// 211
	ns_address m_nAddr;		// 220
	ns_address m_nAddr2;	// 252
	KeyValues* m_ConVars;	// 288
	bool m_bConVarsChanged; // 296
	bool m_bConVarsInited;	// 297
	bool m_bIsHLTV;			// 298
	bool m_bIsReplay;		// 299
	[[maybe_unused]] char pad29[0xA];
	uint32 m_nSendtableCRC;					  // 312
	int m_ClientPlatform;					  // 316
	int m_nSignonTick;						  // 320
	int m_nDeltaTick;						  // 324
	int m_UnkVariable3;						  // 328
	int m_nStringTableAckTick;				  // 332
	int m_UnkVariable4;						  // 336
	CFrameSnapshot* m_pLastSnapshot;		  // 344
	CUtlVector<void*> m_vecLoadedSpawnGroups; // 352
	CMsgPlayerInfo m_playerInfo;			  // 376
	CFrameSnapshot* m_pBaseline;			  // 432
	int m_nBaselineUpdateTick;				  // 440
	CBitVec<MAX_EDICTS> m_BaselinesSent;	  // 444
	int m_nBaselineUsed;					  // 2492
	int m_nLoadingProgress;					  // 2496
	int m_nForceWaitForTick;				  // 2500
	bool m_bLowViolence;					  // 2504
	bool m_bSomethingWithAddressType;		  // 2505
	bool m_bFullyAuthenticated;				  // 2506
	bool m_bUnkBool2507;					  // 2507
	float m_fNextMessageTime;				  // 2508
	float m_fSnapshotInterval;				  // 2512
	float m_fAuthenticatedTime;				  // 2516
	[[maybe_unused]] char pad168[0x124];	  // 2520
	[[maybe_unused]] char pad1658[0x24];	  // 2816 something in CServerSideClientBase::ExecuteStringCommand
	CNetworkStatTrace m_Trace;				  // 2848
	int m_spamCommandsCount;				  // 2888 if the value is greater than 16, the player will be kicked with reason 39
	double m_lastExecutedCommand;			  // 2896 if command executed more than once per second, ++m_spamCommandCount
};

class CServerSideClient : public CServerSideClientBase
{
public:
	virtual ~CServerSideClient() = 0;

public:
	CPlayerBitVec m_VoiceStreams;				   // 2904
	CPlayerBitVec m_VoiceProximity;				   // 2912
	CCheckTransmitInfo m_PackInfo;				   // 2920
	CClientFrameManager m_FrameManager;			   // 3520
	CClientFrame* m_pCurrentFrame;				   // 3808
	float m_flLastClientCommandQuotaStart;		   // 3816
	float m_flTimeClientBecameFullyConnected;	   // 3820
	bool m_bVoiceLoopback;						   // 3824
	int m_nHltvReplayDelay;						   // 3828
	CHLTVServer* m_pHltvReplayServer;			   // 3832
	int m_nHltvReplayStopAt;					   // 3840
	int m_nHltvReplayStartAt;					   // 3844
	int m_nHltvReplaySlowdownBeginAt;			   // 3848
	int m_nHltvReplaySlowdownEndAt;				   // 3852
	float m_flHltvReplaySlowdownRate;			   // 3856
	int m_nHltvLastSendTick;					   // 3860
	float m_flHltvLastReplayRequestTime;		   // 3864
	CUtlVector<INetMessage*> m_HltvQueuedMessages; // 3872
	HltvReplayStats_t m_HltvReplayStats;		   // 3896
	void* m_pLastJob;							   // 3952
};

// not full class reversed
class CHLTVClient : public CServerSideClientBase
{
public:
	virtual ~CHLTVClient() = 0;

public:
	CNetworkGameServerBase* m_pHLTV;	// 2904
	CUtlString m_szPassword;			// 2912
	CUtlString m_szChatGroup;			// 2920
	double m_fLastSendTime;				// 2928
	double m_flLastChatTime;			// 2936
	int m_nLastSendTick;				// 2944
	[[maybe_unused]] char pad2948[0x4]; // 2948
	int m_nFullFrameTime;				// 2952
	[[maybe_unused]] char pad2956[0x4]; // 2956
	[[maybe_unused]] char pad2960[0x4]; // 2960
	bool m_bNoChat;						// 2964
	bool m_bUnkBool;					// 2965
	bool m_bUnkBool2;					// 2966
	bool m_bUnkBool3;					// 2967
}; // sizeof 3008

#endif // SERVERSIDECLIENT_H
