/**
 * =============================================================================
 * CS2Fixes
 * Copyright (C) 2023-2025 Source2ZE
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

#include "playermanager.h"
#include "adminsystem.h"
#include "commands.h"
#include "ctime"
#include "ctimer.h"
#include "engine/igameeventsystem.h"
#include "entity/ccsplayercontroller.h"
#include "entwatch.h"
#include "leader.h"
#include "map_votes.h"
#include "networksystem/inetworkmessages.h"
#include "panoramavote.h"
#include "recipientfilters.h"
#include "serversideclient.h"
#include "tier0/vprof.h"
#include "user_preferences.h"
#include "utils/entity.h"
#include "utlstring.h"
#include "votemanager.h"
#include <../cs2fixes.h>

#include "tier0/memdbgon.h"

extern IVEngineServer2* g_pEngineServer2;
extern CGameEntitySystem* g_pEntitySystem;
extern CGlobalVars* GetGlobals();
extern IGameEventSystem* g_gameEventSystem;
extern CUtlVector<CServerSideClient*>* GetClientList();

CConVar<int> g_cvarAdminImmunityTargetting("cs2f_admin_immunity", FCVAR_NONE, "Mode for which admin immunity system targetting allows: 0 - strictly lower, 1 - equal to or lower, 2 - ignore immunity levels", 0, true, 0, true, 2);
CConVar<bool> g_cvarEnableMapSteamIds("cs2f_map_steamids_enable", FCVAR_NONE, "Whether to make Steam ID's available to maps", false);

ZEPlayerHandle::ZEPlayerHandle() :
	m_Index(INVALID_ZEPLAYERHANDLE_INDEX){};

ZEPlayerHandle::ZEPlayerHandle(CPlayerSlot slot)
{
	m_Parts.m_PlayerSlot = slot.Get();
	m_Parts.m_Serial = ++iZEPlayerHandleSerial;
}

ZEPlayerHandle::ZEPlayerHandle(const ZEPlayerHandle& other)
{
	m_Index = other.m_Index;
}

ZEPlayerHandle::ZEPlayerHandle(ZEPlayer* pZEPlayer)
{
	Set(pZEPlayer);
}

bool ZEPlayerHandle::operator==(ZEPlayer* pZEPlayer) const
{
	return Get() == pZEPlayer;
}

bool ZEPlayerHandle::operator!=(ZEPlayer* pZEPlayer) const
{
	return Get() != pZEPlayer;
}

void ZEPlayerHandle::Set(ZEPlayer* pZEPlayer)
{
	if (pZEPlayer)
		m_Index = pZEPlayer->GetHandle().m_Index;
	else
		m_Index = INVALID_ZEPLAYERHANDLE_INDEX;
}

ZEPlayer* ZEPlayerHandle::Get() const
{
	ZEPlayer* pZEPlayer = g_playerManager->GetPlayer((CPlayerSlot)m_Parts.m_PlayerSlot);

	if (!pZEPlayer)
		return nullptr;

	if (pZEPlayer->GetHandle().m_Index != m_Index)
		return nullptr;

	return pZEPlayer;
}

void ZEPlayer::OnSpawn()
{
	SetSpeedMod(1.f);

	ZEPlayerHandle handle = GetHandle();
	new CTimer(0.0f, false, false, [handle] {
		if (handle.Get())
			handle.Get()->CreateEntwatchHud();
		return -1.0f;
	});
}

void ZEPlayer::OnAuthenticated()
{
	m_bAuthenticated = true;
	m_SteamID = m_UnauthenticatedSteamID;

	Message("%lli authenticated\n", GetSteamId64());

	CheckAdmin();
	CheckInfractions();
	g_pUserPreferencesSystem->PullPreferences(GetPlayerSlot().Get());

	SetSteamIdAttribute();
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
		SetAdminImmunity(0);
		return;
	}

	SetAdminFlags(admin->GetFlags());
	SetAdminImmunity(admin->GetImmunity());

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

CConVar<bool> g_cvarFlashLightShadows("cs2f_flashlight_shadows", FCVAR_NONE, "Whether to enable flashlight shadows", true);
CConVar<bool> g_cvarFlashLightTransmitOthers("cs2f_flashlight_transmit_others", FCVAR_NONE, "Whether to transmit other player's flashlights, recommended to have shadows off for this", false);
CConVar<float> g_cvarFlashLightBrightness("cs2f_flashlight_brightness", FCVAR_NONE, "How bright should flashlights be", 1.0f);
CConVar<float> g_cvarFlashLightDistance("cs2f_flashlight_distance", FCVAR_NONE, "How far flashlights should be from the player's head", 54.0f); // The minimum distance such that an awp wouldn't block the light
CConVar<Color> g_cvarFlashLightColor("cs2f_flashlight_color", FCVAR_NONE, "What color to use for flashlights", Color(255, 255, 255));
CConVar<CUtlString> g_cvarFlashLightAttachment("cs2f_flashlight_attachment", FCVAR_NONE, "Which attachment to parent a flashlight to. If the player model is not properly setup, you might have to use clip_limit here instead", "axis_of_intent");

void ZEPlayer::SpawnFlashLight()
{
	if (GetFlashLight())
		return;

	CCSPlayerPawn* pPawn = (CCSPlayerPawn*)CCSPlayerController::FromSlot(GetPlayerSlot())->GetPawn();

	Vector origin = pPawn->GetAbsOrigin();
	Vector forward;
	AngleVectors(pPawn->m_angEyeAngles(), &forward);

	origin.z += 64.0f;
	origin += forward * g_cvarFlashLightDistance.Get();

	CBarnLight* pLight = CreateEntityByName<CBarnLight>("light_barn");

	pLight->m_bEnabled = true;
	pLight->m_Color->SetColor(g_cvarFlashLightColor.Get().r(), g_cvarFlashLightColor.Get().g(), g_cvarFlashLightColor.Get().b());
	pLight->m_flBrightness = g_cvarFlashLightBrightness.Get();
	pLight->m_flRange = 2048.0f;
	pLight->m_flSoftX = 1.0f;
	pLight->m_flSoftY = 1.0f;
	pLight->m_flSkirt = 0.5f;
	pLight->m_flSkirtNear = 1.0f;
	pLight->m_vSizeParams->Init(45.0f, 45.0f, 0.02f);
	pLight->m_nCastShadows = g_cvarFlashLightShadows.Get();
	pLight->m_nDirectLight = 3;
	pLight->Teleport(&origin, &pPawn->m_angEyeAngles(), nullptr);

	// Have to use keyvalues for this since the schema prop is a resource handle
	CEntityKeyValues* pKeyValues = new CEntityKeyValues();
	pKeyValues->SetString("lightcookie", "materials/effects/lightcookies/flashlight.vtex");

	pLight->DispatchSpawn(pKeyValues);

	pLight->SetParent(pPawn);
	pLight->AcceptInput("SetParentAttachmentMaintainOffset", g_cvarFlashLightAttachment.Get().String());

	SetFlashLight(pLight);
}

void ZEPlayer::ToggleFlashLight()
{
	// Play the "click" sound
	CSingleRecipientFilter filter(GetPlayerSlot());
	CCSPlayerController::FromSlot(GetPlayerSlot())->EmitSoundFilter(filter, "HudChat.Message");

	CBarnLight* pLight = GetFlashLight();

	// Create a flashlight if we don't have one, and don't bother with the input since it spawns enabled
	if (!pLight)
	{
		SpawnFlashLight();
		return;
	}

	pLight->AcceptInput(pLight->m_bEnabled() ? "Disable" : "Enable");
}

CConVar<float> g_cvarFloodInterval("cs2f_flood_interval", FCVAR_NONE, "Amount of time allowed between chat messages acquiring flood tokens", 0.75f, true, 0.0f, false, 0.0f);
CConVar<int> g_cvarMaxFloodTokens("cs2f_max_flood_tokens", FCVAR_NONE, "Maximum number of flood tokens allowed before chat messages are blocked", 3, true, 0, false, 0);
CConVar<float> g_cvarFloodCooldown("cs2f_flood_cooldown", FCVAR_NONE, "Amount of time to block messages for when a player floods", 3.0f, true, 0.0f, false, 0.0f);
CConVar<CUtlString> g_cvarBeaconParticle("cs2f_beacon_particle", FCVAR_NONE, ".vpcf file to be precached and used for beacon", "particles/cs2fixes/player_beacon.vpcf");

bool ZEPlayer::IsFlooding()
{
	if (m_bGagged || !GetGlobals())
		return false;

	float time = GetGlobals()->curtime;
	float newTime = time + g_cvarFloodInterval.Get();

	if (m_flLastTalkTime >= time)
	{
		if (m_iFloodTokens >= g_cvarMaxFloodTokens.Get())
		{
			m_flLastTalkTime = newTime + g_cvarFloodCooldown.Get();
			return true;
		}
		else
		{
			m_iFloodTokens++;
		}
	}
	else if (m_iFloodTokens > 0)
	{
		// Remove one flood token when player chats within time limit (slow decay)
		m_iFloodTokens--;
	}

	m_flLastTalkTime = newTime;
	return false;
}

void PrecacheBeaconParticle(IEntityResourceManifest* pResourceManifest)
{
	pResourceManifest->AddResource(g_cvarBeaconParticle.Get().String());
}

void ZEPlayer::StartBeacon(Color color, ZEPlayerHandle hGiver /* = 0*/)
{
	SetBeaconColor(color);

	CCSPlayerController* pPlayer = CCSPlayerController::FromSlot(m_slot);

	Vector vecAbsOrigin = pPlayer->GetPawn()->GetAbsOrigin();

	vecAbsOrigin.z += 10;

	CParticleSystem* particle = CreateEntityByName<CParticleSystem>("info_particle_system");

	CEntityKeyValues* pKeyValues = new CEntityKeyValues();

	pKeyValues->SetString("effect_name", g_cvarBeaconParticle.Get().String());
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

	ZEPlayer* pGiver = hGiver.Get();
	if (pGiver && pGiver->IsLeader())
		bLeaderBeacon = true;

	new CTimer(1.0f, false, false, [hPlayer, hParticle, hGiver, iTeamNum, bLeaderBeacon]() {
		CParticleSystem* pParticle = hParticle.Get();

		if (!hPlayer.IsValid() || !pParticle)
			return -1.0f;

		CCSPlayerController* pPlayer = CCSPlayerController::FromSlot((CPlayerSlot)hPlayer.GetPlayerSlot());

		if (pPlayer->m_iTeamNum < CS_TEAM_T || !pPlayer->m_hPlayerPawn->IsAlive() || pPlayer->m_iTeamNum != iTeamNum)
		{
			addresses::UTIL_Remove(pParticle);
			return -1.0f;
		}

		if (!bLeaderBeacon)
			return 1.0f;

		ZEPlayer* pBeaconGiver = hGiver.Get();

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
	SetBeaconColor(Color(0, 0, 0, 0));

	CParticleSystem* pParticle = m_hBeaconParticle.Get();

	if (pParticle)
		addresses::UTIL_Remove(pParticle);
}

// Kills off currently active mark (if exists) and makes a new one.
// iDuration being non-positive only kills off active marks.
void ZEPlayer::CreateMark(float fDuration, Vector vecOrigin)
{
	if (m_handleMark && m_handleMark.IsValid())
	{
		UTIL_AddEntityIOEvent(m_handleMark.Get(), "DestroyImmediately", nullptr, nullptr, "", 0);
		UTIL_AddEntityIOEvent(m_handleMark.Get(), "Kill", nullptr, nullptr, "", 0.02f);
	}

	if (fDuration <= 0)
		return;

	CParticleSystem* pMarker = CreateEntityByName<CParticleSystem>("info_particle_system");
	CEntityKeyValues* pKeyValues = new CEntityKeyValues();

	pKeyValues->SetString("effect_name", g_cvarMarkParticlePath.Get().String());
	pKeyValues->SetInt("tint_cp", 1);
	pKeyValues->SetColor("tint_cp_color", GetLeaderColor());
	pKeyValues->SetVector("origin", vecOrigin);
	pKeyValues->SetBool("start_active", true);

	pMarker->DispatchSpawn(pKeyValues);

	UTIL_AddEntityIOEvent(pMarker, "DestroyImmediately", nullptr, nullptr, "", fDuration);
	UTIL_AddEntityIOEvent(pMarker, "Kill", nullptr, nullptr, "", fDuration + 0.02f);

	m_handleMark = pMarker->GetHandle(); // Save handle in case we need to kill mark earlier than above IO.
}

int ZEPlayer::GetLeaderVoteCount()
{
	int iValidVoteCount = 0;

	for (int i = m_vecLeaderVotes.Count() - 1; i >= 0; i--)
		if (m_vecLeaderVotes[i].IsValid())
			iValidVoteCount++;
		else
			m_vecLeaderVotes.Remove(i);

	return iValidVoteCount;
}

bool ZEPlayer::HasPlayerVotedLeader(ZEPlayer* pPlayer)
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
	SetGlowColor(color);
	CCSPlayerController* pController = CCSPlayerController::FromSlot(m_slot);
	CCSPlayerPawn* pPawn = (CCSPlayerPawn*)pController->GetPawn();

	const char* pszModelName = pPawn->GetModelName();

	CBaseModelEntity* pModelGlow = CreateEntityByName<CBaseModelEntity>("prop_dynamic");
	CBaseModelEntity* pModelRelay = CreateEntityByName<CBaseModelEntity>("prop_dynamic");
	CEntityKeyValues* pKeyValuesRelay = new CEntityKeyValues();

	pKeyValuesRelay->SetString("model", pszModelName);
	pKeyValuesRelay->SetInt64("spawnflags", 256U);
	pKeyValuesRelay->SetInt("rendermode", kRenderNone);

	CEntityKeyValues* pKeyValuesGlow = new CEntityKeyValues();
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
	new CTimer(0.5f, false, false, [hGlowModel, hPawn, iTeamNum]() {
		CBaseModelEntity* pModel = hGlowModel.Get();
		CCSPlayerPawn* pawn = hPawn.Get();

		if (!pawn || !pModel)
			return -1.0f;

		if (pawn->m_iTeamNum != iTeamNum || strcmp(pModel->GetModelName(), pawn->GetModelName()))
		{
			CGameSceneNode* pParentSceneNode = pModel->m_CBodyComponent()->m_pSceneNode()->m_pParent();

			if (!pParentSceneNode)
				return -1.0f;

			CBaseModelEntity* pModelParent = (CBaseModelEntity*)pParentSceneNode->m_pOwner();

			if (pModelParent)
				addresses::UTIL_Remove(pModelParent);

			return -1.0f;
		}

		return 0.5f;
	});

	// kill glow after duration, if provided
	if (duration < 1)
		return;

	new CTimer((float)duration, false, false, [hGlowModel]() {
		CBaseModelEntity* pModel = hGlowModel.Get();

		if (!pModel)
			return -1.0f;

		CGameSceneNode* pParentSceneNode = pModel->m_CBodyComponent()->m_pSceneNode()->m_pParent();

		if (!pParentSceneNode)
			return -1.0f;

		CBaseModelEntity* pModelParent = (CBaseModelEntity*)pParentSceneNode->m_pOwner();

		if (pModelParent)
			addresses::UTIL_Remove(pModelParent);

		return -1.0f;
	});
}

void ZEPlayer::EndGlow()
{
	SetGlowColor(Color(0, 0, 0, 0));

	CBaseModelEntity* pGlowModel = m_hGlowModel.Get();

	if (!pGlowModel)
		return;

	CGameSceneNode* pParentSceneNode = pGlowModel->m_CBodyComponent()->m_pSceneNode()->m_pParent();

	if (!pParentSceneNode)
		return;

	CBaseModelEntity* pModelParent = (CBaseModelEntity*)pParentSceneNode->m_pOwner();

	if (pModelParent)
		addresses::UTIL_Remove(pModelParent);
}

void ZEPlayer::CycleButtonWatch()
{
	m_iButtonWatchMode = (m_iButtonWatchMode + 1) % 4;
	g_pUserPreferencesSystem->SetPreferenceInt(m_slot.Get(), BUTTON_WATCH_PREF_KEY_NAME, m_iButtonWatchMode);
}

// 0: Off
// 1: Chat
// 2: Console
// 3: Chat + Console
int ZEPlayer::GetButtonWatchMode()
{
	if (!IsAdminFlagSet(ADMFLAG_GENERIC) || IsFakeClient())
		return 0;
	return g_pUserPreferencesSystem->GetPreferenceInt(m_slot.Get(), BUTTON_WATCH_PREF_KEY_NAME, m_iButtonWatchMode);
}

void ZEPlayer::SetSteamIdAttribute()
{
	if (!g_cvarEnableMapSteamIds.Get())
		return;

	if (!IsAuthenticated())
		return;

	const auto pController = CCSPlayerController::FromSlot(GetPlayerSlot());
	if (!pController || !pController->IsConnected() || pController->IsBot() || pController->m_bIsHLTV())
		return;

	const auto pPawn = pController->GetPlayerPawn();
	if (!pPawn)
		return;

	const auto& steamId = std::to_string(GetSteamId64());
	pPawn->AcceptInput("AddAttribute", steamId.c_str());
	pController->AcceptInput("AddAttribute", steamId.c_str());
}

void ZEPlayer::ReplicateConVar(const char* pszName, const char* pszValue)
{
	INetworkMessageInternal* pNetMsg = g_pNetworkMessages->FindNetworkMessagePartial("SetConVar");
	auto data = pNetMsg->AllocateMessage()->ToPB<CNETMsg_SetConVar>();

	CMsg_CVars_CVar* cvarMsg = data->mutable_convars()->add_cvars();
	cvarMsg->set_name(pszName);
	cvarMsg->set_value(pszValue);

	CSingleRecipientFilter filter(GetPlayerSlot());
	g_gameEventSystem->PostEventAbstract(-1, false, &filter, pNetMsg, data, 0);

	delete data;
}

CBaseViewModel* ZEPlayer::GetOrCreateCustomViewModel(CCSPlayerPawn* pPawn)
{
	CBaseViewModel* pViewmodel = pPawn->m_pViewModelServices()->GetViewModel(2);
	if (pViewmodel)
		return pViewmodel;

	pViewmodel = CreateEntityByName<CBaseViewModel>("predicted_viewmodel");
	if (!pViewmodel)
		return nullptr;

	pViewmodel->DispatchSpawn();
	pViewmodel->SetOwner(pPawn);
	pPawn->m_pViewModelServices()->SetViewModel(2, pViewmodel);

	return pViewmodel;
}

void ZEPlayer::CreateEntwatchHud()
{
	CCSPlayerController* pController = CCSPlayerController::FromSlot(GetPlayerSlot());
	if (!pController)
		return;

	CCSPlayerPawn* pPawn = pController->GetPlayerPawn();
	if (!pPawn)
		return;

	if (!pPawn->m_pViewModelServices())
		return;

	CBaseViewModel* pViewModel = GetOrCreateCustomViewModel(pPawn);
	if (!pViewModel)
	{
		Panic("Failed to get or create custom viewmodel for entwatch hud.\n");
		return;
	}

	CPointWorldText* pText = GetEntwatchHud();
	if (pText)
	{
		pText->Remove();
		pText = nullptr;
	}

	pText = CreateEntityByName<CPointWorldText>("point_worldtext");
	pText->m_bEnabled(true);
	pText->m_bFullbright(true);
	pText->m_flFontSize(GetEntwatchHudSize());

	Color col;
	int a = m_colorEntwatchHud.a();
	if (a == 255) // 254 allows it to be visible through some items
		col.SetColor(m_colorEntwatchHud.r(), m_colorEntwatchHud.g(), m_colorEntwatchHud.b(), 254);
	else
		col.SetColor(m_colorEntwatchHud.r(), m_colorEntwatchHud.g(), m_colorEntwatchHud.b(), a);

	pText->m_Color = col;

	pText->m_flWorldUnitsPerPx(0.005f);
	pText->m_nJustifyVertical(PointWorldTextJustifyVertical_t::POINT_WORLD_TEXT_JUSTIFY_VERTICAL_TOP);

	V_strncpy(pText->m_FontName, "Verdana Bold", 64);

	pText->SetMessage("");

	pText->DispatchSpawn();
	SetEntwatchHud(pText);

	pText->AcceptInput("SetParent", "!activator", pViewModel);

	Vector origin = pViewModel->GetAbsOrigin();
	QAngle vmangles = pViewModel->GetAbsRotation();

	Vector forward;
	Vector right;
	Vector up;
	AngleVectors(vmangles, &forward, &right, &up);

	origin += (forward * 7.0f);

	// -x = move left,  +x = move right
	origin += (right * GetEntwatchHudX());

	// -y = move up,   +y = move down
	origin -= (up * GetEntwatchHudY());

	QAngle angles;
	angles.x = 0.0f;
	angles.y = AngleNormalize(vmangles.y - 90.0f);
	angles.z = AngleNormalize(-vmangles.x + 90.0f);

	pText->Teleport(&origin, &angles, nullptr);
}

int ZEPlayer::GetEntwatchHudMode()
{
	if (IsFakeClient())
		return 0;
	return m_iEntwatchHudMode;
}

void ZEPlayer::SetEntwatchHudMode(int iMode)
{
	m_iEntwatchHudMode = iMode;
	g_pUserPreferencesSystem->SetPreferenceInt(m_slot.Get(), EW_PREF_HUD_MODE, m_iEntwatchHudMode);
}

void ZEPlayer::SetEntwatchClangtags(bool bStatus)
{
	m_bEntwatchClantags = bStatus;
	g_pUserPreferencesSystem->SetPreferenceInt(m_slot.Get(), EW_PREF_CLANTAG, bStatus ? 1 : 0);
}

void ZEPlayer::SetEntwatchHudColor(Color colorHud)
{
	m_colorEntwatchHud = colorHud;
	std::string strColor = std::to_string(colorHud.r()) + " " + std::to_string(colorHud.g()) + " " + std::to_string(colorHud.b()) + " " + std::to_string(colorHud.a());
	g_pUserPreferencesSystem->SetPreference(m_slot.Get(), EW_PREF_HUDCOLOR, strColor.c_str());

	CPointWorldText* pText = GetEntwatchHud();
	if (pText)
	{
		Color c;
		if (colorHud.a() == 255)
			c.SetColor(colorHud.r(), colorHud.g(), colorHud.b(), 254);
		else
			c = colorHud;
		pText->m_Color = c;
	}
}

void ZEPlayer::SetEntwatchHudPos(float x, float y)
{
	m_flEntwatchHudX = x;
	m_flEntwatchHudY = y;
	g_pUserPreferencesSystem->SetPreferenceFloat(m_slot.Get(), EW_PREF_HUDPOS_X, m_flEntwatchHudX);
	g_pUserPreferencesSystem->SetPreferenceFloat(m_slot.Get(), EW_PREF_HUDPOS_Y, m_flEntwatchHudY);

	CreateEntwatchHud();
}

void ZEPlayer::SetEntwatchHudSize(float flSize)
{
	m_flEntwatchHudSize = flSize;
	g_pUserPreferencesSystem->SetPreferenceFloat(m_slot.Get(), EW_PREF_HUDSIZE, m_flEntwatchHudSize);

	CPointWorldText* pText = GetEntwatchHud();
	if (pText)
		pText->m_flFontSize = m_flEntwatchHudSize;
}

void CPlayerManager::OnBotConnected(CPlayerSlot slot)
{
	m_vecPlayers[slot.Get()] = new ZEPlayer(slot, true);
}

bool CPlayerManager::OnClientConnected(CPlayerSlot slot, uint64 xuid, const char* pszNetworkID)
{
	Assert(m_vecPlayers[slot.Get()] == nullptr);

	Message("%d connected\n", slot.Get());

	ZEPlayer* pPlayer = new ZEPlayer(slot);
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

	// Sometimes clients can be already auth'd at this point
	if (g_pEngineServer2->IsClientFullyAuthenticated(slot))
		pPlayer->OnAuthenticated();

	pPlayer->SetConnected();
	m_vecPlayers[slot.Get()] = pPlayer;

	ResetPlayerFlags(slot.Get());

	g_pMapVoteSystem->ClearPlayerInfo(slot.Get());
	g_pMapVoteSystem->ClearInvalidNominations();

	return true;
}

void CPlayerManager::OnClientDisconnect(CPlayerSlot slot)
{
	Message("%d disconnected\n", slot.Get());

	g_pUserPreferencesSystem->PushPreferences(slot.Get());
	g_pUserPreferencesSystem->ClearPreferences(slot.Get());

	if (g_cvarEnableEntWatch.Get())
		EW_PlayerDisconnect(slot.Get());

	delete m_vecPlayers[slot.Get()];
	m_vecPlayers[slot.Get()] = nullptr;

	ResetPlayerFlags(slot.Get());

	g_pMapVoteSystem->ClearPlayerInfo(slot.Get());
	g_pMapVoteSystem->ClearInvalidNominations();

	// One tick delay, to ensure player count decrements
	new CTimer(0.01f, false, true, []() {
		g_pVoteManager->CheckRTVStatus();
		return -1.0f;
	});

	g_pPanoramaVoteHandler->RemovePlayerFromVote(slot.Get());
}

void CPlayerManager::OnClientPutInServer(CPlayerSlot slot)
{
	ZEPlayer* pPlayer = m_vecPlayers[slot.Get()];

	pPlayer->SetInGame(true);
}

void CPlayerManager::OnLateLoad()
{
	if (!GetGlobals())
		return;

	for (int i = 0; i < GetGlobals()->maxClients; i++)
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

CConVar<int> g_cvarDelayAuthFailKick("cs2f_delay_auth_fail_kick", FCVAR_NONE, "How long in seconds to delay kicking players when their Steam authentication fails, use with sv_steamauth_enforce 0", 0, true, 0, false, 0);

void CPlayerManager::OnValidateAuthTicket(ValidateAuthTicketResponse_t* pResponse)
{
	uint64 iSteamId = pResponse->m_SteamID.ConvertToUint64();

	Message("%s: SteamID=%llu Response=%d\n", __func__, iSteamId, pResponse->m_eAuthSessionResponse);

	ZEPlayer* pPlayer = nullptr;

	for (ZEPlayer* pPlayer : m_vecPlayers)
	{
		if (!pPlayer || pPlayer->IsFakeClient() || !(pPlayer->GetUnauthenticatedSteamId64() == iSteamId))
			continue;

		CCSPlayerController* pController = CCSPlayerController::FromSlot(pPlayer->GetPlayerSlot());

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
				if (!g_cvarDelayAuthFailKick.Get())
					return;

				ClientPrint(pController, HUD_PRINTTALK, " \7Your Steam authentication failed due to an invalid or used ticket.");
				ClientPrint(pController, HUD_PRINTTALK, " \7You may have to restart your Steam client in order to fix this.\n");
				[[fallthrough]];
			}

			default:
			{
				if (!g_cvarDelayAuthFailKick.Get())
					return;

				ClientPrint(pController, HUD_PRINTTALK, " \7WARNING: You will be kicked in %i seconds due to failed Steam authentication.\n", g_cvarDelayAuthFailKick.Get());

				ZEPlayerHandle hPlayer = pPlayer->GetHandle();
				new CTimer(g_cvarDelayAuthFailKick.Get(), true, true, [hPlayer]() {
					if (!hPlayer.IsValid())
						return -1.f;

					g_pEngineServer2->DisconnectClient(hPlayer.GetPlayerSlot(), NETWORK_DISCONNECT_KICKED_NOSTEAMLOGIN, "Auto kicked for failed steam authentication");
					return -1.f;
				});
			}
		}
	}
}

void CPlayerManager::CheckInfractions()
{
	if (!GetGlobals())
		return;

	for (int i = 0; i < GetGlobals()->maxClients; i++)
	{
		if (m_vecPlayers[i] == nullptr || m_vecPlayers[i]->IsFakeClient())
			continue;

		m_vecPlayers[i]->CheckInfractions();
	}

	g_pAdminSystem->SaveInfractions();
}

CConVar<bool> g_cvarFlashLightEnable("cs2f_flashlight_enable", FCVAR_NONE, "Whether to enable flashlights", false);

void CPlayerManager::FlashLightThink()
{
	if (!g_cvarFlashLightEnable.Get() || !GetGlobals())
		return;

	VPROF("CPlayerManager::FlashLightThink");

	for (int i = 0; i < GetGlobals()->maxClients; i++)
	{
		CCSPlayerController* pPlayer = CCSPlayerController::FromSlot(i);

		if (!pPlayer || !pPlayer->m_bPawnIsAlive())
			continue;

		uint64* pButtons = pPlayer->GetPawn()->m_pMovementServices->m_nButtons().m_pButtonStates();

		// Check both to make sure flashlight is only toggled when the player presses the key
		if ((pButtons[0] & IN_LOOK_AT_WEAPON) && (pButtons[1] & IN_LOOK_AT_WEAPON))
			pPlayer->GetZEPlayer()->ToggleFlashLight();
	}
}

CConVar<bool> g_cvarHideTeammatesOnly("cs2f_hide_teammates_only", FCVAR_NONE, "Whether to hide teammates only", false);

void CPlayerManager::CheckHideDistances()
{
	if (!g_pEntitySystem || !GetGlobals())
		return;

	VPROF("CPlayerManager::CheckHideDistances");

	for (int i = 0; i < GetGlobals()->maxClients; i++)
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

		for (int j = 0; j < GetGlobals()->maxClients; j++)
		{
			if (j == i)
				continue;

			CCSPlayerController* pTargetController = CCSPlayerController::FromSlot(j);

			if (pTargetController)
			{
				auto pTargetPawn = pTargetController->GetPawn();

				// TODO: Unhide dead pawns if/when valve fixes the crash
				if (pTargetPawn && (!g_cvarHideTeammatesOnly.Get() || pTargetController->m_iTeamNum == team))
					player->SetTransmit(j, pTargetPawn->GetAbsOrigin().DistToSqr(vecPosition) <= hideDistance * hideDistance);
			}
		}
	}
}

static const char* g_szPlayerStates[] =
	{
		"STATE_ACTIVE",
		"STATE_WELCOME",
		"STATE_PICKINGTEAM",
		"STATE_PICKINGCLASS",
		"STATE_DEATH_ANIM",
		"STATE_DEATH_WAIT_FOR_KEY",
		"STATE_OBSERVER_MODE",
		"STATE_GUNGAME_RESPAWN",
		"STATE_DORMANT"};

extern CConVar<bool> g_cvarEnableHide;

void CPlayerManager::UpdatePlayerStates()
{
	if (!GetGlobals())
		return;

	for (int i = 0; i < GetGlobals()->maxClients; i++)
	{
		ZEPlayer* pPlayer = GetPlayer(i);

		if (!pPlayer)
			continue;

		CCSPlayerController* pController = CCSPlayerController::FromSlot(i);

		if (!pController)
			continue;

		uint32 iPreviousPlayerState = pPlayer->GetPlayerState();
		uint32 iCurrentPlayerState = pController->GetPawnState();

		if (iCurrentPlayerState != iPreviousPlayerState)
		{
			if (g_cvarEnableHide.Get())
				Message("Player %s changed states from %s to %s\n", pController->GetPlayerName(), g_szPlayerStates[iPreviousPlayerState], g_szPlayerStates[iCurrentPlayerState]);

			pPlayer->SetPlayerState(iCurrentPlayerState);

			// Send full update to people going in/out of spec as a mitigation for hide crashes
			if (g_cvarEnableHide.Get() && (iCurrentPlayerState == STATE_OBSERVER_MODE || iPreviousPlayerState == STATE_OBSERVER_MODE))
			{
				CServerSideClient* pClient = GetClientBySlot(i);

				if (pClient)
					pClient->ForceFullUpdate();
			}
		}
	}
}

CConVar<bool> g_cvarInfiniteAmmo("cs2f_infinite_reserve_ammo", FCVAR_NONE, "Whether to enable infinite reserve ammo on weapons", false);

void CPlayerManager::SetupInfiniteAmmo()
{
	new CTimer(5.0f, false, true, []() {
		if (!g_cvarInfiniteAmmo.Get() || !GetGlobals())
			return 5.0f;

		VPROF("CPlayerManager::InfiniteAmmoTimer");

		for (int i = 0; i < GetGlobals()->maxClients; i++)
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

// Returns ETargetError::NO_ERRORS if pPlayer can target pTarget given iBlockedFlags and iOnlyThisTeam
// otherwise returns the error encountered in targetting that pTarget, with ETargetError::INVALID given
// iOnlyThisTeam is non-zero and doesnt match pTarget
ETargetError GetTargetError(CCSPlayerController* pPlayer, CCSPlayerController* pTarget, uint64 iBlockedFlags, int iOnlyThisTeam = -1)
{
	if (!pTarget || !pTarget->IsController() || !pTarget->IsConnected() || pTarget->m_bIsHLTV)
		return ETargetError::INVALID;
	else if (!pTarget->IsConnected())
		return ETargetError::CONNECTING;
	else if (iOnlyThisTeam > -1 && pTarget->m_iTeamNum() != iOnlyThisTeam)
		return ETargetError::INVALID;
	else if (iBlockedFlags & NO_SELF && pTarget == pPlayer)
		return ETargetError::SELF;
	else if (iBlockedFlags & NO_TERRORIST && pTarget->m_iTeamNum() == CS_TEAM_T)
		return ETargetError::TERRORIST;
	else if (iBlockedFlags & NO_COUNTER_TERRORIST && pTarget->m_iTeamNum() == CS_TEAM_CT)
		return ETargetError::COUNTER_TERRORIST;
	else if (iBlockedFlags & NO_SPECTATOR && pTarget->m_iTeamNum() <= CS_TEAM_SPECTATOR)
		return ETargetError::SPECTATOR;
	else if (iBlockedFlags & NO_DEAD && (!pTarget->m_bPawnIsAlive() || pTarget->m_iTeamNum() <= CS_TEAM_SPECTATOR))
		return ETargetError::DEAD;
	else if (iBlockedFlags & NO_ALIVE && pTarget->m_bPawnIsAlive())
		return ETargetError::ALIVE;

	ZEPlayer* zpPlayer = pPlayer ? pPlayer->GetZEPlayer() : nullptr;
	ZEPlayer* zpTarget = pTarget->GetZEPlayer();

	if (!zpTarget)
		return ETargetError::INVALID;
	else if (iBlockedFlags & NO_BOT && zpTarget->IsFakeClient())
		return ETargetError::BOT;
	else if (iBlockedFlags & NO_HUMAN && !zpTarget->IsFakeClient())
		return ETargetError::HUMAN;
	else if (iBlockedFlags & NO_UNAUTHENTICATED && !zpTarget->IsAuthenticated())
		return ETargetError::UNAUTHENTICATED;
	else if (zpPlayer && !(iBlockedFlags & NO_IMMUNITY)
			 && ((g_cvarAdminImmunityTargetting.Get() == 0 && zpTarget->GetAdminImmunity() > zpPlayer->GetAdminImmunity())
				 || (g_cvarAdminImmunityTargetting.Get() == 1 && zpTarget->GetAdminImmunity() <= zpPlayer->GetAdminImmunity() && pTarget != pPlayer)))
		return ETargetError::INSUFFICIENT_IMMUNITY_LEVEL;

	return ETargetError::NO_ERRORS;
}

ETargetError CPlayerManager::GetPlayersFromString(CCSPlayerController* pPlayer, const char* pszTarget,
												  int& iNumClients, int* rgiClients, uint64 iBlockedFlags,
												  ETargetType& nType)
{
	if (!GetGlobals())
		return ETargetError::INVALID;

	nType = ETargetType::NONE;
	ZEPlayer* zpPlayer = pPlayer ? pPlayer->GetZEPlayer() : nullptr;
	bool bTargetMultiple = false;
	bool bTargetRandom = false;
	uint64 iInverseFlags = NO_TARGET_BLOCKS;

	if (!V_stricmp(pszTarget, "@me"))
	{
		nType = ETargetType::SELF;

		if (iBlockedFlags & NO_SELF || !pPlayer)
			return ETargetError::SELF;
	}
	else if (!V_stricmp(pszTarget, "@!me"))
	{
		nType = ETargetType::ALL_BUT_SELF;

		if (iBlockedFlags & NO_MULTIPLE)
			return ETargetError::MULTIPLE;

		iBlockedFlags |= NO_SELF;
		bTargetMultiple = true;
	}
	else if (!V_stricmp(pszTarget, "@all"))
	{
		nType = ETargetType::ALL;

		if (iBlockedFlags & NO_MULTIPLE)
			return ETargetError::MULTIPLE;

		bTargetMultiple = true;
	}
	else if (!V_stricmp(pszTarget, "@!all"))
		return ETargetError::INVALID;
	else if (!V_stricmp(pszTarget, "@t"))
	{
		nType = ETargetType::T;

		if (iBlockedFlags & NO_TERRORIST)
			return ETargetError::TERRORIST;
		else if (iBlockedFlags & NO_MULTIPLE)
			return ETargetError::MULTIPLE;

		bTargetMultiple = true;
		iBlockedFlags |= NO_COUNTER_TERRORIST | NO_SPECTATOR;
	}
	else if (!V_stricmp(pszTarget, "@!t"))
	{
		nType = ETargetType::ALL_BUT_T;

		if (iBlockedFlags & NO_MULTIPLE)
			return ETargetError::MULTIPLE;

		bTargetMultiple = true;
		iBlockedFlags |= NO_TERRORIST;
	}
	else if (!V_stricmp(pszTarget, "@ct"))
	{
		nType = ETargetType::CT;

		if (iBlockedFlags & NO_COUNTER_TERRORIST)
			return ETargetError::COUNTER_TERRORIST;
		else if (iBlockedFlags & NO_MULTIPLE)
			return ETargetError::MULTIPLE;

		bTargetMultiple = true;
		iBlockedFlags |= NO_TERRORIST | NO_SPECTATOR;
	}
	else if (!V_stricmp(pszTarget, "@!ct"))
	{
		nType = ETargetType::ALL_BUT_CT;

		if (iBlockedFlags & NO_MULTIPLE)
			return ETargetError::MULTIPLE;

		bTargetMultiple = true;
		iBlockedFlags |= NO_COUNTER_TERRORIST;
	}
	else if (!V_stricmp(pszTarget, "@spec"))
	{
		nType = ETargetType::SPECTATOR;

		if (iBlockedFlags & NO_SPECTATOR)
			return ETargetError::SPECTATOR;
		else if (iBlockedFlags & NO_DEAD)
			return ETargetError::DEAD;
		else if (iBlockedFlags & NO_MULTIPLE)
			return ETargetError::MULTIPLE;

		bTargetMultiple = true;
		iBlockedFlags |= NO_TERRORIST | NO_COUNTER_TERRORIST;
	}
	else if (!V_stricmp(pszTarget, "@!spec"))
	{
		nType = ETargetType::ALL_BUT_SPECTATOR;

		if (iBlockedFlags & NO_MULTIPLE)
			return ETargetError::MULTIPLE;

		bTargetMultiple = true;
		iBlockedFlags |= NO_SPECTATOR;
	}
	else if (!V_stricmp(pszTarget, "@random"))
	{
		nType = ETargetType::RANDOM;

		if (iBlockedFlags & NO_RANDOM)
			return ETargetError::RANDOM;

		bTargetRandom = true;
	}
	else if (!V_stricmp(pszTarget, "@!random"))
	{
		nType = ETargetType::ALL_BUT_RANDOM;

		if (iBlockedFlags & NO_RANDOM)
			return ETargetError::RANDOM;

		iInverseFlags = NO_RANDOM;
	}
	else if (!V_stricmp(pszTarget, "@randomt"))
	{
		nType = ETargetType::RANDOM_T;

		if (iBlockedFlags & NO_TERRORIST)
			return ETargetError::TERRORIST;
		else if (iBlockedFlags & NO_RANDOM)
			return ETargetError::RANDOM;

		bTargetRandom = true;
		iBlockedFlags |= NO_COUNTER_TERRORIST;
	}
	else if (!V_stricmp(pszTarget, "@!randomt"))
	{
		nType = ETargetType::ALL_BUT_RANDOM_T;

		if (iBlockedFlags & NO_RANDOM)
			return ETargetError::RANDOM;

		iInverseFlags = NO_RANDOM | NO_COUNTER_TERRORIST | NO_SPECTATOR;
	}
	else if (!V_stricmp(pszTarget, "@randomct"))
	{
		nType = ETargetType::RANDOM_CT;

		if (iBlockedFlags & NO_COUNTER_TERRORIST)
			return ETargetError::COUNTER_TERRORIST;
		else if (iBlockedFlags & NO_RANDOM)
			return ETargetError::RANDOM;

		bTargetRandom = true;
		iBlockedFlags |= NO_TERRORIST;
	}
	else if (!V_stricmp(pszTarget, "@!randomct"))
	{
		nType = ETargetType::RANDOM_CT;

		if (iBlockedFlags & NO_RANDOM)
			return ETargetError::RANDOM;

		iInverseFlags = NO_RANDOM | NO_TERRORIST | NO_SPECTATOR;
	}
	else if (!V_stricmp(pszTarget, "@randomspec"))
	{
		nType = ETargetType::RANDOM_SPEC;

		if (iBlockedFlags & NO_SPECTATOR)
			return ETargetError::SPECTATOR;
		else if (iBlockedFlags & NO_DEAD)
			return ETargetError::DEAD;
		else if (iBlockedFlags & NO_RANDOM)
			return ETargetError::RANDOM;

		bTargetRandom = true;
		iBlockedFlags |= NO_TERRORIST | NO_COUNTER_TERRORIST;
	}
	else if (!V_stricmp(pszTarget, "@!randomspec"))
	{
		nType = ETargetType::ALL_BUT_RANDOM_SPEC;

		if (iBlockedFlags & NO_RANDOM)
			return ETargetError::RANDOM;

		iInverseFlags = NO_RANDOM | NO_TERRORIST | NO_COUNTER_TERRORIST;
	}
	else if (!V_stricmp(pszTarget, "@dead") || !V_stricmp(pszTarget, "@!alive"))
	{
		nType = ETargetType::DEAD;

		if (iBlockedFlags & NO_DEAD)
			return ETargetError::DEAD;
		if (iBlockedFlags & NO_MULTIPLE)
			return ETargetError::MULTIPLE;

		bTargetMultiple = true;
		iBlockedFlags |= NO_ALIVE;
	}
	else if (!V_stricmp(pszTarget, "@alive") || !V_stricmp(pszTarget, "@!dead"))
	{
		nType = ETargetType::ALIVE;

		if (iBlockedFlags & NO_ALIVE)
			return ETargetError::ALIVE;
		if (iBlockedFlags & NO_MULTIPLE)
			return ETargetError::MULTIPLE;

		bTargetMultiple = true;
		iBlockedFlags |= NO_DEAD;
	}
	else if (!V_stricmp(pszTarget, "@bot") || !V_stricmp(pszTarget, "@!human"))
	{
		nType = ETargetType::BOT;

		if (iBlockedFlags & NO_BOT)
			return ETargetError::BOT;
		if (iBlockedFlags & NO_MULTIPLE)
			return ETargetError::MULTIPLE;

		bTargetMultiple = true;
		iBlockedFlags |= NO_HUMAN;
	}
	else if (!V_stricmp(pszTarget, "@human") || !V_stricmp(pszTarget, "@!bot"))
	{
		nType = ETargetType::HUMAN;

		if (iBlockedFlags & NO_HUMAN)
			return ETargetError::HUMAN;
		if (iBlockedFlags & NO_MULTIPLE)
			return ETargetError::MULTIPLE;

		bTargetMultiple = true;
		iBlockedFlags |= NO_BOT;
	}

	// We have setup what we need and given custom errors if needed for group targetting.
	// Now we actually get the target(s).
	if (nType == ETargetType::SELF)
	{
		ETargetError eType = GetTargetError(pPlayer, pPlayer, iBlockedFlags);
		if (eType != ETargetError::NO_ERRORS)
			return eType;

		rgiClients[iNumClients++] = zpPlayer->GetPlayerSlot().Get();
		return ETargetError::NO_ERRORS;
	}
	else if (bTargetMultiple)
	{
		for (int i = 0; i < GetGlobals()->maxClients; i++)
		{
			if (m_vecPlayers[i] == nullptr)
				continue;

			CCSPlayerController* pTarget = CCSPlayerController::FromSlot(i);

			if (GetTargetError(pPlayer, pTarget, iBlockedFlags) == ETargetError::NO_ERRORS)
				rgiClients[iNumClients++] = i;
		}
	}
	else if (bTargetRandom)
	{
		int iAttempts = 0;

		while (iNumClients == 0 && iAttempts < 10000)
		{
			int iSlot = rand() % (GetGlobals()->maxClients - 1);

			// Prevent infinite loop
			iAttempts++;

			if (m_vecPlayers[iSlot] == nullptr)
				continue;

			CCSPlayerController* pTarget = CCSPlayerController::FromSlot(iSlot);
			if (GetTargetError(pPlayer, pTarget, iBlockedFlags) == ETargetError::NO_ERRORS)
			{
				rgiClients[iNumClients++] = iSlot;
				break;
			}
		}
	}
	else if (iInverseFlags > NO_TARGET_BLOCKS)
	{
		CCSPlayerController* pRandomPlayer = nullptr;
		int iAttempts = 0;

		while (iNumClients == 0 && iAttempts < 10000)
		{
			int iSlot = rand() % (GetGlobals()->maxClients - 1);

			// Prevent infinite loop
			iAttempts++;

			if (m_vecPlayers[iSlot] == nullptr)
				continue;

			CCSPlayerController* pTarget = CCSPlayerController::FromSlot(iSlot);

			// Can ignore immunity and blocked flags here, since we are NOT targetting them
			if (GetTargetError(pPlayer, pTarget, iInverseFlags | NO_IMMUNITY) == ETargetError::NO_ERRORS)
			{
				pRandomPlayer = pTarget;
				break;
			}
		}

		if (pRandomPlayer == nullptr)
			return ETargetError::INVALID;

		for (int i = 0; i < GetGlobals()->maxClients; i++)
		{
			if (m_vecPlayers[i] == nullptr)
				continue;

			CCSPlayerController* pTarget = CCSPlayerController::FromSlot(i);
			if (pRandomPlayer == pTarget)
				continue;

			if (GetTargetError(pPlayer, pTarget, iBlockedFlags) == ETargetError::NO_ERRORS)
				rgiClients[iNumClients++] = i;
		}
	}
	else if (!V_stricmp(pszTarget, "@aim"))
	{
		CBaseEntity* entTarget = nullptr;
		entTarget = UTIL_FindPickerEntity(pPlayer);

		if (!entTarget || !entTarget->IsPawn())
			return ETargetError::INVALID;

		CCSPlayerController* pTarget = CCSPlayerController::FromPawn(static_cast<CCSPlayerPawn*>(entTarget));

		ETargetError eType = GetTargetError(pPlayer, pTarget, iBlockedFlags);
		if (eType != ETargetError::NO_ERRORS)
			return eType;

		nType = ETargetType::AIM;

		rgiClients[iNumClients++] = pTarget->GetPlayerSlot();
	}
	else if (!V_stricmp(pszTarget, "@!aim"))
	{
		CBaseEntity* entTarget = nullptr;
		entTarget = UTIL_FindPickerEntity(pPlayer);

		if (!entTarget || !entTarget->IsPawn())
			return ETargetError::INVALID;

		CCSPlayerController* pAimed = CCSPlayerController::FromPawn(static_cast<CCSPlayerPawn*>(entTarget));

		// Can ignore immunity and blocked flags here, since we are NOT targetting them
		if (GetTargetError(pPlayer, pAimed, NO_IMMUNITY) != ETargetError::NO_ERRORS)
			return ETargetError::INVALID;

		nType = ETargetType::ALL_BUT_AIM;

		for (int i = 0; i < GetGlobals()->maxClients; i++)
		{
			if (m_vecPlayers[i] == nullptr)
				continue;

			CCSPlayerController* pTarget = CCSPlayerController::FromSlot(i);
			if (pAimed == pTarget)
				continue;

			if (GetTargetError(pPlayer, pTarget, iBlockedFlags) == ETargetError::NO_ERRORS)
				rgiClients[iNumClients++] = i;
		}
	}
	else if (*pszTarget == '#')
	{
		int iUserID = V_StringToUint16(pszTarget + 1, -1);

		if (iUserID != -1)
		{
			nType = ETargetType::PLAYER;
			CCSPlayerController* pTarget = CCSPlayerController::FromSlot(GetSlotFromUserId(iUserID).Get());
			ETargetError eType = GetTargetError(pPlayer, pTarget, iBlockedFlags);
			if (eType != ETargetError::NO_ERRORS)
				return eType;
			rgiClients[iNumClients++] = pTarget->GetPlayerSlot();
		}
	}
	else if (*pszTarget == '$')
	{
		uint64 iSteamID = V_StringToUint64(pszTarget + 1, -1);

		if (iSteamID != -1)
		{
			nType = ETargetType::PLAYER;
			ZEPlayer* zpTarget = GetPlayerFromSteamId(iSteamID);
			if (!zpTarget)
				return ETargetError::INVALID;

			CCSPlayerController* pTarget = CCSPlayerController::FromSlot(zpTarget->GetPlayerSlot().Get());

			ETargetError eType = GetTargetError(pPlayer, pTarget, iBlockedFlags);
			if (eType != ETargetError::NO_ERRORS)
				return eType;
			rgiClients[iNumClients++] = pTarget->GetPlayerSlot();
		}
	}
	else
	{
		ETargetError eType = ETargetError::NO_ERRORS;
		bool bExactName = (*pszTarget == '&');
		if (bExactName)
			pszTarget++;

		for (int i = 0; i < GetGlobals()->maxClients; i++)
		{
			if (m_vecPlayers[i] == nullptr)
				continue;

			CCSPlayerController* pTarget = CCSPlayerController::FromSlot(i);

			if (!pTarget || !pTarget->IsController() || !pTarget->IsConnected() || pTarget->m_bIsHLTV)
				continue;

			if ((!bExactName && V_stristr(pTarget->GetPlayerName(), pszTarget)) || !V_strcmp(pTarget->GetPlayerName(), pszTarget))
			{
				nType = ETargetType::PLAYER;
				if (iNumClients == 1)
				{
					iNumClients = 0;
					return ETargetError::MULTIPLE_NAME_MATCHES;
				}
				eType = GetTargetError(pPlayer, pTarget, iBlockedFlags);
				if (eType == ETargetError::NO_ERRORS)
					rgiClients[iNumClients++] = i;
			}
		}
		if (eType != ETargetError::NO_ERRORS)
			return eType;
	}

	return iNumClients ? ETargetError::NO_ERRORS : ETargetError::INVALID;
}

ETargetError CPlayerManager::GetPlayersFromString(CCSPlayerController* pPlayer, const char* pszTarget,
												  int& iNumClients, int* rgiClients, uint64 iBlockedFlags)
{
	ETargetType nUselessVariable = ETargetType::NONE;
	return GetPlayersFromString(pPlayer, pszTarget, iNumClients, rgiClients, iBlockedFlags, nUselessVariable);
}

std::string CPlayerManager::GetErrorString(ETargetError eType, int iSlot)
{
	switch (eType)
	{
		case ETargetError::INVALID:
			return "No matching player was found.";
		case ETargetError::CONNECTING:
			return "This action cannot be performed on connecting players. Please wait a moment and try again.";
		case ETargetError::MULTIPLE_NAME_MATCHES:
			return "More than one player matched the given pattern. Consider using & before the player's name for exact matching.";
		case ETargetError::RANDOM:
			return "This action cannot be performed on random players.";
		case ETargetError::MULTIPLE:
			return "This action cannot be performed on multiple players.";
		case ETargetError::SELF:
			return "This action cannot be performed on yourself.";
		case ETargetError::BOT:
			return "This action cannot be performed on bots.";
		case ETargetError::HUMAN:
			return "This action can only be performed on bots.";
		case ETargetError::DEAD:
			return "This action can only be performed on alive players.";
		case ETargetError::ALIVE:
			return "This action cannot be performed on alive players.";
		case ETargetError::TERRORIST:
			return "This action cannot be performed on terrorists.";
		case ETargetError::COUNTER_TERRORIST:
			return "This action cannot be performed on counter-terrorists.";
		case ETargetError::SPECTATOR:
			return "This action cannot be performed on spectators.";
	}

	CCSPlayerController* pPlayer = iSlot ? CCSPlayerController::FromSlot(iSlot) : nullptr;
	std::string strName = pPlayer ? pPlayer->GetPlayerName() : "";
	if (strName.length() == 0)
	{
		switch (eType)
		{
			case ETargetError::UNAUTHENTICATED:
				return "This action cannot be performed on unauthenticated players. Please wait a moment and try again.";
			case ETargetError::INSUFFICIENT_IMMUNITY_LEVEL:
				return "You do not have permission to target this player.";
		}
	}
	else
	{
		switch (eType)
		{
			case ETargetError::UNAUTHENTICATED:
				return strName + " is not yet authenticated. Please wait a moment and try again.";
			case ETargetError::INSUFFICIENT_IMMUNITY_LEVEL:
				return "You do not have permission to target " + strName + ".";
		}
	}

	// Should never reach here unless an ETargetError type was forgotten somewhere above.
	return "Encountered an unknown ETargetError, please contact a dev with the exact command used.";
}

// Return false if GetPlayersFromString returns anything other than NO_ERRORS and then print the
// error to pPlayer's chat. Otherwise return true and print nothing.
bool CPlayerManager::CanTargetPlayers(CCSPlayerController* pPlayer, const char* pszTarget,
									  int& iNumClients, int* rgiClients, uint64 iBlockedFlags,
									  ETargetType& nType)
{
	ETargetError eType = GetPlayersFromString(pPlayer, pszTarget, iNumClients, rgiClients, iBlockedFlags, nType);

	if (eType != ETargetError::NO_ERRORS)
	{
		ClientPrint(pPlayer, HUD_PRINTTALK, CHAT_PREFIX "%s", g_playerManager->GetErrorString(eType, (iNumClients == 0) ? 0 : pPlayer->GetPlayerSlot()).c_str());
		return false;
	}
	return true;
}

bool CPlayerManager::CanTargetPlayers(CCSPlayerController* pPlayer, const char* pszTarget,
									  int& iNumClients, int* rgiClients, uint64 iBlockedFlags)
{
	ETargetType nUselessVariable = ETargetType::NONE;
	return CanTargetPlayers(pPlayer, pszTarget, iNumClients, rgiClients, iBlockedFlags, nUselessVariable);
}

ZEPlayer* CPlayerManager::GetPlayer(CPlayerSlot slot)
{
	if (slot.Get() < 0 || slot.Get() >= MAXPLAYERS)
		return nullptr;

	return m_vecPlayers[slot.Get()];
};

// In userids, the lower byte is always the player slot
CPlayerSlot CPlayerManager::GetSlotFromUserId(uint16 userid)
{
	return CPlayerSlot(userid & 0xFF);
}

ZEPlayer* CPlayerManager::GetPlayerFromUserId(uint16 userid)
{
	uint8 index = userid & 0xFF;

	if (index >= MAXPLAYERS)
		return nullptr;

	return m_vecPlayers[index];
}

ZEPlayer* CPlayerManager::GetPlayerFromSteamId(uint64 steamid)
{
	for (ZEPlayer* player : m_vecPlayers)
		if (player && player->IsAuthenticated() && player->GetSteamId64() == steamid)
			return player;

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
	int iStopPreferenceStatus = (m_nUsingStopSound & iSlotMask) ? 1 : 0;
	int iSilencePreferenceStatus = (m_nUsingSilenceSound & iSlotMask) ? 2 : 0;
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
	int iStopPreferenceStatus = (m_nUsingStopSound & iSlotMask) ? 1 : 0;
	int iSilencePreferenceStatus = (m_nUsingSilenceSound & iSlotMask) ? 2 : 0;
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
	int iDecalPreferenceStatus = (m_nUsingStopDecals & iSlotMask) ? 1 : 0;
	g_pUserPreferencesSystem->SetPreferenceInt(slot, DECAL_PREF_KEY_NAME, iDecalPreferenceStatus);
}

void CPlayerManager::SetPlayerNoShake(int slot, bool set)
{
	if (set)
		m_nUsingNoShake |= ((uint64)1 << slot);
	else
		m_nUsingNoShake &= ~((uint64)1 << slot);

	// Set the user prefs if the player is ingame
	ZEPlayer* pPlayer = m_vecPlayers[slot];
	if (!pPlayer) return;

	uint64 iSlotMask = (uint64)1 << slot;
	int iNoShakePreferenceStatus = (m_nUsingNoShake & iSlotMask) ? 1 : 0;
	g_pUserPreferencesSystem->SetPreferenceInt(slot, NO_SHAKE_PREF_KEY_NAME, iNoShakePreferenceStatus);
}

void CPlayerManager::ResetPlayerFlags(int slot)
{
	SetPlayerStopSound(slot, true);
	SetPlayerSilenceSound(slot, false);
	SetPlayerStopDecals(slot, true);
	SetPlayerNoShake(slot, false);
}

int CPlayerManager::GetOnlinePlayerCount(bool bCountBots)
{
	int iOnlinePlayers = 0;

	if (!GetClientList())
		return iOnlinePlayers;

	for (int i = 0; i < GetClientList()->Count(); i++)
	{
		CServerSideClient* pClient = (*GetClientList())[i];

		if (pClient && pClient->GetSignonState() >= SIGNONSTATE_CONNECTED && (bCountBots || !pClient->IsFakeClient()))
			iOnlinePlayers++;
	}

	return iOnlinePlayers;
}