#ifndef NETMESSAGES_H
#define NETMESSAGES_H

#ifdef _WIN32
	#pragma once
#endif

#include <netmessages.pb.h>
#include <networkbasetypes.pb.h>
#include <networksystem/netmessage.h>
#include <networksystem_protomessages.pb.h>
#undef min
#undef max

class CNETMsg_Tick_t : public CNetMessagePB<CNETMsg_Tick>
{
};

class CNETMsg_StringCmd_t : public CNetMessagePB<CNETMsg_StringCmd>
{
};

class CNETMsg_SpawnGroup_LoadCompleted_t : public CNetMessagePB<CNETMsg_SpawnGroup_LoadCompleted>
{
};

class CCLCMsg_ClientInfo_t : public CNetMessagePB<CCLCMsg_ClientInfo>
{
};

class CCLCMsg_BaselineAck_t : public CNetMessagePB<CCLCMsg_BaselineAck>
{
};

class CCLCMsg_LoadingProgress_t : public CNetMessagePB<CCLCMsg_LoadingProgress>
{
};

class CCLCMsg_SplitPlayerConnect_t : public CNetMessagePB<CCLCMsg_SplitPlayerConnect>
{
};

class CCLCMsg_SplitPlayerDisconnect_t : public CNetMessagePB<CCLCMsg_SplitPlayerDisconnect>
{
};

class CCLCMsg_CmdKeyValues_t : public CNetMessagePB<CCLCMsg_CmdKeyValues>
{
};

class CCLCMsg_Move_t : public CNetMessagePB<CCLCMsg_Move>
{
};

class CCLCMsg_VoiceData_t : public CNetMessagePB<CCLCMsg_VoiceData>
{
};

class CCLCMsg_FileCRCCheck_t : public CNetMessagePB<CCLCMsg_FileCRCCheck>
{
};

class CCLCMsg_RespondCvarValue_t : public CNetMessagePB<CCLCMsg_RespondCvarValue>
{
};

class NetMessageSplitscreenUserChanged_t : public CNetMessagePB<NetMessageSplitscreenUserChanged>
{
};

class NetMessagePacketStart_t : public CNetMessagePB<NetMessagePacketStart>
{
};

class NetMessagePacketEnd_t : public CNetMessagePB<NetMessagePacketEnd>
{
};

class NetMessageConnectionClosed_t : public CNetMessagePB<NetMessageConnectionClosed>
{
};

class NetMessageConnectionCrashed_t : public CNetMessagePB<NetMessageConnectionCrashed>
{
};

#endif // NETMESSAGES_H
