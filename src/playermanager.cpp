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

#include <../cs2fixes.h>
#include "utlstring.h"
#include "playermanager.h"
#include "adminsystem.h"
#include "commands.h"
#include "map_votes.h"
#include "user_preferences.h"
#include "panoramavote.h"
#include "entity/ccsplayercontroller.h"
#include "utils/entity.h"
#include "serversideclient.h"
#include "recipientfilters.h"
#include "ctimer.h"
#include "ctime"
#include "leader.h"
#include "tier0/vprof.h"
#include "networksystem/inetworkmessages.h"

#include "tier0/memdbgon.h"


extern IVEngineServer2 *g_pEngineServer2;
extern CGameEntitySystem *g_pEntitySystem;
extern CGlobalVars *gpGlobals;

ZEPlayerHandle::ZEPlayerHandle() : m_Index(INVALID_ZEPLAYERHANDLE_INDEX) {};

ZEPlayerHandle::ZEPlayerHandle(CPlayerSlot slot)
{
	m_Parts.m_PlayerSlot = slot.Get();
	m_Parts.m_Serial = ++iZEPlayerHandleSerial;
}

ZEPlayerHandle::ZEPlayerHandle(const ZEPlayerHandle &other)
{
	m_Index = other.m_Index;
}

ZEPlayerHandle::ZEPlayerHandle(ZEPlayer *pZEPlayer)
{
	Set(pZEPlayer);
}

bool ZEPlayerHandle::operator==(ZEPlayer *pZEPlayer) const
{
	return Get() == pZEPlayer;
}

bool ZEPlayerHandle::operator!=(ZEPlayer *pZEPlayer) const
{
	return Get() != pZEPlayer;
}

void ZEPlayerHandle::Set(ZEPlayer *pZEPlayer)
{
	if (pZEPlayer)
		m_Index = pZEPlayer->GetHandle().m_Index;
	else
		m_Index = INVALID_ZEPLAYERHANDLE_INDEX;
}

ZEPlayer *ZEPlayerHandle::Get() const
{
	ZEPlayer *pZEPlayer = g_playerManager->GetPlayer((CPlayerSlot) m_Parts.m_PlayerSlot);

	if (!pZEPlayer)
		return nullptr;
	
	if (pZEPlayer->GetHandle().m_Index != m_Index)
		return nullptr;
	
	return pZEPlayer;
}

void ZEPlayer::OnSpawn()
{
	SetSpeedMod(1.f);
}

void ZEPlayer::OnAuthenticated()
{
	m_bAuthenticated = true;
	m_SteamID = m_UnauthenticatedSteamID;

	Message("%lli authenticated\n", GetSteamId64());

	CheckAdmin();
	CheckInfractions();
	g_pUserPreferencesSystem->PullPreferences(GetPlayerSlot().Get());
}

void ZEPlayer::CheckInfractions()
{
	g_pAdminSystem->ApplyInfractions(this);
}

void ZEPlayer::CheckAdmin()
{
	if (IsFakeClient())
		return;

	auto admin = g_pAdminSystem->FindAdmin(GetSteamId64());
	if (!admin)
	{
		SetAdminFlags(0);
		return;
	}

	SetAdminFlags(admin->GetFlags());

	Message("%lli authenticated as an admin\n", GetSteamId64());
}

bool ZEPlayer::IsAdminFlagSet(uint64 iFlag)
{
	return !iFlag || (m_iAdminFlags & iFlag);
}

int ZEPlayer::GetHideDistance()
{
	return g_pUserPreferencesSystem->GetPreferenceInt(m_slot.Get(), HIDE_DISTANCE_PREF_KEY_NAME, 0);
}

void ZEPlayer::SetHideDistance(int distance)
{
	g_pUserPreferencesSystem->SetPreferenceInt(m_slot.Get(), HIDE_DISTANCE_PREF_KEY_NAME, distance);
}

static bool g_bFlashLightShadows = true;
bool g_bFlashLightTransmitOthers = false;
static float g_flFlashLightBrightness = 1.0f;
static float g_flFlashLightDistance = 54.0f; // The minimum distance such that an awp wouldn't block the light
static Color g_clrFlashLightColor(255, 255, 255);
static std::string g_sFlashLightAttachment = "axis_of_intent";

FAKE_BOOL_CVAR(cs2f_flashlight_shadows, "Whether to enable flashlight shadows", g_bFlashLightShadows, true, false)
FAKE_BOOL_CVAR(cs2f_flashlight_transmit_others, "Whether to transmit other player's flashlights, recommended to have shadows off for this", g_bFlashLightTransmitOthers, true, false)
FAKE_FLOAT_CVAR(cs2f_flashlight_brightness, "How bright should flashlights be", g_flFlashLightBrightness, 1.0f, false)
FAKE_FLOAT_CVAR(cs2f_flashlight_distance, "How far flashlights should be from the player's head", g_flFlashLightDistance, 54.0f, false)
FAKE_COLOR_CVAR(cs2f_flashlight_color, "What color to use for flashlights", g_clrFlashLightColor, false)
FAKE_STRING_CVAR(cs2f_flashlight_attachment, "Which attachment to parent a flashlight to. "
	"If the player model is not properly setup, you might have to use clip_limit here instead", g_sFlashLightAttachment, false)

void ZEPlayer::SpawnFlashLight()
{
	if (GetFlashLight())
		return;

	CCSPlayerPawn *pPawn = (CCSPlayerPawn *)CCSPlayerController::FromSlot(GetPlayerSlot())->GetPawn();

	Vector origin = pPawn->GetAbsOrigin();
	Vector forward;
	AngleVectors(pPawn->m_angEyeAngles(), &forward);

	origin.z += 64.0f;
	origin += forward * g_flFlashLightDistance;

	CBarnLight *pLight = (CBarnLight *)CreateEntityByName("light_barn");

	pLight->m_bEnabled = true;
	pLight->m_Color->SetColor(g_clrFlashLightColor[0], g_clrFlashLightColor[1], g_clrFlashLightColor[2]);
	pLight->m_flBrightness = g_flFlashLightBrightness;
	pLight->m_flRange = 2048.0f;
	pLight->m_flSoftX = 1.0f;
	pLight->m_flSoftY = 1.0f;
	pLight->m_flSkirt = 0.5f;
	pLight->m_flSkirtNear = 1.0f;
	pLight->m_vSizeParams->Init(45.0f, 45.0f, 0.02f);
	pLight->m_nCastShadows = g_bFlashLightShadows;
	pLight->m_nDirectLight = 3;
	pLight->Teleport(&origin, &pPawn->m_angEyeAngles(), nullptr);

	// Have to use keyvalues for this since the schema prop is a resource handle
	CEntityKeyValues *pKeyValues = new CEntityKeyValues();
	pKeyValues->SetString("lightcookie", "materials/effects/lightcookies/flashlight.vtex");

	pLight->DispatchSpawn(pKeyValues);

	pLight->SetParent(pPawn);
	pLight->AcceptInput("SetParentAttachmentMaintainOffset", g_sFlashLightAttachment.c_str());

	SetFlashLight(pLight);
}

void ZEPlayer::ToggleFlashLight()
{
	// Play the "click" sound
	CSingleRecipientFilter filter(GetPlayerSlot());
	CCSPlayerController::FromSlot(GetPlayerSlot())->EmitSoundFilter(filter, "HudChat.Message");

	CBarnLight *pLight = GetFlashLight();

	// Create a flashlight if we don't have one, and don't bother with the input since it spawns enabled
	if (!pLight)
	{
		SpawnFlashLight();
		return;
	}

	pLight->AcceptInput(pLight->m_bEnabled() ? "Disable" : "Enable");
}

static float g_flFloodInterval = 0.75f;
static int g_iMaxFloodTokens = 3;
static float g_flFloodCooldown = 3.0f;
static std::string g_sBeaconParticle = "particles/cs2fixes/player_beacon.vpcf";

FAKE_FLOAT_CVAR(cs2f_flood_interval, "Amount of time allowed between chat messages acquiring flood tokens", g_flFloodInterval, 0.75f, false)
FAKE_INT_CVAR(cs2f_max_flood_tokens, "Maximum number of flood tokens allowed before chat messages are blocked", g_iMaxFloodTokens, 3, false)
FAKE_FLOAT_CVAR(cs2f_flood_cooldown, "Amount of time to block messages for when a player floods", g_flFloodCooldown, 3.0f, false)
FAKE_STRING_CVAR(cs2f_beacon_particle, ".vpcf file to be precached and used for beacon", g_sBeaconParticle, false)

bool ZEPlayer::IsFlooding()
{
	if (m_bGagged) return false;

	float time = gpGlobals->curtime;
	float newTime = time + g_flFloodInterval;

	if (m_flLastTalkTime >= time)
	{
		if (m_iFloodTokens >= g_iMaxFloodTokens)
		{
			m_flLastTalkTime = newTime + g_flFloodCooldown;
			return true;
		}
		else
		{
			m_iFloodTokens++;
		}
	}
	else if(m_iFloodTokens > 0)
	{
		// Remove one flood token when player chats within time limit (slow decay)
		m_iFloodTokens--;
	}

	m_flLastTalkTime = newTime;
	return false;
}

void PrecacheBeaconParticle(IEntityResourceManifest* pResourceManifest)
{
	pResourceManifest->AddResource(g_sBeaconParticle.c_str());
}

void ZEPlayer::StartBeacon(Color color, ZEPlayerHandle hGiver/* = 0*/)
{
	CCSPlayerController* pPlayer = CCSPlayerController::FromSlot(m_slot);

	Vector vecAbsOrigin = pPlayer->GetPawn()->GetAbsOrigin();

	vecAbsOrigin.z += 10;

	CParticleSystem* particle = (CParticleSystem*)CreateEntityByName("info_particle_system");

	CEntityKeyValues* pKeyValues = new CEntityKeyValues();

	pKeyValues->SetString("effect_name", g_sBeaconParticle.c_str());
	pKeyValues->SetInt("tint_cp", 1);
	pKeyValues->SetVector("origin", vecAbsOrigin);
	pKeyValues->SetBool("start_active", true);

	particle->m_clrTint->SetRawColor(color.GetRawColor());

	particle->DispatchSpawn(pKeyValues);
	particle->SetParent(pPlayer->GetPawn());

	m_hBeaconParticle.Set(particle);

	CHandle<CParticleSystem> hParticle = particle->GetHandle();
	ZEPlayerHandle hPlayer = m_Handle;
	int iTeamNum = pPlayer->m_iTeamNum;
	bool bLeaderBeacon = false;

	ZEPlayer *pGiver = hGiver.Get();
	if (pGiver && pGiver->IsLeader())
		bLeaderBeacon = true;

	new CTimer(1.0f, false, false, [hPlayer, hParticle, hGiver, iTeamNum, bLeaderBeacon]()
	{
		CParticleSystem *pParticle = hParticle.Get();

		if (!hPlayer.IsValid() || !pParticle)
			return -1.0f;

		CCSPlayerController *pPlayer = CCSPlayerController::FromSlot((CPlayerSlot) hPlayer.GetPlayerSlot());

		if (pPlayer->m_iTeamNum < CS_TEAM_T || !pPlayer->m_hPlayerPawn->IsAlive() || pPlayer->m_iTeamNum != iTeamNum)
		{
			addresses::UTIL_Remove(pParticle);
			return -1.0f;
		}

		if (!bLeaderBeacon)
			return 1.0f;

		ZEPlayer *pBeaconGiver = hGiver.Get();

		// Continue beacon, leader is not on the server. No reason to remove the beacon
		if (!pBeaconGiver)
			return 1.0f;

		// Remove beacon granted by leader if his leader was stripped
		if (!pBeaconGiver->IsLeader())
		{
			addresses::UTIL_Remove(pParticle);
			return -1.0f;
		}

		return 1.0f;
	});
}

void ZEPlayer::EndBeacon()
{
	CParticleSystem *pParticle = m_hBeaconParticle.Get();

	if (pParticle)
		addresses::UTIL_Remove(pParticle);
}

void ZEPlayer::SetLeader(int leaderIndex)
{
	if (leaderIndex >= g_nLeaderColorMapSize)
	{
		m_iLeaderIndex = g_iLeaderIndex = 1;
		return;
	}

	m_iLeaderIndex = leaderIndex;
}

int ZEPlayer::GetLeaderVoteCount()
{
	int iValidVoteCount = 0;

	for (int i = m_vecLeaderVotes.Count() - 1; i >= 0; i--)
	{
		if (m_vecLeaderVotes[i].IsValid())
			iValidVoteCount++;
		else
			m_vecLeaderVotes.Remove(i);
	}

	return iValidVoteCount;
}

bool ZEPlayer::HasPlayerVotedLeader(ZEPlayer *pPlayer)
{
	FOR_EACH_VEC(m_vecLeaderVotes, i)
	{
		if (m_vecLeaderVotes[i] == pPlayer)
			return true;
	}

	return false;
}

void ZEPlayer::AddLeaderVote(ZEPlayer* pPlayer)
{
	m_vecLeaderVotes.AddToTail(pPlayer->GetHandle());
}

void ZEPlayer::PurgeLeaderVotes()
{
	m_vecLeaderVotes.Purge();
}

void ZEPlayer::StartGlow(Color color, int duration)
{
	CCSPlayerController *pController = CCSPlayerController::FromSlot(m_slot);
	CCSPlayerPawn *pPawn = (CCSPlayerPawn*)pController->GetPawn();
	
	const char *pszModelName = pPawn->GetModelName();
	
	CBaseModelEntity *pModelGlow = (CBaseModelEntity*)CreateEntityByName("prop_dynamic");
	CBaseModelEntity *pModelRelay = (CBaseModelEntity*)CreateEntityByName("prop_dynamic");
	CEntityKeyValues *pKeyValuesRelay = new CEntityKeyValues();
	
	pKeyValuesRelay->SetString("model", pszModelName);
	pKeyValuesRelay->SetInt64("spawnflags", 256U);
	pKeyValuesRelay->SetInt("rendermode", kRenderNone);

	CEntityKeyValues *pKeyValuesGlow = new CEntityKeyValues();
	pKeyValuesGlow->SetString("model", pszModelName);
	pKeyValuesGlow->SetInt64("spawnflags", 256U);
	pKeyValuesGlow->SetColor("glowcolor", color);
	pKeyValuesGlow->SetInt("glowrange", 5000);
	pKeyValuesGlow->SetInt("glowteam", -1);
	pKeyValuesGlow->SetInt("glowstate", 3);
	pKeyValuesGlow->SetInt("renderamt", 1);

	pModelGlow->DispatchSpawn(pKeyValuesGlow);
	pModelRelay->DispatchSpawn(pKeyValuesRelay);
	pModelRelay->AcceptInput("FollowEntity", "!activator", pPawn);
	pModelGlow->AcceptInput("FollowEntity", "!activator", pModelRelay);

	m_hGlowModel.Set(pModelGlow);
	
	CHandle<CBaseModelEntity> hGlowModel = m_hGlowModel;
	CHandle<CCSPlayerPawn> hPawn = pPawn->GetHandle();
	int iTeamNum = hPawn->m_iTeamNum();

	// check if player's team or model changed
	new CTimer(0.5f, false, false, [hGlowModel, hPawn, iTeamNum]()
	{
		CBaseModelEntity *pModel = hGlowModel.Get();
		CCSPlayerPawn *pawn = hPawn.Get();

		if (!pawn || !pModel)
			return -1.0f;

		if (pawn->m_iTeamNum != iTeamNum || strcmp(pModel->GetModelName(), pawn->GetModelName()))
		{
			CGameSceneNode *pParentSceneNode = pModel->m_CBodyComponent()->m_pSceneNode()->m_pParent();

			if (!pParentSceneNode)
				return -1.0f;

			CBaseModelEntity *pModelParent = (CBaseModelEntity*)pParentSceneNode->m_pOwner();

			if (pModelParent)
				addresses::UTIL_Remove(pModelParent);
			
			return -1.0f;
		}

		return 0.5f;
	});

	// kill glow after duration, if provided
	if (duration < 1)
		return;
	
	new CTimer((float)duration, false, false, [hGlowModel]()
	{
		CBaseModelEntity *pModel = hGlowModel.Get();

		if (!pModel)
			return -1.0f;

		CGameSceneNode *pParentSceneNode = pModel->m_CBodyComponent()->m_pSceneNode()->m_pParent();

		if (!pParentSceneNode)
			return -1.0f;

		CBaseModelEntity *pModelParent = (CBaseModelEntity*)pParentSceneNode->m_pOwner();

		if (pModelParent)
			addresses::UTIL_Remove(pModelParent);

		return -1.0f;
	});
}

void ZEPlayer::EndGlow()
{
	CBaseModelEntity *pGlowModel = m_hGlowModel.Get();

	if (!pGlowModel)
		return;

	CGameSceneNode *pParentSceneNode = pGlowModel->m_CBodyComponent()->m_pSceneNode()->m_pParent();

	if (!pParentSceneNode)
		return;

	CBaseModelEntity *pModelParent = (CBaseModelEntity*)pParentSceneNode->m_pOwner();

	if (pModelParent)
		addresses::UTIL_Remove(pModelParent);
}

void ZEPlayer::ReplicateConVar(const char* pszName, const char* pszValue)
{
	INetworkMessageInternal* pNetMsg = g_pNetworkMessages->FindNetworkMessagePartial("SetConVar");
	auto data = pNetMsg->AllocateMessage()->ToPB<CNETMsg_SetConVar>();

	CMsg_CVars_CVar* cvarMsg = data->mutable_convars()->add_cvars();
	cvarMsg->set_name(pszName);
	cvarMsg->set_value(pszValue);

	GetClientBySlot(GetPlayerSlot())->GetNetChannel()->SendNetMessage(pNetMsg, data, BUF_RELIABLE);
	pNetMsg->DeallocateMessage(data);
}

void CPlayerManager::OnBotConnected(CPlayerSlot slot)
{
	m_vecPlayers[slot.Get()] = new ZEPlayer(slot, true);
}

bool CPlayerManager::OnClientConnected(CPlayerSlot slot, uint64 xuid, const char* pszNetworkID)
{
	Assert(m_vecPlayers[slot.Get()] == nullptr);

	Message("%d connected\n", slot.Get());

	ZEPlayer *pPlayer = new ZEPlayer(slot);
	pPlayer->SetUnauthenticatedSteamId(new CSteamID(xuid));

	std::string ip(pszNetworkID);

	// Remove port
	for (int i = 0; i < ip.length(); i++)
	{
		if (ip[i] == ':')
		{
			ip = ip.substr(0, i);
			break;
		}
	}

	pPlayer->SetIpAddress(ip);

	if (!g_pAdminSystem->ApplyInfractions(pPlayer))
	{
		// Player is banned
		delete pPlayer;
		return false;
	}

	pPlayer->SetConnected();
	m_vecPlayers[slot.Get()] = pPlayer;

	ResetPlayerFlags(slot.Get());

	g_pMapVoteSystem->ClearPlayerInfo(slot.Get());
	
	return true;
}

void CPlayerManager::OnClientDisconnect(CPlayerSlot slot)
{
	Message("%d disconnected\n", slot.Get());

	g_pUserPreferencesSystem->PushPreferences(slot.Get());
	g_pUserPreferencesSystem->ClearPreferences(slot.Get());

	delete m_vecPlayers[slot.Get()];
	m_vecPlayers[slot.Get()] = nullptr;

	ResetPlayerFlags(slot.Get());

	g_pMapVoteSystem->ClearPlayerInfo(slot.Get());

	g_pPanoramaVoteHandler->RemovePlayerFromVote(slot.Get());
}

void CPlayerManager::OnClientPutInServer(CPlayerSlot slot)
{
	ZEPlayer* pPlayer = m_vecPlayers[slot.Get()];

	pPlayer->SetInGame(true);
}

void CPlayerManager::OnLateLoad()
{
	for (int i = 0; i < gpGlobals->maxClients; i++)
	{
		CCSPlayerController* pController = CCSPlayerController::FromSlot(i);

		if (!pController || !pController->IsController() || !pController->IsConnected())
			continue;

		OnClientConnected(i, pController->m_steamID(), "0.0.0.0:0");
	}
}

void CPlayerManager::OnSteamAPIActivated()
{
	m_CallbackValidateAuthTicketResponse.Register(this, &CPlayerManager::OnValidateAuthTicket);
}

int g_iDelayAuthFailKick = 0;
FAKE_INT_CVAR(cs2f_delay_auth_fail_kick, "How long in seconds to delay kicking players when their Steam authentication fails, use with sv_steamauth_enforce 0", g_iDelayAuthFailKick, 0, false);

void CPlayerManager::OnValidateAuthTicket(ValidateAuthTicketResponse_t *pResponse)
{
	uint64 iSteamId = pResponse->m_SteamID.ConvertToUint64();

	Message("%s: SteamID=%llu Response=%d\n", __func__, iSteamId, pResponse->m_eAuthSessionResponse);

	ZEPlayer *pPlayer = nullptr;

	for (ZEPlayer *pPlayer : m_vecPlayers)
	{
		if (!pPlayer || pPlayer->IsFakeClient() || !(pPlayer->GetUnauthenticatedSteamId64() == iSteamId))
			continue;

		CCSPlayerController *pController = CCSPlayerController::FromSlot(pPlayer->GetPlayerSlot());

		switch (pResponse->m_eAuthSessionResponse)
		{
			case k_EAuthSessionResponseOK:
			{
				pPlayer->OnAuthenticated();
				return;
			}

			case k_EAuthSessionResponseAuthTicketInvalid:
			case k_EAuthSessionResponseAuthTicketInvalidAlreadyUsed:
			{
				if (!g_iDelayAuthFailKick)
					return;

				ClientPrint(pController, HUD_PRINTTALK, " \7Your Steam authentication failed due to an invalid or used ticket.");
				ClientPrint(pController, HUD_PRINTTALK, " \7You may have to restart your Steam client in order to fix this.\n");
				[[fallthrough]];
			}

			default:
			{
				if (!g_iDelayAuthFailKick)
					return;

				ClientPrint(pController, HUD_PRINTTALK, " \7WARNING: You will be kicked in %i seconds due to failed Steam authentication.\n", g_iDelayAuthFailKick);

				ZEPlayerHandle hPlayer = pPlayer->GetHandle();
				new CTimer(g_iDelayAuthFailKick, true, true, [hPlayer]()
				{
					if (!hPlayer.IsValid())
						return -1.f;

					g_pEngineServer2->DisconnectClient(hPlayer.GetPlayerSlot(), NETWORK_DISCONNECT_KICKED_NOSTEAMLOGIN);
					return -1.f;
				});
			}
		}
	}
}

void CPlayerManager::CheckInfractions()
{
	for (int i = 0; i < gpGlobals->maxClients; i++)
	{
		if (m_vecPlayers[i] == nullptr || m_vecPlayers[i]->IsFakeClient())
			continue;

		m_vecPlayers[i]->CheckInfractions();
	}

	g_pAdminSystem->SaveInfractions();
}

static bool g_bFlashLightEnable = false;

FAKE_BOOL_CVAR(cs2f_flashlight_enable, "Whether to enable flashlights", g_bFlashLightEnable, false, false)

void CPlayerManager::FlashLightThink()
{
	if (!g_bFlashLightEnable)
		return;

	VPROF("CPlayerManager::FlashLightThink");

	for (int i = 0; i < gpGlobals->maxClients; i++)
	{
		CCSPlayerController *pPlayer = CCSPlayerController::FromSlot(i);

		if (!pPlayer || !pPlayer->m_bPawnIsAlive())
			continue;

		uint64 *pButtons = pPlayer->GetPawn()->m_pMovementServices->m_nButtons().m_pButtonStates();

		// Check both to make sure flashlight is only toggled when the player presses the key
		if ((pButtons[0] & IN_LOOK_AT_WEAPON) && (pButtons[1] & IN_LOOK_AT_WEAPON))
			pPlayer->GetZEPlayer()->ToggleFlashLight();
	}
}

static bool g_bHideTeammatesOnly = false;

FAKE_BOOL_CVAR(cs2f_hide_teammates_only, "Whether to hide teammates only", g_bHideTeammatesOnly, false, false)

void CPlayerManager::CheckHideDistances()
{
	if (!g_pEntitySystem)
		return;

	VPROF("CPlayerManager::CheckHideDistances");

	for (int i = 0; i < gpGlobals->maxClients; i++)
	{
		auto player = GetPlayer(i);

		if (!player)
			continue;

		player->ClearTransmit();
		auto hideDistance = player->GetHideDistance();

		if (!hideDistance)
			continue;

		CCSPlayerController* pController = CCSPlayerController::FromSlot(i);

		if (!pController)
			continue;

		auto pPawn = pController->GetPawn();

		if (!pPawn || !pPawn->IsAlive())
			continue;

		auto vecPosition = pPawn->GetAbsOrigin();
		int team = pController->m_iTeamNum;

		for (int j = 0; j < gpGlobals->maxClients; j++)
		{
			if (j == i)
				continue;

			CCSPlayerController* pTargetController = CCSPlayerController::FromSlot(j);

			if (pTargetController)
			{
				auto pTargetPawn = pTargetController->GetPawn();

				// TODO: Unhide dead pawns if/when valve fixes the crash
				if (pTargetPawn && (!g_bHideTeammatesOnly || pTargetController->m_iTeamNum == team))
				{
					player->SetTransmit(j, pTargetPawn->GetAbsOrigin().DistToSqr(vecPosition) <= hideDistance * hideDistance);
				}
			}
		}
	}
}

static const char *g_szPlayerStates[] =
{
	"STATE_ACTIVE",
	"STATE_WELCOME",
	"STATE_PICKINGTEAM",
	"STATE_PICKINGCLASS",
	"STATE_DEATH_ANIM",
	"STATE_DEATH_WAIT_FOR_KEY",
	"STATE_OBSERVER_MODE",
	"STATE_GUNGAME_RESPAWN",
	"STATE_DORMANT"
};

extern bool g_bEnableHide;

void CPlayerManager::UpdatePlayerStates()
{
	for (int i = 0; i < gpGlobals->maxClients; i++)
	{
		ZEPlayer *pPlayer = GetPlayer(i);

		if (!pPlayer)
			continue;

		CCSPlayerController *pController = CCSPlayerController::FromSlot(i);

		if (!pController)
			continue;

		uint32 iPreviousPlayerState = pPlayer->GetPlayerState();
		uint32 iCurrentPlayerState = pController->GetPawnState();

		if (iCurrentPlayerState != iPreviousPlayerState)
		{
			if (g_bEnableHide)
				Message("Player %s changed states from %s to %s\n", pController->GetPlayerName(), g_szPlayerStates[iPreviousPlayerState], g_szPlayerStates[iCurrentPlayerState]);

			pPlayer->SetPlayerState(iCurrentPlayerState);

			// Send full update to people going in/out of spec as a mitigation for hide crashes
			if (g_bEnableHide && (iCurrentPlayerState == STATE_OBSERVER_MODE || iPreviousPlayerState == STATE_OBSERVER_MODE))
			{
				CServerSideClient *pClient = GetClientBySlot(i);

				if (pClient)
					pClient->ForceFullUpdate();
			}
		}
	}
}

static bool g_bInfiniteAmmo = false;
FAKE_BOOL_CVAR(cs2f_infinite_reserve_ammo, "Whether to enable infinite reserve ammo on weapons", g_bInfiniteAmmo, false, false)

void CPlayerManager::SetupInfiniteAmmo()
{
	new CTimer(5.0f, false, true, []()
	{
		if (!g_bInfiniteAmmo)
			return 5.0f;

		VPROF("CPlayerManager::InfiniteAmmoTimer");

		for (int i = 0; i < gpGlobals->maxClients; i++)
		{
			CCSPlayerController* pController = CCSPlayerController::FromSlot(i);

			if (!pController)
				continue;

			auto pPawn = pController->GetPawn();

			if (!pPawn)
				continue;

			CCSPlayer_WeaponServices* pWeaponServices = pPawn->m_pWeaponServices;

			// it can sometimes be null when player joined on the very first round? 
			if (!pWeaponServices)
				continue;

			CUtlVector<CHandle<CBasePlayerWeapon>>* weapons = pWeaponServices->m_hMyWeapons();

			FOR_EACH_VEC(*weapons, i)
			{
				CBasePlayerWeapon* weapon = (*weapons)[i].Get();

				if (!weapon)
					continue;

				if (weapon->GetWeaponVData()->m_GearSlot() == GEAR_SLOT_RIFLE || weapon->GetWeaponVData()->m_GearSlot() == GEAR_SLOT_PISTOL)
					weapon->AcceptInput("SetReserveAmmoAmount", "999"); // 999 will be automatically clamped to the weapons m_nPrimaryReserveAmmoMax
			}
		}

		return 5.0f;
	});
}

ETargetType CPlayerManager::TargetPlayerString(int iCommandClient, const char* target, int& iNumClients, int *clients)
{
	ETargetType targetType = ETargetType::NONE;
	if (!V_stricmp(target, "@me"))
		targetType = ETargetType::SELF;
	else if (!V_stricmp(target, "@all"))
		targetType = ETargetType::ALL;
	else if (!V_stricmp(target, "@t"))
		targetType = ETargetType::T;
	else if (!V_stricmp(target, "@ct"))
		targetType = ETargetType::CT;
	else if (!V_stricmp(target, "@spec"))
		targetType = ETargetType::SPECTATOR;
	else if (!V_stricmp(target, "@random"))
		targetType = ETargetType::RANDOM;
	else if (!V_stricmp(target, "@randomt"))
		targetType = ETargetType::RANDOM_T;
	else if (!V_stricmp(target, "@randomct"))
		targetType = ETargetType::RANDOM_CT;
	
	if (targetType == ETargetType::SELF && iCommandClient != -1)
	{
		clients[iNumClients++] = iCommandClient;
	}
	else if (targetType == ETargetType::ALL)
	{
		for (int i = 0; i < gpGlobals->maxClients; i++)
		{
			if (m_vecPlayers[i] == nullptr)
				continue;

			CCSPlayerController* player = CCSPlayerController::FromSlot(i);

			if (!player || !player->IsController() || !player->IsConnected())
				continue;

			clients[iNumClients++] = i;
		}
	}
	else if (targetType >= ETargetType::SPECTATOR)
	{
		for (int i = 0; i < gpGlobals->maxClients; i++)
		{
			if (m_vecPlayers[i] == nullptr)
				continue;

			CCSPlayerController* player = CCSPlayerController::FromSlot(i);

			if (!player || !player->IsController() || !player->IsConnected())
				continue;

			if (player->m_iTeamNum() != (targetType == ETargetType::T ? CS_TEAM_T : targetType == ETargetType::CT ? CS_TEAM_CT : CS_TEAM_SPECTATOR))
				continue;

			clients[iNumClients++] = i;
		}
	}
	else if (targetType >= ETargetType::RANDOM && targetType <= ETargetType::RANDOM_CT)
	{
		int attempts = 0;

		while (iNumClients == 0 && attempts < 10000)
		{
			int slot = rand() % (gpGlobals->maxClients - 1);

			// Prevent infinite loop
			attempts++;

			if (m_vecPlayers[slot] == nullptr)
				continue;

			CCSPlayerController* player = CCSPlayerController::FromSlot(slot);

			if (!player || !player->IsController() || !player->IsConnected())
				continue;

			if (targetType >= ETargetType::RANDOM_T && (player->m_iTeamNum() != (targetType == ETargetType::RANDOM_T ? CS_TEAM_T : CS_TEAM_CT)))
				continue;

			clients[iNumClients++] = slot;
		}
	}
	else if (*target == '#')
	{
		int userid = V_StringToUint16(target + 1, -1);

		if (userid != -1)
		{
			targetType = ETargetType::PLAYER;
			CCSPlayerController* player = CCSPlayerController::FromSlot(GetSlotFromUserId(userid).Get());
			if(player && player->IsController() && player->IsConnected())
				clients[iNumClients++] = GetSlotFromUserId(userid).Get();
		}
	}
	else
	{
		for (int i = 0; i < gpGlobals->maxClients; i++)
		{
			if (m_vecPlayers[i] == nullptr)
				continue;

			CCSPlayerController* player = CCSPlayerController::FromSlot(i);

			if (!player || !player->IsController() || !player->IsConnected())
				continue;

			if (V_stristr(player->GetPlayerName(), target))
			{
				targetType = ETargetType::PLAYER;
				clients[iNumClients++] = i;
			}
		}
	}

	return targetType;
}

ZEPlayer *CPlayerManager::GetPlayer(CPlayerSlot slot)
{
	if (slot.Get() < 0 || slot.Get() >= gpGlobals->maxClients)
		return nullptr;

	return m_vecPlayers[slot.Get()];
};

// In userids, the lower byte is always the player slot
CPlayerSlot CPlayerManager::GetSlotFromUserId(uint16 userid)
{
	return CPlayerSlot(userid & 0xFF);
}

ZEPlayer *CPlayerManager::GetPlayerFromUserId(uint16 userid)
{
	uint8 index = userid & 0xFF;

	if (index >= gpGlobals->maxClients)
		return nullptr;

	return m_vecPlayers[index];
}

ZEPlayer* CPlayerManager::GetPlayerFromSteamId(uint64 steamid)
{
	for (ZEPlayer* player : m_vecPlayers)
	{
		if (player && player->IsAuthenticated() && player->GetSteamId64() == steamid)
			return player;
	}

	return nullptr;
}

void CPlayerManager::SetPlayerStopSound(int slot, bool set)
{
	if (set)
		m_nUsingStopSound |= ((uint64)1 << slot);
	else
		m_nUsingStopSound &= ~((uint64)1 << slot);

	// Set the user prefs if the player is ingame
	ZEPlayer* pPlayer = m_vecPlayers[slot];
	if (!pPlayer) return;

	uint64 iSlotMask = (uint64)1 << slot;
	int iStopPreferenceStatus = (m_nUsingStopSound & iSlotMask)?1:0;
	int iSilencePreferenceStatus = (m_nUsingSilenceSound & iSlotMask)?2:0;
	g_pUserPreferencesSystem->SetPreferenceInt(slot, SOUND_STATUS_PREF_KEY_NAME, iStopPreferenceStatus + iSilencePreferenceStatus);
}

void CPlayerManager::SetPlayerSilenceSound(int slot, bool set)
{
	if (set)
		m_nUsingSilenceSound |= ((uint64)1 << slot);
	else
		m_nUsingSilenceSound &= ~((uint64)1 << slot);

	// Set the user prefs if the player is ingame
	ZEPlayer* pPlayer = m_vecPlayers[slot];
	if (!pPlayer) return;

	uint64 iSlotMask = (uint64)1 << slot;
	int iStopPreferenceStatus = (m_nUsingStopSound & iSlotMask)?1:0;
	int iSilencePreferenceStatus = (m_nUsingSilenceSound & iSlotMask)?2:0;
	g_pUserPreferencesSystem->SetPreferenceInt(slot, SOUND_STATUS_PREF_KEY_NAME, iStopPreferenceStatus + iSilencePreferenceStatus);
}

void CPlayerManager::SetPlayerStopDecals(int slot, bool set)
{
	if (set)
		m_nUsingStopDecals |= ((uint64)1 << slot);
	else
		m_nUsingStopDecals &= ~((uint64)1 << slot);

	// Set the user prefs if the player is ingame
	ZEPlayer* pPlayer = m_vecPlayers[slot];
	if (!pPlayer) return;

	uint64 iSlotMask = (uint64)1 << slot;
	int iDecalPreferenceStatus = (m_nUsingStopDecals & iSlotMask)?1:0;
	g_pUserPreferencesSystem->SetPreferenceInt(slot, DECAL_PREF_KEY_NAME, iDecalPreferenceStatus);
}

void CPlayerManager::ResetPlayerFlags(int slot)
{
	SetPlayerStopSound(slot, true);
	SetPlayerSilenceSound(slot, false);
	SetPlayerStopDecals(slot, true);
}