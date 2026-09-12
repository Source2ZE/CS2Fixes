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

#include "virtual_hooks.h"
#include "adminsystem.h"
#include "cchecktransmitinfo.h"
#include "cfgparser.h"
#include "commands.h"
#include "common.h"
#include "cs2fixes.h"
#include "cs_gameevents.pb.h"
#include "cstrike15_usermessages.pb.h"
#include "ctimer.h"
#include "entities.h"
#include "entity/ccsplayercontroller.h"
#include "entity/customhudlayout.h"
#include "entity/services.h"
#include "entitylistener.h"
#include "entitysystem.h"
#include "entwatch.h"
#include "eventlistener.h"
#include "gameconfig.h"
#include "gameevents.pb.h"
#include "icvar.h"
#include "idlemanager.h"
#include "iserver.h"
#include "leader.h"
#include "map_votes.h"
#include "mapmigrations.h"
#include "module.h"
#include "networkstringtabledefs.h"
#include "panoramavote.h"
#include "playermanager.h"
#include "serversideclient.h"
#include "tier0/vprof.h"
#include "usermessages.pb.h"
#include "votemanager.h"
#include "zombiereborn.h"

#include "tier0/memdbgon.h"

class GameSessionConfiguration_t
{};

KHook::Virtual<IServerGameDLL, void, bool, bool, bool> gameFrameHook(&IServerGameDLL::GameFrame, nullptr, Hook_GameFrame_Post);
KHook::Virtual<IServerGameDLL, void> gameServerSteamAPIActivatedHook(&IServerGameDLL::GameServerSteamAPIActivated, Hook_GameServerSteamAPIActivated, nullptr);
KHook::Virtual<IServerGameDLL, void, KeyValues*> applyGameSettingsHook(&IServerGameDLL::ApplyGameSettings, Hook_ApplyGameSettings, nullptr);
KHook::Virtual<IServerGameClients, void, CPlayerSlot, bool, const char*, uint64> clientActiveHook(&IServerGameClients::ClientActive, nullptr, Hook_ClientActive_Post);
KHook::Virtual<IServerGameClients, void, CPlayerSlot, ENetworkDisconnectionReason, const char*, uint64, const char*> clientDisconnectHook(&IServerGameClients::ClientDisconnect, nullptr, Hook_ClientDisconnect_Post);
KHook::Virtual<IServerGameClients, void, CPlayerSlot, const char*, int, uint64> clientPutInServerHook(&IServerGameClients::ClientPutInServer, nullptr, Hook_ClientPutInServer_Post);
KHook::Virtual<IServerGameClients, void, CPlayerSlot> clientSettingsChangedHook(&IServerGameClients::ClientSettingsChanged, Hook_ClientSettingsChanged, nullptr);
KHook::Virtual<IServerGameClients, void, CPlayerSlot, const char*, uint64, const char*, const char*, bool> onClientConnectedHook(&IServerGameClients::OnClientConnected, Hook_OnClientConnected, nullptr);
KHook::Virtual<IServerGameClients, bool, CPlayerSlot, const char*, uint64, const char*, bool, CBufferString*> clientConnectHook(&IServerGameClients::ClientConnect, Hook_ClientConnect, nullptr);
KHook::Virtual<IServerGameClients, void, CPlayerSlot, const CCommand&> clientCommandHook(&IServerGameClients::ClientCommand, Hook_ClientCommand, nullptr);
KHook::Virtual<IServerGameClients, void, CPlayerSlot, int, uint32, const void*> clientSvcUserMessageHook(&IServerGameClients::ClientSvcUserMessage, Hook_ClientSvcUserMessage, nullptr);
KHook::Virtual<IGameEventSystem, void, CSplitScreenSlot, bool, int, const uint64*, INetworkMessageInternal*, const CNetMessage*, unsigned long, NetChannelBufType_t> postEventAbstractHook(&IGameEventSystem::PostEventAbstract, Hook_PostEventAbstract, nullptr);
KHook::Virtual<INetworkServerService, void, const GameSessionConfiguration_t&, ISource2WorldSession*, const char*> startupServerHook(&INetworkServerService::StartupServer, nullptr, Hook_StartupServer_Post);
KHook::Virtual<ISource2GameEntities, void, CCheckTransmitInfo**, int, CBitVec<16384>&, CBitVec<16384>&, const Entity2Networkable_t**, const uint16*, int> checkTransmitHook(&ISource2GameEntities::CheckTransmit, nullptr, Hook_CheckTransmit_Post);
KHook::Virtual<ICvar, void, ConCommandRef, const CCommandContext&, const CCommand&> dispatchConCommandHook(&ICvar::DispatchConCommand, Hook_DispatchConCommand, nullptr);
KHook::Virtual<IGameTypes, void, const char*, const CUtlStringList&> createWorkshopMapGroupHook(0U, Hook_CreateWorkshopMapGroup, nullptr);
KHook::Virtual<IGameEventManager2, int, const char*, bool> loadEventsFromFileHook(&IGameEventManager2::LoadEventsFromFile, Hook_LoadEventsFromFile, nullptr);
KHook::Virtual<IGameEventManager2, bool, IGameEvent*, bool> fireEventHook(&IGameEventManager2::FireEvent, Hook_FireEvent, nullptr);
KHook::Virtual<CEntitySystem, void, int, const EntitySpawnInfo_t*> spawnHook(&CEntitySystem::Spawn, Hook_Spawn, nullptr);
KHook::Virtual<CServerSideClient, bool, const CCLCMsg_VoiceData_t&> processVoiceDataHook(&CServerSideClient::ProcessVoiceData, Hook_ProcessVoiceData, nullptr);
KHook::Virtual<INetworkGameServer, void, IGameSpawnGroupMgr*> setGameSpawnGroupMgrHook(&INetworkGameServer::SetGameSpawnGroupMgr, Hook_SetGameSpawnGroupMgr, nullptr);
KHook::Virtual<CVPhys2World, void, CUtlVector<TouchLinked_t>*, bool> getTouchingListHook(0U, nullptr, Hook_GetTouchingList_Post);
KHook::Virtual<CCSPlayer_MovementServices, void, double> checkMovingGroundHook(0U, Hook_CheckMovingGround, nullptr);
KHook::Virtual<CCSPlayer_WeaponServices, void, CBasePlayerWeapon*, Vector*, Vector*> dropWeaponHook(0U, nullptr, Hook_DropWeapon_Post);
KHook::Virtual<CGamePlayerEquip, void, InputData_t*> playerEquipUseHook(0U, Hook_PlayerEquipUse, nullptr);
KHook::Virtual<CGamePlayerEquip, void, CEntityPrecacheContext*> playerEquipPrecacheHook(0U, nullptr, Hook_PlayerEquipPrecache_Post);
KHook::Virtual<CTriggerGravity, void, CEntityPrecacheContext*> triggerGravityPrecacheHook(0U, nullptr, Hook_TriggerGravityPrecache_Post);
KHook::Virtual<CTriggerGravity, void, CBaseEntity*> triggerGravityEndTouchHook(0U, nullptr, Hook_TriggerGravityEndTouch_Post);
KHook::Virtual<CCSPlayerPawn, bool, CTakeDamageResult*> onTakeDamageAliveHook(0U, Hook_OnTakeDamage_Alive, nullptr);
KHook::Virtual<CCSPlayerPawn, void, const Vector*, const QAngle*, const Vector*> playerPawnTeleportHook(0U, Hook_CCSPlayerPawn_Teleport, Hook_CCSPlayerPawn_Teleport_Post);

IGameEventManager2* g_pCGameEventManagerVTable = nullptr;
CEntitySystem* g_pCEntitySystemVTable = nullptr;
CVPhys2World* g_pCVPhys2WorldVTable = nullptr;
CCSPlayer_MovementServices* g_pCCSPlayer_MovementServicesVTable = nullptr;
CCSPlayer_WeaponServices* g_pCCSPlayer_WeaponServicesVTable = nullptr;
CGamePlayerEquip* g_pCGamePlayerEquipVTable = nullptr;
CTriggerGravity* g_pTriggerGravityVTable = nullptr;
CCSPlayerPawn* g_pCCSPlayerPawnVTable = nullptr;
CServerSideClient* g_pCServerSideClientVTable = nullptr;

template <typename CLASS, typename RETURN, typename... ARGS>
bool SetupVirtualHook(KHook::Virtual<CLASS, RETURN, ARGS...>& hook, const char* name, CLASS* pInstance = nullptr)
{
	int offset = g_GameConfig->GetOffset(name);
	if (offset == -1)
	{
		Panic("Failed to find offset for %s\n", name);
		g_bRequiredInitLoaded = false;
		return false;
	}

	hook.Configure(offset);

	if (pInstance)
		hook.Add(pInstance);

	return true;
}

template <typename CLASS, typename RETURN, typename... ARGS>
void SetupGlobalVirtualHook(KHook::Virtual<CLASS, RETURN, ARGS...>& hook, CLASS*& pVTable, CModule* module, const char* className, const char* offsetName = nullptr)
{
	if (!pVTable)
		pVTable = (CLASS*)module->FindVirtualTable(className);

	if (!pVTable)
	{
		Panic("Failed to find %s vtable\n", className);
		g_bRequiredInitLoaded = false;
		return;
	}

	if (offsetName && !SetupVirtualHook(hook, offsetName))
		return;

	hook.AddGlobal((CLASS*)&pVTable);
}

void InitVirtualHooks()
{
	gameFrameHook.Add(g_pSource2Server);
	gameServerSteamAPIActivatedHook.Add(g_pSource2Server);
	applyGameSettingsHook.Add(g_pSource2Server);
	clientActiveHook.Add(g_pSource2GameClients);
	clientDisconnectHook.Add(g_pSource2GameClients);
	clientPutInServerHook.Add(g_pSource2GameClients);
	clientSettingsChangedHook.Add(g_pSource2GameClients);
	onClientConnectedHook.Add(g_pSource2GameClients);
	clientConnectHook.Add(g_pSource2GameClients);
	clientCommandHook.Add(g_pSource2GameClients);
	clientSvcUserMessageHook.Add(g_pSource2GameClients);
	postEventAbstractHook.Add(g_gameEventSystem);
	startupServerHook.Add(g_pNetworkServerService);
	checkTransmitHook.Add(g_pSource2GameEntities);
	dispatchConCommandHook.Add(g_pCVar);

	SetupVirtualHook(createWorkshopMapGroupHook, "IGameTypes_CreateWorkshopMapGroup", g_pGameTypes);

	SetupGlobalVirtualHook(loadEventsFromFileHook, g_pCGameEventManagerVTable, modules::server, "CGameEventManager");
	SetupGlobalVirtualHook(fireEventHook, g_pCGameEventManagerVTable, modules::server, "CGameEventManager");
	SetupGlobalVirtualHook(spawnHook, g_pCEntitySystemVTable, modules::server, "CGameEntitySystem");
	SetupGlobalVirtualHook(processVoiceDataHook, g_pCServerSideClientVTable, modules::engine, "CServerSideClient");
	SetupGlobalVirtualHook(getTouchingListHook, g_pCVPhys2WorldVTable, modules::vphysics2, "CVPhys2World", "CVPhys2World::GetTouchingList");
	SetupGlobalVirtualHook(checkMovingGroundHook, g_pCCSPlayer_MovementServicesVTable, modules::server, "CCSPlayer_MovementServices", "CCSPlayer_MovementServices::CheckMovingGround");
	SetupGlobalVirtualHook(dropWeaponHook, g_pCCSPlayer_WeaponServicesVTable, modules::server, "CCSPlayer_WeaponServices", "CCSPlayer_WeaponServices::DropWeapon");
	SetupGlobalVirtualHook(playerEquipUseHook, g_pCGamePlayerEquipVTable, modules::server, "CGamePlayerEquip", "CBaseEntity::Use");
	SetupGlobalVirtualHook(playerEquipPrecacheHook, g_pCGamePlayerEquipVTable, modules::server, "CGamePlayerEquip", "CBaseEntity::Precache");
	SetupGlobalVirtualHook(triggerGravityPrecacheHook, g_pTriggerGravityVTable, modules::server, "CTriggerGravity", "CBaseEntity::Precache");
	SetupGlobalVirtualHook(triggerGravityEndTouchHook, g_pTriggerGravityVTable, modules::server, "CTriggerGravity", "CBaseEntity::EndTouch");
	SetupGlobalVirtualHook(onTakeDamageAliveHook, g_pCCSPlayerPawnVTable, modules::server, "CCSPlayerPawn", "CCSPlayerPawn::OnTakeDamage_Alive");
	SetupGlobalVirtualHook(playerPawnTeleportHook, g_pCCSPlayerPawnVTable, modules::server, "CCSPlayerPawn", "Teleport");
}

void RemoveVirtualHooks()
{
	gameFrameHook.Remove(g_pSource2Server);
	gameServerSteamAPIActivatedHook.Remove(g_pSource2Server);
	applyGameSettingsHook.Remove(g_pSource2Server);
	clientActiveHook.Remove(g_pSource2GameClients);
	clientDisconnectHook.Remove(g_pSource2GameClients);
	clientPutInServerHook.Remove(g_pSource2GameClients);
	clientSettingsChangedHook.Remove(g_pSource2GameClients);
	onClientConnectedHook.Remove(g_pSource2GameClients);
	clientConnectHook.Remove(g_pSource2GameClients);
	clientCommandHook.Remove(g_pSource2GameClients);
	clientSvcUserMessageHook.Remove(g_pSource2GameClients);
	postEventAbstractHook.Remove(g_gameEventSystem);
	startupServerHook.Remove(g_pNetworkServerService);
	checkTransmitHook.Remove(g_pSource2GameEntities);
	dispatchConCommandHook.Remove(g_pCVar);
	loadEventsFromFileHook.RemoveGlobal((IGameEventManager2*)&g_pCGameEventManagerVTable);
	fireEventHook.RemoveGlobal((IGameEventManager2*)&g_pCGameEventManagerVTable);
	spawnHook.RemoveGlobal((CEntitySystem*)&g_pCEntitySystemVTable);
	processVoiceDataHook.RemoveGlobal((CServerSideClient*)&g_pCServerSideClientVTable);
	setGameSpawnGroupMgrHook.Remove(GetNetworkGameServer());
	createWorkshopMapGroupHook.Remove(g_pGameTypes);
	getTouchingListHook.RemoveGlobal((CVPhys2World*)&g_pCVPhys2WorldVTable);
	checkMovingGroundHook.RemoveGlobal((CCSPlayer_MovementServices*)&g_pCCSPlayer_MovementServicesVTable);
	dropWeaponHook.RemoveGlobal((CCSPlayer_WeaponServices*)&g_pCCSPlayer_WeaponServicesVTable);
	playerEquipUseHook.RemoveGlobal((CGamePlayerEquip*)&g_pCGamePlayerEquipVTable);
	playerEquipPrecacheHook.RemoveGlobal((CGamePlayerEquip*)&g_pCGamePlayerEquipVTable);
	triggerGravityPrecacheHook.RemoveGlobal((CTriggerGravity*)&g_pTriggerGravityVTable);
	triggerGravityEndTouchHook.RemoveGlobal((CTriggerGravity*)&g_pTriggerGravityVTable);
	onTakeDamageAliveHook.RemoveGlobal((CCSPlayerPawn*)&g_pCCSPlayerPawnVTable);
	playerPawnTeleportHook.RemoveGlobal((CCSPlayerPawn*)&g_pCCSPlayerPawnVTable);
}

KHook::Return<void> Hook_GameFrame_Post(IServerGameDLL* pThis, bool simulating, bool bFirstTick, bool bLastTick)
{
	/**
	 * simulating:
	 * ***********
	 * true  | game is ticking
	 * false | game is not ticking
	 */

	VPROF_BUDGET("CS2Fixes::Hook_GameFramePost", "CS2FixesPerFrame");

	if (!GetGlobals())
		return {KHook::Action::Ignore};

	if (simulating && g_bHasTicked)
		g_flUniversalTime += GetGlobals()->curtime - g_flLastTickedTime;

	g_flLastTickedTime = GetGlobals()->curtime;
	g_bHasTicked = true;

	RunTimers();
	EntityHandler_OnGameFramePost(simulating, GetGlobals()->tickcount);

	return {KHook::Action::Ignore};
}

KHook::Return<void> Hook_GameServerSteamAPIActivated(IServerGameDLL* pThis)
{
	g_playerManager->OnSteamAPIActivated();

	if (g_cvarVoteManagerEnable.Get() && !g_pMapVoteSystem->IsMapListLoaded())
		g_pMapVoteSystem->LoadMapList();

	return {KHook::Action::Ignore};
}

KHook::Return<void> Hook_ApplyGameSettings(IServerGameDLL* pThis, KeyValues* pKV)
{
	const char* pszMapName;
	uint64 iWorkshopId;

	if (pKV->FindKey("launchoptions") && pKV->FindKey("launchoptions")->FindKey("levelname"))
		pszMapName = pKV->FindKey("launchoptions")->GetString("levelname");
	else
		pszMapName = "";

	if (pKV->FindKey("launchoptions") && pKV->FindKey("launchoptions")->FindKey("customgamemode"))
		iWorkshopId = pKV->FindKey("launchoptions")->GetUint64("customgamemode");
	else
		iWorkshopId = 0;

	g_pCfgParser->ApplyGameSettings(pszMapName);
	g_pMapVoteSystem->ApplyGameSettings(pszMapName, iWorkshopId);
	g_pMapMigrations->ApplyGameSettings(iWorkshopId);

	return {KHook::Action::Ignore};
}

KHook::Return<void> Hook_ClientActive_Post(IServerGameClients* pThis, CPlayerSlot slot, bool bLoadGame, const char* pszName, uint64 xuid)
{
	Message("Hook_ClientActive(%d, %d, \"%s\", %lli)\n", slot, bLoadGame, pszName, xuid);

	return {KHook::Action::Ignore};
}

KHook::Return<void> Hook_ClientDisconnect_Post(IServerGameClients* pThis, CPlayerSlot slot, ENetworkDisconnectionReason reason, const char* pszName, uint64 xuid, const char* pszNetworkID)
{
	Message("Hook_ClientDisconnect(%d, %d, \"%s\", %lli)\n", slot, reason, pszName, xuid);

	CCSPlayerController* player = CCSPlayerController::FromSlot(slot);

	if (g_cvarEnableZR.Get())
	{
		// Controller team num is not valid post-disconnect, so just check both teams
		if (!ZR_CheckTeamWinConditions(CS_TEAM_T))
			ZR_CheckTeamWinConditions(CS_TEAM_CT);
	}

	ZEPlayer* pPlayer = g_playerManager->GetPlayer(slot);

	if (!pPlayer)
		return {KHook::Action::Ignore};

	// Dont add to c_listdc clients that are downloading MultiAddonManager stuff or were present during a map change
	if (reason != NETWORK_DISCONNECT_LOOPSHUTDOWN && reason != NETWORK_DISCONNECT_SHUTDOWN)
		g_pAdminSystem->AddDisconnectedPlayer(pszName, xuid, pPlayer ? pPlayer->GetIpAddress() : "");

	g_playerManager->OnClientDisconnect(slot);

	return {KHook::Action::Ignore};
}

KHook::Return<void> Hook_ClientPutInServer_Post(IServerGameClients* pThis, CPlayerSlot slot, const char* pszName, int type, uint64 xuid)
{
	Message("Hook_ClientPutInServer(%d, \"%s\", %d, %d, %lli)\n", slot, pszName, type, xuid);

	if (!g_playerManager->GetPlayer(slot))
		return {KHook::Action::Ignore};

	g_playerManager->OnClientPutInServer(slot);

	if (g_cvarEnableZR.Get())
		ZR_Hook_ClientPutInServer(slot, pszName, type, xuid);

	return {KHook::Action::Ignore};
}

KHook::Return<void> Hook_ClientSettingsChanged(IServerGameClients* pThis, CPlayerSlot slot)
{
#ifdef _DEBUG
	Message("Hook_ClientSettingsChanged(%d)\n", slot);
#endif

	return {KHook::Action::Ignore};
}

KHook::Return<void> Hook_OnClientConnected(IServerGameClients* pThis, CPlayerSlot slot, const char* pszName, uint64 xuid, const char* pszNetworkID, const char* pszAddress, bool bFakePlayer)
{
	Message("Hook_OnClientConnected(%d, \"%s\", %lli, \"%s\", \"%s\", %d)\n", slot, pszName, xuid, pszNetworkID, pszAddress, bFakePlayer);

	static ConVarRefAbstract tv_name("tv_name");
	const char* pszTvName = tv_name.GetString().Get();

	// Ideally we would use CServerSideClient::IsHLTV().. but it doesn't work :(
	if (bFakePlayer && V_strcmp(pszName, pszTvName))
		g_playerManager->OnBotConnected(slot);

	return {KHook::Action::Ignore};
}

KHook::Return<bool> Hook_ClientConnect(IServerGameClients* pThis, CPlayerSlot slot, const char* pszName, uint64 xuid, const char* pszNetworkID, bool unk1, CBufferString* pRejectReason)
{
	Message("Hook_ClientConnect(%d, \"%s\", %lli, \"%s\", %d, \"%s\")\n", slot, pszName, xuid, pszNetworkID, unk1, pRejectReason->Get());

	// Player is banned
	if (!g_playerManager->OnClientConnected(slot, xuid, pszNetworkID))
		return {KHook::Action::Supersede, false};

	return {KHook::Action::Ignore};
}

KHook::Return<void> Hook_ClientCommand(IServerGameClients* pThis, CPlayerSlot slot, const CCommand& args)
{
#ifdef _DEBUG
	Message("Hook_ClientCommand(%d, \"%s\")\n", slot, args.GetCommandString());
#endif

	ZEPlayer* pPlayer = g_playerManager->GetPlayer(slot);

	if (g_cvarIdleKickTime.Get() > 0.0f)
	{
		CCSPlayerController* pController = CCSPlayerController::FromSlot(slot);
		if (pPlayer && pController)
		{
			// Only spectators doing spectator commands reset idle timer
			if (pController->m_iTeamNum() == CS_TEAM_SPECTATOR && (!V_stricmp(args[0], "spec_mode") || !V_stricmp(args[0], "spec_prev") || !V_stricmp(args[0], "spec_next")))
				pPlayer->UpdateLastInputTime();
		}
	}

	if (g_cvarVoteManagerEnable.Get() && V_stricmp(args[0], "endmatch_votenextmap") == 0 && args.ArgC() == 2)
	{
		if (g_pMapVoteSystem->RegisterPlayerVote(slot, atoi(args[1])))
			return {KHook::Action::Ignore};
		else
			return {KHook::Action::Supersede};
	}

	if (g_cvarEnableZR.Get() && slot != -1 && !V_strnicmp(args.Arg(0), "jointeam", 8))
	{
		ZR_Hook_ClientCommand_JoinTeam(slot, args);
		return {KHook::Action::Supersede};
	}

	return {KHook::Action::Ignore};
}

KHook::Return<void> Hook_ClientSvcUserMessage(IServerGameClients* pThis, CPlayerSlot slot, int um_type, uint32 size, const void* buf)
{
	auto pController = CCSPlayerController::FromSlot(slot);

	if (!pController)
		return {KHook::Action::Ignore};

	if (um_type == CS_UM_CustomHudClicked)
	{
		CCSUsrMsg_CustomHudClicked message;

		if (message.ParseFromArray(buf, size))
		{
			CHandle<CCSCustomHudLayout> hLayout = CBaseHandle::FromPackedInt(message.custom_hud_layout());

			if (hLayout.Get())
				hLayout->OnClick(pController, message.button_id());
		}
	}

	return {KHook::Action::Ignore};
}

CConVar<bool> g_cvarBlockParticleMsgs("cs2f_block_particle_msgs", FCVAR_NONE, "Whether to block CUserMsg_ParticleManager messages to fix lag/crashes, experimental", false);

KHook::Return<void> Hook_PostEventAbstract(IGameEventSystem* pThis, CSplitScreenSlot nSlot, bool bLocalOnly, int nClientCount, const uint64* clients,
										   INetworkMessageInternal* pEvent, const CNetMessage* pData, unsigned long nSize, NetChannelBufType_t bufType)
{
	// Message( "Hook_PostEvent(%d, %d, %d, %lli)\n", nSlot, bLocalOnly, nClientCount, clients );
	NetMessageInfo_t* info = pEvent->GetNetMessageInfo();

	if (g_cvarEnableStopSound.Get() && info->m_MessageId == GE_FireBulletsId)
	{
		if (g_playerManager->GetSilenceSoundMask())
		{
			// Post the silenced sound to those who use silencesound
			// Creating a new event object requires us to include the protobuf c files which I didn't feel like doing yet
			// So instead just edit the event in place and reset later
			auto msg = const_cast<CNetMessage*>(pData)->ToPB<CMsgTEFireBullets>();

			int32_t weapon_id = msg->weapon_id();
			int32_t sound_type = msg->sound_type();
			int32_t item_def_index = msg->item_def_index();

			// original weapon_id will override new settings if not removed
			msg->set_weapon_id(0);
			msg->set_sound_type(9);
			msg->set_item_def_index(61); // weapon_usp_silencer

			uint64 clientMask = *(uint64*)clients & g_playerManager->GetSilenceSoundMask();

			postEventAbstractHook.CallOriginal(pThis, nSlot, bLocalOnly, nClientCount, &clientMask, pEvent, msg, nSize, bufType);

			msg->set_weapon_id(weapon_id);
			msg->set_sound_type(sound_type);
			msg->set_item_def_index(item_def_index);
		}

		// Filter out people using stop/silence sound from the original event
		*(uint64*)clients &= ~g_playerManager->GetStopSoundMask();
		*(uint64*)clients &= ~g_playerManager->GetSilenceSoundMask();
	}
	else if (info->m_MessageId == GE_PlaceDecalEvent)
	{
		*(uint64*)clients &= ~g_playerManager->GetStopDecalsMask();
	}
	else if (info->m_MessageId == GE_Source1LegacyGameEvent)
	{
		if (g_cvarEnableLeader.Get())
			Leader_PostEventAbstract_Source1LegacyGameEvent(clients, pData);
	}
	else if (info->m_MessageId == UM_Shake)
	{
		auto pPBData = const_cast<CNetMessage*>(pData)->ToPB<CUserMessageShake>();
		if (g_cvarMaxShakeAmp.Get() >= 0 && pPBData->amplitude() > g_cvarMaxShakeAmp.Get())
			pPBData->set_amplitude(g_cvarMaxShakeAmp.Get());

		// remove client with noshake from the event
		if (g_cvarEnableNoShake.Get())
			*(uint64*)clients &= ~g_playerManager->GetNoShakeMask();
	}
	else if (info->m_MessageId == GE_SosStartSoundEvent)
	{
		auto msg = const_cast<CNetMessage*>(pData)->ToPB<CMsgSosStartSoundEvent>();

		if (g_cvarEnableZR.Get())
			ZR_PostEventAbstract_SosStartSoundEvent(clients, msg);

		if (g_cvarEnableStopSound.Get())
		{
			static std::set<uint32> soundEventHashes;

			ExecuteOnce(
				soundEventHashes.insert(GetSoundEventHash("Weapon_sg556.ZoomIn"));
				soundEventHashes.insert(GetSoundEventHash("Weapon_sg556.ZoomOut"));
				soundEventHashes.insert(GetSoundEventHash("Weapon_AUG.ZoomIn"));
				soundEventHashes.insert(GetSoundEventHash("Weapon_AUG.ZoomOut"));
				soundEventHashes.insert(GetSoundEventHash("Weapon_SSG08.Zoom"));
				soundEventHashes.insert(GetSoundEventHash("Weapon_SSG08.ZoomOut"));
				soundEventHashes.insert(GetSoundEventHash("Weapon_SCAR20.Zoom"));
				soundEventHashes.insert(GetSoundEventHash("Weapon_SCAR20.ZoomOut"));
				soundEventHashes.insert(GetSoundEventHash("Weapon_G3SG1.Zoom"));
				soundEventHashes.insert(GetSoundEventHash("Weapon_G3SG1.ZoomOut"));
				soundEventHashes.insert(GetSoundEventHash("Weapon_AWP.Zoom"));
				soundEventHashes.insert(GetSoundEventHash("Weapon_AWP.ZoomOut"));
				soundEventHashes.insert(GetSoundEventHash("Weapon_Revolver.Prepare"));
				soundEventHashes.insert(GetSoundEventHash("Weapon.AutoSemiAutoSwitch")););

			if (!soundEventHashes.contains(msg->soundevent_hash()))
				return {KHook::Action::Ignore};

			uint64 stopSoundMask = g_playerManager->GetStopSoundMask();
			uint64 silenceSoundMask = g_playerManager->GetSilenceSoundMask();

			if (!msg->has_source_entity_index())
				return {KHook::Action::Ignore};

			CBaseEntity* pSourceEntity = (CBaseEntity*)g_pEntitySystem->GetEntityInstance(CEntityIndex(msg->source_entity_index()));
			int playerSlot = -1;

			if (!pSourceEntity)
				return {KHook::Action::Ignore};

			if (pSourceEntity->IsPawn() && ((CCSPlayerPawn*)pSourceEntity)->GetController())
			{
				playerSlot = ((CCSPlayerPawn*)pSourceEntity)->GetController()->GetPlayerSlot();
			}
			else if (!V_strncasecmp(pSourceEntity->GetClassname(), "weapon_", 7))
			{
				CCSPlayerPawn* pPawn = (CCSPlayerPawn*)pSourceEntity->m_hOwnerEntity().Get();

				if (pPawn && pPawn->IsPawn() && pPawn->GetController())
					playerSlot = pPawn->GetController()->GetPlayerSlot();
			}

			// Remove player who triggered this sound from masks
			// Because some of these sounds never get played locally (Zoom's, Knife Hit/Stab)
			if (playerSlot != -1 && g_playerManager->IsPlayerUsingStopSound(playerSlot))
				stopSoundMask &= ~((uint64)1 << playerSlot);

			if (playerSlot != -1 && g_playerManager->IsPlayerUsingSilenceSound(playerSlot))
				silenceSoundMask &= ~((uint64)1 << playerSlot);

			// Filter out people using stop/silence sound from hearing this sound from other players
			*(uint64*)clients &= ~stopSoundMask;
			*(uint64*)clients &= ~silenceSoundMask;
		}
	}
	else if (info->m_MessageId == UM_ParticleManager)
	{
		// These messages were previously unused, but recently started being used for weapon particles in the AG2 update
		// Unfortunately, this new system seems extremely unoptimized for 64 players, and was causing severe performance issues & vector overflow client crashes
		if (g_cvarBlockParticleMsgs.Get())
			*(uint64*)clients = 0;
	}

	return {KHook::Action::Ignore};
}

CConVar<CUtlString> g_cvarMotdUrl("cs2f_motd_url", FCVAR_NONE, "Server MOTD URL, shows up as a \"Server Website\" button in scoreboard", "");

KHook::Return<void> Hook_StartupServer_Post(INetworkServerService* pThis, const GameSessionConfiguration_t& config, ISource2WorldSession* pSession, const char* pszMapName)
{
	g_pEntitySystem = GameEntitySystem();
	g_pEntitySystem->AddListenerEntity(g_pEntityListener);

	if (GetNetworkGameServer())
		setGameSpawnGroupMgrHook.Add(GetNetworkGameServer());

	Message("Hook_StartupServer: %s\n", pszMapName);

	RegisterEventListeners();

	if (g_bHasTicked)
		RemoveTimers(TIMERFLAG_MAP);

	g_bHasTicked = false;

	g_pPanoramaVoteHandler->Reset();
	g_pVoteManager->VoteManager_Init();
	g_pIdleSystem->Reset();

	INetworkStringTable* pInfoPanelTable = g_pNetworkStringTableServer->FindTable("InfoPanel");

	if (pInfoPanelTable && V_strcmp(g_cvarMotdUrl.Get(), ""))
	{
		SetStringUserDataRequest_t pUserData;
		pUserData.m_pRawData = (void*)g_cvarMotdUrl.Get().Get();
		pUserData.m_cbDataSize = g_cvarMotdUrl.Get().Length() + 1;

		pInfoPanelTable->AddString(true, "motd", &pUserData);
	}

	return {KHook::Action::Ignore};
}

KHook::Return<void> Hook_CheckTransmit_Post(ISource2GameEntities* pThis, CCheckTransmitInfo** ppInfoList, int infoCount, CBitVec<16384>& unionTransmitEdicts,
											CBitVec<16384>&, const Entity2Networkable_t** pNetworkables, const uint16* pEntityIndicies, int nEntities)
{
	if (!g_pEntitySystem || !GetGlobals())
		return {KHook::Action::Ignore};

	VPROF("CS2Fixes::Hook_CheckTransmit");

	for (int i = 0; i < infoCount; i++)
	{
		auto& pInfo = (CCheckTransmitInfoExtended*&)(ppInfoList[i]);
		CCSPlayerController* pSelfController = CCSPlayerController::FromSlot(pInfo->m_nPlayerSlot);

		if (!pSelfController || !pSelfController->IsConnected())
			continue;

		auto pSelfZEPlayer = g_playerManager->GetPlayer(pInfo->m_nPlayerSlot);

		if (!pSelfZEPlayer)
			continue;

		for (int j = 0; j < GetGlobals()->maxClients; j++)
		{
			CCSPlayerController* pController = CCSPlayerController::FromSlot(j);
			// Always transmit to themselves
			if (!pController || pController->m_bIsHLTV || j == pInfo->m_nPlayerSlot.Get())
				continue;

			// Don't transmit other players' flashlights
			CBarnLight* pFlashLight = pController->IsConnected() ? g_playerManager->GetPlayer(j)->GetFlashLight() : nullptr;

			if (!g_cvarFlashLightTransmitOthers.Get() && pFlashLight)
				pInfo->m_pTransmitEntity->Clear(pFlashLight->entindex());

			if (g_cvarEnableEntWatch.Get() && g_pEWHandler->IsConfigLoaded())
			{
				// Don't transmit other players' entwatch hud
				CPointWorldText* pHud = pController->IsConnected() ? g_playerManager->GetPlayer(j)->GetEntwatchHud() : nullptr;
				if (pHud)
					pInfo->m_pTransmitEntity->Clear(pHud->entindex());
			}

			// Always transmit other players if spectating
			if (!g_cvarEnableHide.Get() || pSelfController->GetPawnState() == STATE_OBSERVER_MODE)
				continue;

			// Get the actual pawn as the player could be currently spectating
			CCSPlayerPawn* pPawn = pController->GetPlayerPawn();

			if (!pPawn)
				continue;

			// Do not hide leaders or item holders to other players
			ZEPlayer* pOtherZEPlayer = g_playerManager->GetPlayer(j);
			if (pSelfZEPlayer->ShouldBlockTransmit(j) && pOtherZEPlayer && !pOtherZEPlayer->IsLeader() && g_pEWHandler->FindItemInstanceByOwner(j, false, 0) == -1)
			{
				pInfo->m_pTransmitEntity->Clear(pPawn->entindex());
				pInfo->m_pTransmitNonPlayers->Set(pPawn->entindex());
			}
		}

		// Don't transmit glow model to it's owner
		CBaseModelEntity* pGlowModel = pSelfZEPlayer->GetGlowModel();

		if (pGlowModel)
			pInfo->m_pTransmitEntity->Clear(pGlowModel->entindex());
	}

	return {KHook::Action::Ignore};
}

KHook::Return<void> Hook_DispatchConCommand(ICvar* pThis, ConCommandRef cmdHandle, const CCommandContext& ctx, const CCommand& args)
{
	VPROF_BUDGET("CS2Fixes::Hook_DispatchConCommand", "ConCommands");

	if (!g_pEntitySystem)
		return {KHook::Action::Ignore};

	auto iCommandPlayerSlot = ctx.GetPlayerSlot();

	if (!g_cvarEnableCommands.Get())
		return {KHook::Action::Ignore};

	bool bSay = !V_stricmp(args.Arg(0), "say");
	bool bTeamSay = !V_stricmp(args.Arg(0), "say_team");

	if (iCommandPlayerSlot != -1 && (bSay || bTeamSay))
	{
		CCSPlayerController* pController = CCSPlayerController::FromSlot(iCommandPlayerSlot);
		ZEPlayer* pPlayer = pController ? pController->GetZEPlayer() : nullptr;

		// Block chat messages from players not fully ingame, can be interpreted as console messages
		if (!pPlayer || !pPlayer->IsInGame())
		{
			Message("Blocked chat message from user ID %i not fully in game\n", g_pEngineServer2->GetPlayerUserId(iCommandPlayerSlot).Get());
			return {KHook::Action::Supersede};
		}

		bool bGagged = pPlayer->IsGagged();
		bool bFlooding = pPlayer->IsFlooding();
		bool bIsAdmin = pPlayer->IsAdminFlagSet(ADMFLAG_GENERIC);
		bool bAdminChat = bTeamSay && *args[1] == '@';
		bool bSilent = *args[1] == '/' || bAdminChat;
		bool bCommand = *args[1] == '!' || *args[1] == '/';

		// Chat messages should generate events regardless
		if (pController)
		{
			IGameEvent* pEvent = g_gameEventManager->CreateEvent("player_chat");

			if (pEvent)
			{
				pEvent->SetBool("teamonly", bTeamSay);
				pEvent->SetInt("userid", pController->GetPlayerSlot());
				pEvent->SetString("text", args[1]);

				g_gameEventManager->FireEvent(pEvent, true);
			}
		}

		if (!bGagged && !bSilent && !bFlooding)
		{
			dispatchConCommandHook.CallOriginal(pThis, cmdHandle, ctx, args);

			// Reset idle time if message is sent to chat
			if (g_cvarIdleKickTime.Get() > 0.0f)
				pPlayer->UpdateLastInputTime();
		}
		else if (bFlooding)
		{
			if (pController)
				ClientPrint(pController, HUD_PRINTTALK, CHAT_PREFIX "You are flooding the server!");
		}
		else if (bAdminChat && GetGlobals()) // Admin chat can be sent by anyone but only seen by admins, use flood protection here too
		{
			// HACK: At this point, we can safely modify the arg buffer as it won't be passed anywhere else
			// The string here is originally ("@foo bar"), trim it to be (foo bar)
			char* pszMessage = (char*)(args.ArgS() + 2);
			pszMessage[V_strlen(pszMessage) - 1] = 0;

			for (int i = 0; i < GetGlobals()->maxClients; i++)
			{
				ZEPlayer* pPlayer = g_playerManager->GetPlayer(i);

				if (!pPlayer)
					continue;

				if (i == iCommandPlayerSlot.Get() || pPlayer->IsAdminFlagSet(ADMFLAG_GENERIC))
					ClientPrint(CCSPlayerController::FromSlot(i), HUD_PRINTTALK, " \4(%sADMINS) %s:\6 %s", bIsAdmin ? "" : "TO ", pController->GetPlayerName().c_str(), pszMessage);
			}
		}

		// Finally, run the chat command if it is one, so anything will print after the player's message
		if (bCommand)
		{
			char* pszMessage = (char*)(args.ArgS() + 1);

			if (pszMessage[0] == '"' || pszMessage[0] == '!' || pszMessage[0] == '/')
				pszMessage += 1;

			// Host_Say at some point removes the trailing " for whatever reason, so we only remove if it was never called
			if ((bGagged || bSilent || bFlooding) && pszMessage[V_strlen(pszMessage) - 1] == '"')
				pszMessage[V_strlen(pszMessage) - 1] = '\0';

			ParseChatCommand(pszMessage, pController);
		}

		return {KHook::Action::Supersede};
	}

	return {KHook::Action::Ignore};
}

KHook::Return<void> Hook_CreateWorkshopMapGroup(IGameTypes* pThis, const char* name, const CUtlStringList& mapList)
{
	if (g_cvarVoteManagerEnable.Get() && g_pMapVoteSystem->IsMapListLoaded())
		return KHook::Recall<void (IGameTypes::*)(const char*, const CUtlStringList&)>(nullptr, {KHook::Action::Ignore}, pThis, name, g_pMapVoteSystem->CreateWorkshopMapGroup());

	return {KHook::Action::Ignore};
}

KHook::Return<int> Hook_LoadEventsFromFile(IGameEventManager2* pThis, const char* filename, bool bSearchAll)
{
	ExecuteOnce(g_gameEventManager = pThis);

	return {KHook::Action::Ignore};
}

KHook::Return<bool> Hook_FireEvent(IGameEventManager2* pThis, IGameEvent* pEvent, bool bDontBroadcast)
{
	// Make player_connect obey cs2f_map_steamids_enable as well
	if (!g_cvarEnableMapSteamIds.Get() && !V_stricmp(pEvent->GetName(), "player_connect"))
	{
		pEvent->SetString("networkid", "");
		pEvent->SetUint64("xuid", 0);
	}

	return {KHook::Action::Ignore};
}

KHook::Return<void> Hook_Spawn(CEntitySystem* pThis, int nCount, const EntitySpawnInfo_t* pInfo)
{
	for (int i = 0; i < nCount; i++)
		g_pMapMigrations->OnEntitySpawned_Pre(reinterpret_cast<CBaseEntity*>(pInfo[i].m_pEntity->m_pInstance), pInfo[i].m_pKeyValues);

	return {KHook::Action::Ignore};
}

KHook::Return<bool> Hook_ProcessVoiceData(CServerSideClient* pClient, const CCLCMsg_VoiceData_t& msg)
{
	ZEPlayer* pPlayer = g_playerManager->GetPlayer(pClient->GetPlayerSlot());

	if (!pPlayer)
		return {KHook::Action::Ignore};

	if (pPlayer->IsMuted())
		return {KHook::Action::Supersede, true};

	if (GetGlobals())
		pPlayer->SetLastVoiceTime(GetGlobals()->curtime);

	return {KHook::Action::Ignore};
}

KHook::Return<void> Hook_SetGameSpawnGroupMgr(INetworkGameServer* pThis, IGameSpawnGroupMgr* pSpawnGroupMgr)
{
	// This also resets our stored pointer on deletion, since null gets passed into this function, nice!
	g_pSpawnGroupMgr = (CSpawnGroupMgrGameSystem*)pSpawnGroupMgr;

	return {KHook::Action::Ignore};
}

CConVar<bool> g_cvarFixPhysicsPlayerShuffle("cs2f_shuffle_player_physics_sim", FCVAR_NONE, "Whether to enable shuffle player list in physics simulate", false);

struct TouchLinked_t
{
	uint32_t TouchFlags;

private:
	uint8_t padding_0[20];

public:
	CBaseHandle SourceHandle;
	CBaseHandle TargetHandle;

private:
	uint8_t padding_1[224];

public:
	[[nodiscard]] bool IsUnTouching() const
	{
		return !!(TouchFlags & 0x10);
	}

	[[nodiscard]] bool IsTouching() const
	{
		return (!!(TouchFlags & 4)) || (!!(TouchFlags & 8));
	}
};
static_assert(sizeof(TouchLinked_t) == 256, "Touch_t size mismatch");
KHook::Return<void> Hook_GetTouchingList_Post(CVPhys2World* pThis, CUtlVector<TouchLinked_t>* pList, bool unknown)
{
	if (!g_cvarFixPhysicsPlayerShuffle.Get() || pList->Count() <= 1)
		return {KHook::Action::Ignore};

	// [Kxnrl]
	// seems it sorted by flags?

	if (GetGlobals())
		std::srand(GetGlobals()->tickcount);

	// Fisher-Yates shuffle

	std::vector<TouchLinked_t> touchingLinks;
	std::vector<TouchLinked_t> unTouchLinks;

	FOR_EACH_VEC(*pList, i)
	{
		const auto& link = pList->Element(i);
		if (link.IsUnTouching())
			unTouchLinks.push_back(link);
		else
			touchingLinks.push_back(link);
	}

	if (touchingLinks.size() <= 1)
		return {KHook::Action::Ignore};

	for (size_t i = touchingLinks.size() - 1; i > 0; --i)
	{
		const auto j = std::rand() % (i + 1);
		std::swap(touchingLinks[i], touchingLinks[j]);
	}

	pList->Purge();

	for (const auto& link : touchingLinks)
		pList->AddToTail(link);
	for (const auto& link : unTouchLinks)
		pList->AddToTail(link);

	return {KHook::Action::Ignore};
}

KHook::Return<void> Hook_CheckMovingGround(CCSPlayer_MovementServices* pThis, double frametime)
{
	CCSPlayerPawn* pPawn = pThis->GetPawn();

	if (!pPawn || !GetGlobals())
		return {KHook::Action::Ignore};

	CCSPlayerController* pController = pPawn->GetOriginalController();

	if (!pController)
		return {KHook::Action::Ignore};

	int iSlot = pController->GetPlayerSlot();

	static int aPlayerTicks[MAXPLAYERS] = {0};

	// The point of doing this is to avoid running the function (and applying/resetting basevelocity) multiple times per tick
	// This can happen when the client or server lags
	if (aPlayerTicks[iSlot] == GetGlobals()->tickcount)
		return {KHook::Action::Supersede};

	aPlayerTicks[iSlot] = GetGlobals()->tickcount;

	return {KHook::Action::Ignore};
}

KHook::Return<void> Hook_DropWeapon_Post(CCSPlayer_WeaponServices* pThis, CBasePlayerWeapon* pWeapon, Vector* pVecTarget, Vector* pVelocity)
{
	if (g_cvarEnableEntWatch.Get())
		EW_DropWeapon(pThis, pWeapon);

	return {KHook::Action::Ignore};
}

KHook::Return<void> Hook_PlayerEquipUse(CGamePlayerEquip* pThis, InputData_t* pInput)
{
	CGamePlayerEquipHandler::Use(pThis, pInput);

	return {KHook::Action::Ignore};
}

KHook::Return<void> Hook_PlayerEquipPrecache_Post(CGamePlayerEquip* pThis, CEntityPrecacheContext* param)
{
	const auto kv = param->m_pKeyValues;
	CGamePlayerEquipHandler::OnPrecache(pThis, kv);

	return {KHook::Action::Ignore};
}

KHook::Return<void> Hook_TriggerGravityPrecache_Post(CTriggerGravity* pThis, CEntityPrecacheContext* param)
{
	const auto kv = param->m_pKeyValues;
	CTriggerGravityHandler::OnPrecache(pThis, kv);

	return {KHook::Action::Ignore};
}

KHook::Return<void> Hook_TriggerGravityEndTouch_Post(CTriggerGravity* pThis, CBaseEntity* pOther)
{
	CTriggerGravityHandler::OnEndTouch(pThis, pOther);

	return {KHook::Action::Ignore};
}

CConVar<bool> g_cvarDropMapWeapons("cs2f_drop_map_weapons", FCVAR_NONE, "Whether to force drop map-spawned weapons on death", false);

KHook::Return<bool> Hook_OnTakeDamage_Alive(CCSPlayerPawn* pPawn, CTakeDamageResult* pDamageResult)
{
	if (g_cvarEnableZR.Get() && ZR_Hook_OnTakeDamage_Alive(pDamageResult->m_pOriginatingInfo, pPawn))
	{
		pDamageResult->m_bWasDamageSuppressed = true;
		pDamageResult->m_flDamageDealt = 0.0f;
		return {KHook::Action::Supersede, false};
	}

	// This is a shit place to be doing this, but player_death event is too late and there is no pre-hook alternative
	// Check if this is going to kill the player
	if (g_cvarDropMapWeapons.Get() && pPawn && pPawn->m_iHealth() <= 0)
	{
		if (g_cvarEnableEntWatch.Get())
		{
			CCSPlayerController* pController = pPawn->GetOriginalController();
			if (pController)
				EW_PlayerDeathPre(pController);
		}

		pPawn->DropMapWeapons();
	}

	return {KHook::Action::Ignore};
}

float g_fTeleportXAngle;

KHook::Return<void> Hook_CCSPlayerPawn_Teleport(CCSPlayerPawn* pPawn, const Vector* pPosition, const QAngle* pAngles, const Vector* pVelocity)
{
	if (!pAngles)
		return {KHook::Action::Ignore};

	g_fTeleportXAngle = pAngles->x;

	QAngle* pCastAngles = const_cast<QAngle*>(pAngles);

	// Post-AG2, changing x or z angles via Teleport on a playermodel will bug out, go through SnapViewAngles for x later instead
	pCastAngles->x = 0.0f;
	pCastAngles->z = 0.0f;

	return {KHook::Action::Ignore};
}

KHook::Return<void> Hook_CCSPlayerPawn_Teleport_Post(CCSPlayerPawn* pPawn, const Vector* pPosition, const QAngle* pAngles, const Vector* pVelocity)
{
	if (!pAngles)
		return {KHook::Action::Ignore};

	// Revert the x edit, z should always be 0
	QAngle* pCastAngles = const_cast<QAngle*>(pAngles);
	pCastAngles->x = g_fTeleportXAngle;

	pPawn->SnapViewAngles(pCastAngles);
	return {KHook::Action::Ignore};
}
