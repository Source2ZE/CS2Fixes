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

#include "entwatch.h"
#include "commands.h"
#include "cs2fixes.h"
#include "ctimer.h"
#include "customio.h"
#include "detours.h"
#include "engine/igameeventsystem.h"
#include "entity/cbasebutton.h"
#include "entity/cgamerules.h"
#include "entity/cmathcounter.h"
#include "entity/cpointworldtext.h"
#include "entity/cteam.h"
#include "entity/services.h"
#include "eventlistener.h"
#include "gameevents.pb.h"
#include "leader.h"
#include "networksystem/inetworkmessages.h"
#include "playermanager.h"
#include "recipientfilters.h"
#include "serversideclient.h"
#include "tier0/vprof.h"
#include "user_preferences.h"
#include "usermessages.pb.h"
#include "utils/entity.h"
#include "vendor/nlohmann/json.hpp"
#include "zombiereborn.h"
#include <fstream>
#include <sstream>

#include "tier0/memdbgon.h"

extern CGlobalVars* GetGlobals();
extern IGameEventManager2* g_gameEventManager;
extern IGameEventSystem* g_gameEventSystem;
extern INetworkMessages* g_pNetworkMessages;
extern CCSGameRules* g_pGameRules;

CEWHandler* g_pEWHandler = nullptr;

SH_DECL_MANUALHOOK1_void(CBaseButton_Use, 0, 0, 0, InputData_t*);
SH_DECL_MANUALHOOK1_void(CPhysBox_Use, 0, 0, 0, InputData_t*);
SH_DECL_MANUALHOOK1_void(CRotButton_Use, 0, 0, 0, InputData_t*);
SH_DECL_MANUALHOOK1_void(CMomentaryRotButton_Use, 0, 0, 0, InputData_t*);
SH_DECL_MANUALHOOK1_void(CPhysicalButton_Use, 0, 0, 0, InputData_t*);

SH_DECL_MANUALHOOK1_void(CTriggerTeleport_StartTouch, 0, 0, 0, CBaseEntity*);
SH_DECL_MANUALHOOK1_void(CTriggerTeleport_Touch, 0, 0, 0, CBaseEntity*);
SH_DECL_MANUALHOOK1_void(CTriggerTeleport_EndTouch, 0, 0, 0, CBaseEntity*);
SH_DECL_MANUALHOOK1_void(CTriggerOnce_StartTouch, 0, 0, 0, CBaseEntity*);
SH_DECL_MANUALHOOK1_void(CTriggerOnce_Touch, 0, 0, 0, CBaseEntity*);
SH_DECL_MANUALHOOK1_void(CTriggerOnce_EndTouch, 0, 0, 0, CBaseEntity*);
SH_DECL_MANUALHOOK1_void(CTriggerMultiple_StartTouch, 0, 0, 0, CBaseEntity*);
SH_DECL_MANUALHOOK1_void(CTriggerMultiple_Touch, 0, 0, 0, CBaseEntity*);
SH_DECL_MANUALHOOK1_void(CTriggerMultiple_EndTouch, 0, 0, 0, CBaseEntity*);

CConVar<bool> g_cvarEnableEntWatch("entwatch_enable", FCVAR_NONE, "INCOMPATIBLE WITH CS#. Whether to enable EntWatch features", false);
CConVar<bool> g_cvarEnableFiltering("entwatch_auto_filter", FCVAR_NONE, "Whether to automatically block non-item holders from triggering uses", true);
CConVar<bool> g_cvarUseEntwatchClantag("entwatch_clantag", FCVAR_NONE, "Whether to set item holder's clantag and set score", true);

CConVar<int> g_cvarItemHolderScore("entwatch_score", FCVAR_NONE, "Score to give item holders (0 = dont change score at all) Requires entwatch_clantag 1", 9999, true, 0, false, 0);

void EWItemHandler::SetDefaultValues()
{
	type = EWHandlerType::Other;
	mode = EWHandlerMode::EWMode_None;
	szHammerid = "";
	szOutput = "";
	flCooldown = 0.0;
	iMaxUses = 0;
	flOffset = 0.0;
	flMaxOffset = 0.0;
	bShowUse = false;
	bShowHud = false;
	templated = EWCfg_Auto;
}

void EWItemHandler::Print()
{
	Message("     type: %d\n", (int)type);
	Message("     mode: %d\n", (int)mode);
	Message(" hammerid: %s\n", szHammerid.c_str());
	Message("    event: %s\n", szOutput.c_str());
	Message(" cooldown: %.1f\n", flCooldown);
	Message("  maxuses: %d\n", iMaxUses);
	Message("   offset: %.1f\n", flOffset);
	Message("maxoffset: %.1f\n", flMaxOffset);
	Message(" bshowuse: %s\n", bShowUse ? "True" : "False");
	Message(" bshowhud: %s\n", bShowHud ? "True" : "False");
	Message("templated: %d\n", (int)templated);
}

EWItemHandler::EWItemHandler(std::shared_ptr<EWItemHandler> pOther)
{
	type = pOther->type;
	mode = pOther->mode;
	szName = pOther->szName;
	szHammerid = pOther->szHammerid;
	szOutput = pOther->szOutput;
	flCooldown = pOther->flCooldown;
	iMaxUses = pOther->iMaxUses;
	flOffset = pOther->flOffset;
	flMaxOffset = pOther->flMaxOffset;
	bShowUse = pOther->bShowUse;
	bShowHud = pOther->bShowHud;
	templated = pOther->templated;

	pItem = pOther->pItem;
	iEntIndex = -1;
	iCurrentUses = 0;
	flCounterValue = 0;
	flCounterMax = 0;
	flLastUsed = -1.0;
	flLastShownUse = -1.0;
}

EWItemHandler::EWItemHandler(ordered_json jsonKeys)
{
	SetDefaultValues();

	if (jsonKeys.contains("type"))
	{
		std::string value = jsonKeys["type"].get<std::string>();

		if (value == "button")
			type = EWHandlerType::Button;
		else if (value == "counterdown")
			type = EWHandlerType::CounterDown;
		else if (value == "counterup")
			type = EWHandlerType::CounterUp;
	}

	if (jsonKeys.contains("name"))
		szName = jsonKeys["name"].get<std::string>();

	if (jsonKeys.contains("hammerid"))
		szHammerid = jsonKeys["hammerid"].get<std::string>();

	if (jsonKeys.contains("event"))
		szOutput = jsonKeys["event"].get<std::string>();

	if (jsonKeys.contains("mode"))
		mode = (EWHandlerMode)jsonKeys["mode"].get<int>();

	if (jsonKeys.contains("cooldown"))
		flCooldown = jsonKeys["cooldown"].get<float>();

	if (jsonKeys.contains("maxuses"))
		iMaxUses = jsonKeys["maxuses"].get<int>();

	if (jsonKeys.contains("offset"))
	{
		int size = jsonKeys["offset"].size();
		if (size == 1)
		{
			if (jsonKeys["offset"].is_array())
				flOffset = jsonKeys["offset"][0].get<float>();
			else
				flOffset = jsonKeys["offset"].get<float>();

			flMaxOffset = flOffset;
		}
		else if (size >= 2)
		{
			flOffset = jsonKeys["offset"][0].get<float>();
			flMaxOffset = jsonKeys["offset"][1].get<float>();
		}
	}

	if (jsonKeys.contains("message"))
		bShowUse = jsonKeys["message"].get<bool>();

	if (jsonKeys.contains("ui"))
		bShowHud = jsonKeys["ui"].get<bool>();

	if (jsonKeys.contains("templated"))
		templated = jsonKeys["templated"].get<bool>() ? EWCfg_Yes : EWCfg_No;
}

void EWItemHandler::RemoveHook()
{
	CBaseEntity* pEnt = (CBaseEntity*)g_pEntitySystem->GetEntityInstance((CEntityIndex)iEntIndex);
	if (pEnt)
		g_pEWHandler->RemoveHandler(pEnt);
}

// Hook +use if button for owner filtering
// Hook OutValue if counter (and setup countermax)
// Other is auto hooked with FireOutput
void EWItemHandler::RegisterEntity(CBaseEntity* pEntity)
{
	iEntIndex = pEntity->entindex();
	switch (type)
	{
		case Button:
			g_pEWHandler->AddUseHook(pEntity);
			break;
		case CounterDown:
		case CounterUp:
			szOutput = "OutValue";

			CMathCounter* pCounter = (CMathCounter*)pEntity;
			if (!pCounter)
				return;

			float max = (pCounter->m_flMax - pCounter->m_flMin) + flMaxOffset;

			if (mode == CounterValue)
			{
				flCounterMax = max;

				float val = 0.0;
				if (type == CounterDown)
					val = pCounter->GetCounterValue() - pCounter->m_flMin;
				else if (type == CounterUp)
					val = pCounter->m_flMax - pCounter->GetCounterValue();
				flCounterValue = val + flOffset;
			}
			else
			{
				float val = 0.0;
				if (type == CounterDown)
					val = pCounter->m_flMax - pCounter->GetCounterValue();
				else if (type == CounterUp)
					val = pCounter->GetCounterValue() - pCounter->m_flMin;

				val += flOffset;

				iCurrentUses = static_cast<int>(std::round(val));
				iMaxUses = static_cast<int>(std::round(max));
			}
			// Message("Counter init values: %d/%d\n", iCurrentUses, iMaxUses);
			break;
	}
}

void EWItemHandler::Use(float flCounterVal)
{
	if (!GetGlobals())
		return;

	if (!pItem || pItem->iWeaponEnt == -1)
		return;

	// No tracking is necessary if its not being shown anywhere
	if (!bShowHud && !bShowUse)
		return;

	if (type == EWHandlerType::CounterDown || type == EWHandlerType::CounterUp || mode == EWHandlerMode::CounterValue)
	{
		UseCounter(flCounterVal);
		return;
	}

	float flUnlockedAt = (flLastUsed + flCooldown) - 1.0f; // give 1s leeway

	switch (mode)
	{
		case EWMode_None:
			break;
		case Cooldown:
			flLastUsed = GetGlobals()->curtime;
			break;
		case MaxUses:
			if (iCurrentUses < iMaxUses)
			{
				flLastUsed = GetGlobals()->curtime;
				iCurrentUses++;
			}
			break;
		case CooldownAfterUses:
			iCurrentUses++;
			if (iCurrentUses >= iMaxUses)
			{
				flLastUsed = GetGlobals()->curtime;
				iCurrentUses = 0;
			}
			break;
		case CounterValue:
			// Handled in CounterUse()
			return;
	}

	if (pItem->iOwnerSlot == -1)
		return;

	if (!bShowUse)
		return;

	if ((GetGlobals()->curtime - flLastShownUse) < 0.1)
		return;

	CCSPlayerController* pController = CCSPlayerController::FromSlot(pItem->iOwnerSlot);
	if (!pController)
		return;

	if (GetGlobals()->curtime < flUnlockedAt)
		return;

	std::string extra = "";
	if (szName != "")
		extra = " (" + szName + ")";

	ClientPrintAll(HUD_PRINTTALK, EW_PREFIX "\x03%s \x05used %s%s%s", pController->GetPlayerName(), pItem->sChatColor, pItem->szItemName.c_str(), extra.c_str());
	flLastShownUse = GetGlobals()->curtime;
}

void EWItemHandler::UseCounter(float flCounterVal)
{
	// Message("USECOUNTER: CounterVal:%.2f\n", flCounterVal);
	CMathCounter* pCounter = (CMathCounter*)g_pEntitySystem->GetEntityInstance((CEntityIndex)iEntIndex);
	if (!pCounter)
		return;

	float flUnlockedAt = (flLastUsed + flCooldown) - 1.0f; // give 1s leeway
	int newCurrentUses = 0;
	switch (mode)
	{
		case EWMode_None:
			break;
		case Cooldown:
			flLastUsed = GetGlobals()->curtime;
			break;
		case MaxUses:
			iMaxUses = (pCounter->m_flMax - pCounter->m_flMin) + flMaxOffset;

			if (type == EWHandlerType::CounterDown)
				newCurrentUses = pCounter->m_flMax - flCounterVal;
			else if (type == EWHandlerType::CounterUp)
				newCurrentUses = flCounterVal - pCounter->m_flMin;

			newCurrentUses += flOffset;

			if (newCurrentUses <= iCurrentUses) // Our allowed uses increased or didnt change(?), dont show in chat
			{
				iCurrentUses = newCurrentUses;
				return;
			}
			iCurrentUses = newCurrentUses;

			flLastUsed = GetGlobals()->curtime;
			break;
		case CooldownAfterUses:
			iMaxUses = (pCounter->m_flMax - pCounter->m_flMin) + flMaxOffset;

			if (type == EWHandlerType::CounterDown)
				newCurrentUses = pCounter->m_flMax - flCounterVal;
			else if (type == EWHandlerType::CounterUp)
				newCurrentUses = flCounterVal - pCounter->m_flMin;

			newCurrentUses += flOffset;

			if (newCurrentUses <= iCurrentUses) // Our allowed uses increased or didnt change(?), dont show in chat
			{
				iCurrentUses = newCurrentUses;
				return;
			}
			iCurrentUses = newCurrentUses;

			if (iCurrentUses >= iMaxUses)
				flLastUsed = GetGlobals()->curtime;
			break;
		case CounterValue:
			flCounterMax = (pCounter->m_flMax - pCounter->m_flMin) + flMaxOffset;

			if (type == EWHandlerType::CounterDown)
				flCounterValue = flCounterVal - pCounter->m_flMin;
			else if (type == EWHandlerType::CounterUp)
				flCounterValue = pCounter->m_flMax - flCounterVal;

			flCounterValue += flOffset;

			return;
	}

	if (pItem->iOwnerSlot == -1)
		return;

	if (!bShowUse)
		return;

	if ((GetGlobals()->curtime - flLastShownUse) < 0.1)
		return;

	CCSPlayerController* pController = CCSPlayerController::FromSlot(pItem->iOwnerSlot);
	if (!pController)
		return;

	if (GetGlobals()->curtime < flUnlockedAt)
		return;

	std::string extra = "";
	if (szName != "")
		extra = " (" + szName + ")";

	ClientPrintAll(HUD_PRINTTALK, EW_PREFIX "\x03%s \x05used %s%s%s", pController->GetPlayerName(), pItem->sChatColor, pItem->szItemName.c_str(), extra.c_str());
	flLastShownUse = GetGlobals()->curtime;
}

void EWItemHandler::UpdateHudText()
{
	int timeleft;
	if (flLastUsed == -1.0)
		timeleft = 0;
	else
		timeleft = flCooldown - (GetGlobals()->curtime - (flLastUsed + 1));

	switch (mode)
	{
		case EWMode_None:
			szHudText = "+";
			return;
		case Cooldown:
			if (timeleft <= 0)
				szHudText = "R";
			else
				szHudText = std::to_string(timeleft);
			return;
		case MaxUses:
			if (timeleft > 0)
				szHudText = std::to_string(timeleft);
			else if (iCurrentUses >= iMaxUses)
				szHudText = "E";
			else
				szHudText = std::to_string(iCurrentUses) + "/" + std::to_string(iMaxUses);
			break;
		case CooldownAfterUses:
			if (timeleft > 0)
				szHudText = std::to_string(timeleft);
			else
				szHudText = std::to_string(iCurrentUses) + "/" + std::to_string(iMaxUses);
			break;
		case CounterValue:
			if (flCounterValue <= 0.0)
				szHudText = "E";
			else
				szHudText = std::to_string(static_cast<int>(std::round(flCounterValue))) + "/" + std::to_string(static_cast<int>(std::round(flCounterMax)));
	}
}

void EWItem::SetDefaultValues()
{
	szItemName = "";
	szShortName = "";
	/* no default hammerid */
	V_strcpy(sChatColor, "\x01");
	bShowPickup = true;
	bShowHud = true;
	transfer = EWCfg_Auto;
	templated = EWCfg_Auto;
	vecHandlers.clear();
	vecTriggers.clear();
}

void EWItem::ParseColor(std::string value)
{
	if (value == "white" || value == "default")
		V_strcpy(sChatColor, "\x01");
	else if (value == "darkred")
		V_strcpy(sChatColor, "\x02");
	else if (value == "team")
		V_strcpy(sChatColor, "\x03");
	else if (value == "green")
		V_strcpy(sChatColor, "\x04");
	else if (value == "lightgreen")
		V_strcpy(sChatColor, "\x05");
	else if (value == "olive")
		V_strcpy(sChatColor, "\x06");
	else if (value == "red")
		V_strcpy(sChatColor, "\x07");
	else if (value == "gray" || value == "grey")
		V_strcpy(sChatColor, "\x08");
	else if (value == "yellow")
		V_strcpy(sChatColor, "\x09");
	else if (value == "silver")
		V_strcpy(sChatColor, "\x0A");
	else if (value == "blue")
		V_strcpy(sChatColor, "\x0B");
	else if (value == "darkblue")
		V_strcpy(sChatColor, "\x0C");
	// \x0D is the same as \x0A
	else if (value == "purple" || value == "pink")
		V_strcpy(sChatColor, "\x0E");
	else if (value == "red2")
		V_strcpy(sChatColor, "\x0F");
	else if (value == "orange" || value == "gold")
		V_strcpy(sChatColor, "\x10");
}

EWItem::EWItem(std::shared_ptr<EWItem> pItem)
{
	id = pItem->id;
	szItemName = pItem->szItemName;
	szShortName = pItem->szShortName;
	szHammerid = pItem->szHammerid;
	V_strcpy(sChatColor, pItem->sChatColor);
	bShowPickup = pItem->bShowPickup;
	bShowHud = pItem->bShowHud;
	transfer = pItem->transfer;
	templated = pItem->templated;

	vecHandlers.clear();
	for (int i = 0; i < (pItem->vecHandlers).size(); i++)
	{
		std::shared_ptr<EWItemHandler> pHandler = std::make_shared<EWItemHandler>(pItem->vecHandlers[i]);
		vecHandlers.push_back(pHandler);
	}

	vecTriggers.clear();
	for (int i = 0; i < pItem->vecTriggers.size(); i++)
		vecTriggers.emplace_back(pItem->vecTriggers[i]);
}

EWItem::EWItem(ordered_json jsonKeys, int _id)
{
	id = _id;
	SetDefaultValues();

	if (jsonKeys.contains("name"))
		szItemName = jsonKeys["name"].get<std::string>();

	if (jsonKeys.contains("shortname"))
		szShortName = jsonKeys["shortname"].get<std::string>();
	else
		szShortName = szItemName; // Set shortname to long name if it isnt set

	// We know it contains hammerid
	szHammerid = jsonKeys["hammerid"].get<std::string>();

	if (jsonKeys.contains("color"))
		ParseColor(jsonKeys["color"].get<std::string>());
	else if (jsonKeys.contains("colour")) // gotta represent
		ParseColor(jsonKeys["colour"].get<std::string>());

	if (jsonKeys.contains("message"))
		bShowPickup = jsonKeys["message"].get<bool>();

	if (jsonKeys.contains("ui"))
		bShowHud = jsonKeys["ui"].get<bool>();

	if (jsonKeys.contains("transfer"))
		transfer = jsonKeys["transfer"].get<bool>() ? EWCfg_Yes : EWCfg_No;

	if (jsonKeys.contains("templated"))
		templated = jsonKeys["templated"].get<bool>() ? EWCfg_Yes : EWCfg_No;

	if (jsonKeys.contains("triggers"))
	{
		if (jsonKeys["triggers"].size() > 0)
		{
			for (std::string bl : jsonKeys["triggers"])
			{
				if (bl == "")
					continue;

				vecTriggers.push_back(bl);
			}
		}
	}

	if (jsonKeys.contains("handlers"))
	{
		if (jsonKeys["handlers"].size() > 0)
		{
			for (auto& [key, handlerEntry] : jsonKeys["handlers"].items())
			{
				std::shared_ptr<EWItemHandler> handler = std::make_shared<EWItemHandler>(handlerEntry);
				vecHandlers.push_back(handler);
			}
		}
	}
}

// true: found at least one matching handler for this ent, false: didnt
bool EWItemInstance::RegisterHandler(CBaseEntity* pEnt, int iHandlerTemplateNum)
{
	bool found = false;
	for (int i = 0; i < (vecHandlers).size(); i++)
	{
		std::shared_ptr<EWItemHandler> handler = vecHandlers[i];
		if (handler->iEntIndex != -1)
			continue; // this handler is already setup

		// check handler id
		std::string hammerid = pEnt->m_sUniqueHammerID.Get().String();
		if (handler->szHammerid != hammerid)
			continue;

		// check template numbers

		// if handler is specifically not templated then register
		if (handler->templated != EWCfg_No)
		{
			// if weapon is not templated then we cant compare template numbers
			// so just register
			if (templated == EWCfg_Yes)
			{
				if (iTemplateNum != iHandlerTemplateNum)
				{
					// Message("template numbers do not match [item:%d   handler:%d]\n", iTemplateNum, iHandlerTemplateNum);
					continue;
				}
			}
		}

		handler->RegisterEntity(pEnt);
		handler->pItem = this;
		found = true;
		// Might be more than one handler per entity so dont return yet
	}
	return found;
}

bool EWItemInstance::RemoveHandler(CBaseEntity* pEnt)
{
	for (int i = 0; i < (vecHandlers).size(); i++)
	{
		if (vecHandlers[i]->iEntIndex == pEnt->entindex())
		{
			if (vecHandlers[i]->type == EWHandlerType::Button)
				g_pEWHandler->RemoveUseHook(pEnt);
			vecHandlers[i]->iEntIndex = -1;
			return true;
		}
	}
	return false;
}

int EWItemInstance::FindHandlerByEntIndex(int indexToFind)
{
	if (vecHandlers.size() <= 0)
		return -1;

	for (int i = 0; i < (vecHandlers).size(); i++)
		if (vecHandlers[i]->iEntIndex == indexToFind)
			return i;
	return -1;
}

void EWItemInstance::FindExistingHandlers()
{
	for (int i = 0; i < (vecHandlers).size(); i++)
	{
		std::shared_ptr<EWItemHandler> handler = vecHandlers[i];

		// ONLY specified NON-TEMPLATED handlers should do this
		if (handler->iEntIndex != -1 || handler->templated == EWCfg_Yes)
			continue;

		CBaseEntity* pTarget = nullptr;
		while ((pTarget = UTIL_FindEntityByName(pTarget, "*")))
		{
			if (!V_strcmp(pTarget->m_sUniqueHammerID().Get(), handler->szHammerid.c_str()))
			{
				if (handler->templated == EWCfg_Auto)
				{
					// Check if template suffix actually matches or not templated
					int suffix = GetTemplateSuffixNumber(pTarget->GetName());
					if (suffix != -1 && suffix != iTemplateNum)
						continue;
				}

				handler->RegisterEntity(pTarget);
				handler->pItem = this;
				Message("[Entwatch] LATE REGISTERED HANDLER. Item:%s  Handler:%d  entindex:%d\n", szItemName.c_str(), i, pTarget->entindex());
				break;
			}
		}
	}
}

/* Called when a player picks up this item */
void EWItemInstance::Pickup(int slot)
{
	iOwnerSlot = slot;

	ZEPlayer* pPlayer = g_playerManager->GetPlayer(CPlayerSlot(iOwnerSlot));
	CCSPlayerController* pController = CCSPlayerController::FromSlot(iOwnerSlot);
	if (!pPlayer || !pController)
	{
		iOwnerSlot = -1;
		return;
	}

	sLastOwnerName = pController->GetPlayerName();

	if (iTeamNum == CS_TEAM_NONE)
		iTeamNum = pController->m_iTeamNum();

	if (pPlayer->IsFakeClient())
		Message(EW_PREFIX "%s [BOT] has picked up %s (weaponid:%d)\n", pController->GetPlayerName(), szItemName.c_str(), iWeaponEnt);
	else
		Message(EW_PREFIX "%s [%llu] has picked up %s (weaponid:%d)\n", pController->GetPlayerName(), pPlayer->GetUnauthenticatedSteamId64(), szItemName.c_str(), iWeaponEnt);

	// Set clantag
	if (g_cvarUseEntwatchClantag.Get() && bShowHud)
	{
		// Only set clantag if owner doesnt already have one set
		bool bShouldSetClantag = true;
		int otherItem = g_pEWHandler->FindItemInstanceByOwner(iOwnerSlot, false, 0);
		while (otherItem != -1)
		{
			if (g_pEWHandler->vecItems[otherItem]->bHasThisClantag)
			{
				bShouldSetClantag = false;
				break;
			}
			otherItem = g_pEWHandler->FindItemInstanceByOwner(iOwnerSlot, false, otherItem + 1);
		}

		if (bShouldSetClantag)
		{
			bHasThisClantag = true;
			pController->m_szClan(sClantag);
			if (g_cvarItemHolderScore.Get() > -1)
			{
				int score = pController->m_iScore + g_cvarItemHolderScore.Get();
				pController->m_iScore = score;
			}

			EW_SendBeginNewMatchEvent();
		}
	}

	if (bShowPickup)
		ClientPrintAll(HUD_PRINTTALK, EW_PREFIX "\x03%s \x05has picked up %s%s", pController->GetPlayerName(), sChatColor, szItemName.c_str());
}

void EWItemInstance::Drop(EWDropReason reason, CCSPlayerController* pController)
{
	ZEPlayer* pPlayer = g_playerManager->GetPlayer(CPlayerSlot(iOwnerSlot));
	if (!pPlayer)
	{
		iOwnerSlot = -1;
		return;
	}

	if (g_cvarUseEntwatchClantag.Get() && bShowHud && bHasThisClantag)
	{
		bool bSetAnotherClantag = false;

		// Check if this player is holding another item that should be shown
		int otherItem = g_pEWHandler->FindItemInstanceByOwner(iOwnerSlot, false, 0);
		while (otherItem != -1)
		{
			if (g_pEWHandler->vecItems[otherItem]->bShowHud && !g_pEWHandler->vecItems[otherItem]->bHasThisClantag)
			{
				// Player IS holding another item, score doesnt need adjusting

				pController->m_szClan(g_pEWHandler->vecItems[otherItem]->sClantag);
				g_pEWHandler->vecItems[otherItem]->bHasThisClantag = true;
				bSetAnotherClantag = true;
				break;
			}
			otherItem = g_pEWHandler->FindItemInstanceByOwner(iOwnerSlot, false, otherItem + 1);
		}
		bHasThisClantag = false;

		if (!bSetAnotherClantag)
		{
			if (g_cvarItemHolderScore.Get() != 0)
			{
				int score = pController->m_iScore - g_cvarItemHolderScore.Get();
				pController->m_iScore = score;
			}

			pController->m_szClan("");
		}

		EW_SendBeginNewMatchEvent();
	}

	char sPlayerInfo[64];
	if (pPlayer->IsFakeClient())
		V_snprintf(sPlayerInfo, sizeof(sPlayerInfo), "%s [BOT]", pController->GetPlayerName());
	else
		V_snprintf(sPlayerInfo, sizeof(sPlayerInfo), "%s [%llu]", pController->GetPlayerName(), pPlayer->GetUnauthenticatedSteamId64());

	switch (reason)
	{
		case EWDropReason::Drop:

			Message(EW_PREFIX "%s has dropped %s (weaponid:%d)\n", sPlayerInfo, szItemName.c_str(), iWeaponEnt);
			if (bShowPickup)
				ClientPrintAll(HUD_PRINTTALK, EW_PREFIX "\x03%s \x05has dropped %s%s", pController->GetPlayerName(), sChatColor, szItemName.c_str());
			break;
		case EWDropReason::Infected:
			if (bAllowDrop)
			{
				Message(EW_PREFIX "%s got infected and dropped %s (weaponid:%d)\n", sPlayerInfo, szItemName.c_str(), iWeaponEnt);
				if (bShowPickup)
					ClientPrintAll(HUD_PRINTTALK, EW_PREFIX "\x03%s\x05 got infected and dropped %s%s", pController->GetPlayerName(), sChatColor, szItemName.c_str());
			}
			else
			{
				Message(EW_PREFIX "%s got infected with %s (weaponid:%d)\n", sPlayerInfo, szItemName.c_str(), iWeaponEnt);
				if (bShowPickup)
					ClientPrintAll(HUD_PRINTTALK, EW_PREFIX "\x03%s\x05 got infected with %s%s", pController->GetPlayerName(), sChatColor, szItemName.c_str());
			}
			break;
		case EWDropReason::Death:
			if (bAllowDrop)
			{
				Message(EW_PREFIX "%s has died and dropped %s (weaponid:%d)\n", sPlayerInfo, szItemName.c_str(), iWeaponEnt);
				if (bShowPickup)
					ClientPrintAll(HUD_PRINTTALK, EW_PREFIX "\x03%s\x05 died and dropped %s%s", pController->GetPlayerName(), sChatColor, szItemName.c_str());
			}
			else
			{
				Message(EW_PREFIX "%s has died with %s (weaponid:%d)\n", sPlayerInfo, szItemName.c_str(), iWeaponEnt);
				if (bShowPickup)
					ClientPrintAll(HUD_PRINTTALK, EW_PREFIX "\x03%s\x05 died with %s%s", pController->GetPlayerName(), sChatColor, szItemName.c_str());
			}
			break;
		case EWDropReason::Disconnect:
			if (bAllowDrop)
			{
				Message(EW_PREFIX "%s has disconnected and dropped %s (weaponid:%d)\n", sPlayerInfo, szItemName.c_str(), iWeaponEnt);
				if (bShowPickup)
					ClientPrintAll(HUD_PRINTTALK, EW_PREFIX "\x03%s\x05 disconnected and dropped %s%s", pController->GetPlayerName(), sChatColor, szItemName.c_str());
			}
			else
			{
				Message(EW_PREFIX "%s has disconnected with %s (weaponid:%d)\n", sPlayerInfo, szItemName.c_str(), iWeaponEnt);
				if (bShowPickup)
					ClientPrintAll(HUD_PRINTTALK, EW_PREFIX "\x03%s\x05 disconnected with %s%s", pController->GetPlayerName(), sChatColor, szItemName.c_str());
			}
			break;
		case EWDropReason::Deleted:
		default:
			break;
	}

	iOwnerSlot = -1;
}

std::string EWItemInstance::GetHandlerStateText()
{
	std::string sText = "+";
	bool first = true;
	for (int i = 0; i < (vecHandlers).size(); i++)
	{
		if (!vecHandlers[i]->bShowHud || (int)(vecHandlers[i]->mode) >= EWMode_Last)
			continue;

		vecHandlers[i]->UpdateHudText();
		if (first)
		{
			sText = vecHandlers[i]->szHudText;
			first = false;
		}
		else
		{
			sText.append("|");
			sText.append(vecHandlers[i]->szHudText);
		}
	}
	// Message("%s Item handler text: %s\n", szItemName.c_str(), sText.c_str());
	return sText;
}

void CEWHandler::UnLoadConfig()
{
	if (!bConfigLoaded)
		return;

	if (EW_IsFireOutputHooked())
		mapIOFunctions.erase("entwatch");

	// Clantags first so scores can be set back properly
	ResetAllClantags();

	mapItemConfig.clear();

	for (int i = 0; i < (vecItems).size(); i++)
		for (int j = 0; j < (vecItems[i]->vecHandlers).size(); j++)
			vecItems[i]->vecHandlers[j]->RemoveHook();
	vecItems.clear();

	RemoveAllUseHooks();
	RemoveAllTriggers();

	bConfigLoaded = false;
}

/*
 *  Load a config file by mapname
 */
void CEWHandler::LoadMapConfig(const char* sMapName)
{
	const char* pszMapConfigPath = "addons/cs2fixes/configs/entwatch/maps/";

	char szPath[MAX_PATH];
	V_snprintf(szPath, sizeof(szPath), "%s%s.jsonc", pszMapConfigPath, sMapName);

	LoadConfig(szPath);
}

/*
 * Load a config file from /csgo/ folder
 */
void CEWHandler::LoadConfig(const char* sFilePath)
{
	UnLoadConfig();

	char szPath[MAX_PATH];
	V_snprintf(szPath, sizeof(szPath), "%s%s%s", Plat_GetGameDirectory(), "/csgo/", sFilePath);

	std::ifstream jsoncFile(szPath);

	if (!jsoncFile.is_open())
	{
		Message("[EntWatch] Failed to open %s\n", sFilePath);
		return;
	}

	ordered_json jsonItems = ordered_json::parse(jsoncFile, nullptr, false, true);
	if (jsonItems.is_discarded())
	{
		Panic("[EntWatch] Error parsing json! %s\n", sFilePath);
		return;
	}

	for (auto& [szItemName, jsonItemData] : jsonItems.items())
	{
		if (!jsonItemData.contains("hammerid"))
		{
			Panic("[EntWatch] Item without a hammerid\n");
			continue;
		}

		std::string sHammerid = jsonItemData["hammerid"].get<std::string>();
		std::shared_ptr<EWItem> item = std::make_shared<EWItem>(jsonItemData, mapItemConfig.size());

		mapItemConfig[hash_32_fnv1a_const(sHammerid.c_str())] = item;
	}

	if (mapItemConfig.size() > 0)
	{
		// Hook FireOutput
		if (!SetupFireOutputInternalDetour())
			mapIOFunctions.erase("entwatch");
		else if (!EW_IsFireOutputHooked())
			mapIOFunctions["entwatch"] = EW_FireOutput;
	}

	bConfigLoaded = true;
}

void CEWHandler::PrintLoadedConfig(CPlayerSlot slot)
{
	CCSPlayerController* player = CCSPlayerController::FromSlot(slot);
	if (!bConfigLoaded)
	{
		ClientPrint(player, HUD_PRINTTALK, EW_PREFIX "No config loaded.");
		return;
	}

	int i = -1;
	for (auto const& [key, item] : mapItemConfig)
	{
		i++;
		if (!item)
		{
			ClientPrint(player, HUD_PRINTCONSOLE, EW_PREFIX "Null item in the item map at pos %d", i);
			continue;
		}

		ClientPrint(player, HUD_PRINTCONSOLE, EW_PREFIX "------------ Item %02d ------------", i);
		ClientPrint(player, HUD_PRINTCONSOLE, EW_PREFIX "     Name:  %s", item->szItemName.c_str());
		ClientPrint(player, HUD_PRINTCONSOLE, EW_PREFIX "ShortName:  %s", item->szShortName.c_str());
		ClientPrint(player, HUD_PRINTCONSOLE, EW_PREFIX " Hammerid:  %s", item->szHammerid.c_str());
		ClientPrint(player, HUD_PRINTCONSOLE, EW_PREFIX "  Message:  %s", item->bShowPickup ? "True" : "False");
		ClientPrint(player, HUD_PRINTCONSOLE, EW_PREFIX "       UI:  %s", item->bShowHud ? "True" : "False");
		if (item->transfer == EWCfg_Auto)
			ClientPrint(player, HUD_PRINTCONSOLE, EW_PREFIX " Transfer:  Auto");
		else if (item->transfer == EWCfg_Yes)
			ClientPrint(player, HUD_PRINTCONSOLE, EW_PREFIX " Transfer:  True");
		else
			ClientPrint(player, HUD_PRINTCONSOLE, EW_PREFIX " Transfer:  False");

		ClientPrint(player, HUD_PRINTCONSOLE, EW_PREFIX " ");
		if (item->vecHandlers.size() == 0)
		{
			ClientPrint(player, HUD_PRINTCONSOLE, EW_PREFIX "        No handlers set.");
		}
		else
		{
			for (int j = 0; j < (item->vecHandlers).size(); j++)
			{
				// "          "
				ClientPrint(player, HUD_PRINTCONSOLE, EW_PREFIX "          --------- Handler %d ---------", j);
				ClientPrint(player, HUD_PRINTCONSOLE, EW_PREFIX "              Type:  %s", item->vecHandlers[j]->type == EWHandlerType::Button ? "Button" : "GameUi");
				ClientPrint(player, HUD_PRINTCONSOLE, EW_PREFIX "              Mode:  %d", (int)item->vecHandlers[j]->mode);
				ClientPrint(player, HUD_PRINTCONSOLE, EW_PREFIX "          Hammerid:  %s", item->vecHandlers[j]->szHammerid.c_str());
				ClientPrint(player, HUD_PRINTCONSOLE, EW_PREFIX "             Event:  %s", item->vecHandlers[j]->szOutput.c_str());
				ClientPrint(player, HUD_PRINTCONSOLE, EW_PREFIX "          Cooldown:  %.1f", item->vecHandlers[j]->flCooldown);
				ClientPrint(player, HUD_PRINTCONSOLE, EW_PREFIX "          Max Uses:  %d", item->vecHandlers[j]->iMaxUses);
				ClientPrint(player, HUD_PRINTCONSOLE, EW_PREFIX "           Message:  %s", item->vecHandlers[j]->bShowUse ? "True" : "False");
				ClientPrint(player, HUD_PRINTCONSOLE, EW_PREFIX "          --------- --------- ---------");
			}
		}

		ClientPrint(player, HUD_PRINTCONSOLE, EW_PREFIX " ");
		if (item->vecTriggers.size() == 0)
			ClientPrint(player, HUD_PRINTCONSOLE, EW_PREFIX "       No triggers set.");
		else
			for (int j = 0; j < item->vecTriggers.size(); j++)
				ClientPrint(player, HUD_PRINTCONSOLE, EW_PREFIX " Trigger %d:  %s", j, item->vecTriggers[j].c_str());

		ClientPrint(player, HUD_PRINTCONSOLE, EW_PREFIX "------------ ------- ------------");
	}
	ClientPrint(player, HUD_PRINTTALK, EW_PREFIX "See console for output.");
}

void CEWHandler::ClearItems()
{
	mapTransfers.clear();
	vecItems.clear();
}

/*
 *	Finds the index of an item instance with a given entity index
 *  Returns index into vecItems or -1 if not found
 */
int CEWHandler::FindItemInstanceByWeapon(int iWeaponEnt)
{
	for (int i = 0; i < (vecItems).size(); i++)
	{
		if (vecItems[i]->iWeaponEnt == -1)
			continue;

		if (vecItems[i]->iWeaponEnt == iWeaponEnt)
			return i;
	}
	return -1;
}

int CEWHandler::FindItemInstanceByOwner(int iOwnerSlot, bool bOnlyTransferrable, int iStartItem)
{
	for (int i = iStartItem; i < vecItems.size(); i++)
	{
		if (bOnlyTransferrable && vecItems[i]->transfer == EWCfg_No)
			continue;

		if (vecItems[i]->iWeaponEnt == -1)
			continue;

		if (vecItems[i]->iOwnerSlot == iOwnerSlot)
			return i;
	}
	return -1;
}

int CEWHandler::FindItemInstanceByName(std::string sItemName, bool bOnlyTransferrable, bool bExact, int iStartItem)
{
	std::string lowercaseInput = "";
	for (char ch : sItemName)
		lowercaseInput += std::tolower(ch);

	for (int i = iStartItem; i < (vecItems).size(); i++)
	{
		if (bOnlyTransferrable && vecItems[i]->transfer == EWCfg_No)
			continue;

		if (vecItems[i]->iWeaponEnt == -1)
			continue;

		std::string itemname = "";

		// Short name first
		for (char ch : vecItems[i]->szShortName)
			itemname += std::tolower(ch);

		if (bExact)
		{
			if (itemname == lowercaseInput)
				return i;
		}
		else
		{
			if (itemname.find(lowercaseInput) != std::string::npos)
				return i;
		}

		// long name
		itemname = "";
		for (char ch : vecItems[i]->szItemName)
			itemname += std::tolower(ch);

		if (bExact)
		{
			if (itemname == lowercaseInput)
				return i;
		}
		else
		{
			if (itemname.find(lowercaseInput) != std::string::npos)
				return i;
		}
	}
	return -1;
}

void CEWHandler::RegisterHandler(CBaseEntity* pEnt)
{
	int templatenum = GetTemplateSuffixNumber(pEnt->GetName());
	for (int i = 0; i < (vecItems).size(); i++)
	{
		if (vecItems[i]->RegisterHandler(pEnt, templatenum))
		{
			Message("REGISTERED HANDLER. Item:%s Instance:%d  entindex:%d\n", vecItems[i]->szItemName.c_str(), i + 1, pEnt->entindex());
			return;
		}
	}
}

bool CEWHandler::RegisterTrigger(CBaseEntity* pEnt)
{
	for (auto const& [key, pItem] : mapItemConfig)
	{
		if (pItem->vecTriggers.size() < 1)
			continue;

		const char* sHammerid = pEnt->m_sUniqueHammerID().Get();
		for (int j = 0; j < pItem->vecTriggers.size(); j++)
		{
			if (strcmp(sHammerid, pItem->vecTriggers[j].c_str()))
				continue;

			AddTouchHook(pEnt);

			return true;
		}
	}
	return false;
}

void CEWHandler::AddTouchHook(CBaseEntity* pEnt)
{
	static int startOffset = g_GameConfig->GetOffset("CBaseEntity::StartTouch");
	static int touchOffset = g_GameConfig->GetOffset("CBaseEntity::Touch");
	static int endOffset = g_GameConfig->GetOffset("CBaseEntity::EndTouch");

	if (!V_strcmp(pEnt->GetClassname(), "trigger_teleport"))
	{
		if (iTriggerTeleportTouchHooks[0] == -1)
		{
			const auto pTeleport_VTable = modules::server->FindVirtualTable("CTriggerTeleport");

			SH_MANUALHOOK_RECONFIGURE(CTriggerTeleport_StartTouch, startOffset, 0, 0);
			iTriggerTeleportTouchHooks[0] = SH_ADD_MANUALDVPHOOK(CTriggerTeleport_StartTouch, pTeleport_VTable, SH_MEMBER(this, &CEWHandler::Hook_Touch), false);

			SH_MANUALHOOK_RECONFIGURE(CTriggerTeleport_Touch, touchOffset, 0, 0);
			iTriggerTeleportTouchHooks[1] = SH_ADD_MANUALDVPHOOK(CTriggerTeleport_Touch, pTeleport_VTable, SH_MEMBER(this, &CEWHandler::Hook_Touch), false);

			SH_MANUALHOOK_RECONFIGURE(CTriggerTeleport_EndTouch, endOffset, 0, 0);
			iTriggerTeleportTouchHooks[2] = SH_ADD_MANUALDVPHOOK(CTriggerTeleport_EndTouch, pTeleport_VTable, SH_MEMBER(this, &CEWHandler::Hook_Touch), false);
		}
	}
	else if (!V_strcmp(pEnt->GetClassname(), "trigger_multiple"))
	{
		if (iTriggerMultipleTouchHooks[0] == -1)
		{
			const auto pMultiple_VTable = modules::server->FindVirtualTable("CTriggerMultiple");

			SH_MANUALHOOK_RECONFIGURE(CTriggerMultiple_StartTouch, startOffset, 0, 0);
			iTriggerMultipleTouchHooks[0] = SH_ADD_MANUALDVPHOOK(CTriggerMultiple_StartTouch, pMultiple_VTable, SH_MEMBER(this, &CEWHandler::Hook_Touch), false);

			SH_MANUALHOOK_RECONFIGURE(CTriggerMultiple_Touch, touchOffset, 0, 0);
			iTriggerMultipleTouchHooks[1] = SH_ADD_MANUALDVPHOOK(CTriggerMultiple_Touch, pMultiple_VTable, SH_MEMBER(this, &CEWHandler::Hook_Touch), false);

			SH_MANUALHOOK_RECONFIGURE(CTriggerMultiple_EndTouch, endOffset, 0, 0);
			iTriggerMultipleTouchHooks[2] = SH_ADD_MANUALDVPHOOK(CTriggerMultiple_EndTouch, pMultiple_VTable, SH_MEMBER(this, &CEWHandler::Hook_Touch), false);
		}
	}
	else if (!V_strcmp(pEnt->GetClassname(), "trigger_once"))
	{
		if (iTriggerOnceTouchHooks[0] == -1)
		{
			const auto pOnce_VTable = modules::server->FindVirtualTable("CTriggerOnce");

			SH_MANUALHOOK_RECONFIGURE(CTriggerOnce_StartTouch, startOffset, 0, 0);
			iTriggerOnceTouchHooks[0] = SH_ADD_MANUALDVPHOOK(CTriggerOnce_StartTouch, pOnce_VTable, SH_MEMBER(this, &CEWHandler::Hook_Touch), false);

			SH_MANUALHOOK_RECONFIGURE(CTriggerOnce_Touch, touchOffset, 0, 0);
			iTriggerOnceTouchHooks[1] = SH_ADD_MANUALDVPHOOK(CTriggerOnce_Touch, pOnce_VTable, SH_MEMBER(this, &CEWHandler::Hook_Touch), false);

			SH_MANUALHOOK_RECONFIGURE(CTriggerOnce_EndTouch, endOffset, 0, 0);
			iTriggerOnceTouchHooks[2] = SH_ADD_MANUALDVPHOOK(CTriggerOnce_EndTouch, pOnce_VTable, SH_MEMBER(this, &CEWHandler::Hook_Touch), false);
		}
	}

	vecHookedTriggers.push_back(pEnt->GetHandle());
}

void CEWHandler::Hook_Touch(CBaseEntity* pOther)
{
	CBaseEntity* pEntity = META_IFACEPTR(CBaseEntity);
	if (!pEntity)
		RETURN_META(MRES_IGNORED);

	bool bFound = false;
	for (int i = 0; i < (vecHookedTriggers).size(); i++)
	{
		if (vecHookedTriggers[i] == pEntity->GetHandle())
		{
			bFound = true;
			break;
		}
	}

	if (!bFound)
		RETURN_META(MRES_IGNORED);

	CCSPlayerPawn* pPawn = (CCSPlayerPawn*)pOther;
	if (!pPawn)
		RETURN_META(MRES_IGNORED);

	CCSPlayerController* pController = pPawn->GetOriginalController();
	if (!pController)
		RETURN_META(MRES_IGNORED);

	ZEPlayer* zpPlayer = pController->GetZEPlayer();
	if (!zpPlayer)
		RETURN_META(MRES_IGNORED);

	if (zpPlayer->IsEbanned())
		RETURN_META(MRES_SUPERCEDE);

	RETURN_META(MRES_IGNORED);
}

bool CEWHandler::RemoveTrigger(CBaseEntity* pEnt)
{
	for (int i = 0; i < (vecHookedTriggers).size(); i++)
	{
		if (vecHookedTriggers[i] == pEnt->GetHandle())
		{
			vecHookedTriggers.erase(vecHookedTriggers.begin() + i);

			return true;
		}
	}
	return false;
}

void CEWHandler::RemoveAllTriggers()
{
	Message("[EntWatch] Fully unhooking touch hooks\n");
	for (int i = 0; i < 3; i++)
	{
		SH_REMOVE_HOOK_ID(iTriggerTeleportTouchHooks[i]);
		iTriggerTeleportTouchHooks[i] = -1;

		SH_REMOVE_HOOK_ID(iTriggerMultipleTouchHooks[i]);
		iTriggerMultipleTouchHooks[i] = -1;

		SH_REMOVE_HOOK_ID(iTriggerOnceTouchHooks[i]);
		iTriggerOnceTouchHooks[i] = -1;
	}
	vecHookedTriggers.clear();
}

void CEWHandler::RemoveHandler(CBaseEntity* pEnt)
{
	for (int i = 0; i < (vecItems).size(); i++)
		if (vecItems[i]->RemoveHandler(pEnt))
			return;
}

void CEWHandler::ResetAllClantags()
{
	// Reset item holders scores to what they should be
	if (g_cvarItemHolderScore.Get() != 0)
	{
		for (int i = 0; i < (vecItems).size(); i++)
		{
			if (vecItems[i]->bHasThisClantag)
			{
				CCSPlayerController* pOwner = CCSPlayerController::FromSlot(CPlayerSlot(vecItems[i]->iOwnerSlot));
				if (!pOwner)
					continue;

				int score = pOwner->m_iScore - g_cvarItemHolderScore.Get();
				pOwner->m_iScore = score;
				vecItems[i]->bHasThisClantag = false;
			}
		}
	}

	if (!GetGlobals())
		return;

	// Reset everyone's scores and tags for insurance
	for (int i = 0; i < GetGlobals()->maxClients; i++)
	{
		CCSPlayerController* pController = CCSPlayerController::FromSlot(i);
		if (!pController)
			continue;

		// Bring score down below entwatch_score so new item holders show above
		if (pController->m_iScore >= g_cvarItemHolderScore.Get())
		{
			int score = pController->m_iScore % g_cvarItemHolderScore.Get();
			pController->m_iScore = score;
		}

		pController->m_szClan("");
	}

	EW_SendBeginNewMatchEvent();
}

int CEWHandler::RegisterItem(CBasePlayerWeapon* pWeapon)
{
	std::string sHammerid = pWeapon->m_sUniqueHammerID.Get().String();

	auto i = mapItemConfig.find(hash_32_fnv1a_const(sHammerid.c_str()));

	if (i == mapItemConfig.end())
		return -1;

	std::shared_ptr<EWItem> item = i->second;

	Message("Registering item %s \n", item->szItemName.c_str(), vecItems.size() + 1);

	std::shared_ptr<EWItemInstance> instance = std::make_shared<EWItemInstance>(pWeapon->entindex(), item);

	V_snprintf(instance->sClantag, sizeof(EWItemInstance::sClantag), "[+]%s:", instance->szShortName.c_str());

	bool bKnife = pWeapon->GetWeaponVData()->m_GearSlot() == GEAR_SLOT_KNIFE;
	instance->bAllowDrop = !bKnife;

	// Auto detect transfer, allow transfer if not knife
	if (instance->transfer == EWCfg_Auto)
		instance->transfer = bKnife ? EWCfg_No : EWCfg_Yes;

	// Check if we are templated
	if (instance->templated != EWCfg_No)
	{
		int templatenum = GetTemplateSuffixNumber(pWeapon->GetName());
		if (templatenum == -1)
		{
			instance->templated = EWCfg_No;
		}
		else
		{
			instance->templated = EWCfg_Yes;
			instance->iTemplateNum = templatenum;
		}
	}

	// Place items in order of the config
	int place = -1;
	for (int i = 0; i < (vecItems).size(); i++)
	{
		if (vecItems[i]->id >= instance->id)
		{
			place = i;
			break;
		}
	}

	if (place == -1) // reached the end, our id still higher
	{
		vecItems.push_back(instance);

		return (vecItems.size() - 1);
	}

	vecItems.insert(vecItems.begin() + place, instance);

	// Also have to update any ongoing etransfers with items that fall after this new one
	for (auto const& [key, transferInfo] : mapTransfers)
	{
		for (int i = 0; i < transferInfo->itemIds.size(); i++)
		{
			if (transferInfo->itemIds[i] < place)
				continue;

			transferInfo->itemIds[i]++;
		}
	}

	return place;
}

// Weapon entity of specified item has been deleted
void CEWHandler::RemoveWeaponFromItem(int itemId)
{
	std::shared_ptr<EWItemInstance> pItem = vecItems[itemId];
	if (!pItem)
	{
		vecItems.erase(vecItems.begin() + itemId);
		return;
	}

	// Use drop to handle clantag/score stuff
	if (pItem->iOwnerSlot != -1)
	{
		CCSPlayerController* pOwner = CCSPlayerController::FromSlot(CPlayerSlot(pItem->iOwnerSlot));
		if (pOwner)
			pItem->Drop(EWDropReason::Deleted, pOwner);
	}

	pItem->iWeaponEnt = -1;
}

/* Player picked up a weapon in the config */
void CEWHandler::PlayerPickup(CCSPlayerPawn* pPawn, int iItemInstance)
{
	if (iItemInstance < 0 || iItemInstance >= vecItems.size())
		return;

	std::shared_ptr<EWItemInstance> item = vecItems[iItemInstance];

	if (item->iWeaponEnt == -1)
		return;

	item->Pickup(pPawn->m_hOriginalController->GetPlayerSlot());

	if (!m_bHudTicking)
	{
		m_bHudTicking = true;
		new CTimer(EW_HUD_TICKRATE, false, false, [] {
			return EW_UpdateHud();
		});
	}
}

void CEWHandler::PlayerDrop(EWDropReason reason, int iItemInstance, CCSPlayerController* pController)
{
	if (!pController)
		return;

	// Drop is only called when ONE item is being dropped by choice
	// Death and disconnect drop all items currently owned
	if (reason == EWDropReason::Drop)
	{
		if (iItemInstance == -1 || iItemInstance >= vecItems.size())
			return;

		std::shared_ptr<EWItemInstance> pItem = vecItems[iItemInstance];
		if (!pItem)
			return;

		if (pItem->iOwnerSlot != pController->GetPlayerSlot())
			return;

		pItem->Drop(reason, pController);
	}
	else
	{
		// Find all items owned by this player
		for (int i = 0; i < (vecItems).size(); i++)
		{
			if (vecItems[i]->iOwnerSlot != pController->GetPlayerSlot())
				continue;

			vecItems[i]->Drop(reason, pController);
		}
	}
}

void CEWHandler::Transfer(CCSPlayerController* pCaller, int iItemInstance, CHandle<CCSPlayerController> hReceiver)
{
	CCSPlayerController* pReceiver = hReceiver.Get();
	if (!pReceiver)
	{
		ClientPrint(pCaller, HUD_PRINTTALK, EW_PREFIX "Receiver is no longer valid.");
		return;
	}
	CCSPlayerPawn* pReceiverPawn = pReceiver->GetPlayerPawn();
	if (!pReceiverPawn || !pReceiverPawn->m_pWeaponServices)
	{
		ClientPrint(pCaller, HUD_PRINTTALK, EW_PREFIX "Receiver is no longer valid.");
		return;
	}

	if (iItemInstance < 0 || iItemInstance >= vecItems.size() || !vecItems[iItemInstance] || vecItems[iItemInstance]->iWeaponEnt == -1)
	{
		ClientPrint(pCaller, HUD_PRINTTALK, EW_PREFIX "Item is no longer valid.");
		return;
	}

	CBasePlayerWeapon* pItemWeapon = (CBasePlayerWeapon*)g_pEntitySystem->GetEntityInstance((CEntityIndex)vecItems[iItemInstance]->iWeaponEnt);
	if (!pItemWeapon)
	{
		ClientPrint(pCaller, HUD_PRINTTALK, EW_PREFIX "There was an error while transferring.");
		return;
	}
	gear_slot_t itemSlot = pItemWeapon->GetWeaponVData()->m_GearSlot();

	CCSPlayerController* pOwner = nullptr;

	// Gotta make current owner drop the weapon
	if (vecItems[iItemInstance]->iOwnerSlot != -1)
	{
		pOwner = CCSPlayerController::FromSlot(CPlayerSlot(vecItems[iItemInstance]->iOwnerSlot));
		if (!pOwner)
		{
			ClientPrint(pCaller, HUD_PRINTTALK, EW_PREFIX "There was an error while transferring.");
			return;
		}

		CCSPlayerPawn* pOwnerPawn = pOwner->GetPlayerPawn();
		if (!pOwnerPawn || !pOwnerPawn->m_pWeaponServices)
		{
			ClientPrint(pCaller, HUD_PRINTTALK, EW_PREFIX "There was an error while transferring.");
			return;
		}

		CUtlVector<CHandle<CBasePlayerWeapon>>* weapons = pOwnerPawn->m_pWeaponServices()->m_hMyWeapons();
		FOR_EACH_VEC(*weapons, i)
		{
			CBasePlayerWeapon* pWeapon = (*weapons)[i].Get();

			if (!pWeapon)
				continue;

			if (pWeapon->GetWeaponVData()->m_GearSlot() == itemSlot)
			{
				pOwnerPawn->m_pWeaponServices()->DropWeapon(pWeapon);
				break;
			}
		}
	}

	// Make receiver drop the weapon in the item slot
	CUtlVector<CHandle<CBasePlayerWeapon>>* weapons = pReceiverPawn->m_pWeaponServices()->m_hMyWeapons();
	FOR_EACH_VEC(*weapons, i)
	{
		CBasePlayerWeapon* pWeapon = (*weapons)[i].Get();

		if (!pWeapon)
			continue;

		if (pWeapon->GetWeaponVData()->m_GearSlot() == itemSlot)
		{
			pReceiverPawn->m_pWeaponServices()->DropWeapon(pWeapon);
			break;
		}
	}

	// Give the item to the receiver
	Vector vecOrigin = pReceiverPawn->GetAbsOrigin();
	pItemWeapon->Teleport(&vecOrigin, nullptr, nullptr);

	const char* pszCommandPlayerName = pCaller ? pCaller->GetPlayerName() : "Console";

	ZEPlayer* pZEReceiver = g_playerManager->GetPlayer(pReceiver->GetPlayerSlot());
	char sReceiverInfo[64];
	if (pZEReceiver->IsFakeClient())
		V_snprintf(sReceiverInfo, sizeof(sReceiverInfo), "%s [BOT]", pReceiver->GetPlayerName());
	else
		V_snprintf(sReceiverInfo, sizeof(sReceiverInfo), "%s [%llu]", pReceiver->GetPlayerName(), pZEReceiver->GetUnauthenticatedSteamId64());

	if (pOwner)
	{
		ZEPlayer* pZEOwner = g_playerManager->GetPlayer(pOwner->GetPlayerSlot());
		char sOwnerInfo[64];
		if (pZEOwner->IsFakeClient())
			V_snprintf(sOwnerInfo, sizeof(sOwnerInfo), "%s [BOT]", pOwner->GetPlayerName());
		else
			V_snprintf(sOwnerInfo, sizeof(sOwnerInfo), "%s [%llu]", pOwner->GetPlayerName(), pZEOwner->GetUnauthenticatedSteamId64());

		Message("[EntWatch] %s transferred %s from %s to %s\n",
				pszCommandPlayerName,
				g_pEWHandler->vecItems[iItemInstance]->szItemName.c_str(),
				sOwnerInfo,
				sReceiverInfo);

		ClientPrintAll(HUD_PRINTTALK, EW_PREFIX "Admin\x02 %s\x01 has transferred%s %s\x01 from\x02 %s\x01 to\x02 %s\x01.",
					   pszCommandPlayerName,
					   g_pEWHandler->vecItems[iItemInstance]->sChatColor,
					   g_pEWHandler->vecItems[iItemInstance]->szItemName.c_str(),
					   pOwner->GetPlayerName(),
					   pReceiver->GetPlayerName());
	}
	else
	{
		Message("[EntWatch] %s transferred %s to %s\n",
				pszCommandPlayerName,
				g_pEWHandler->vecItems[iItemInstance]->szItemName.c_str(),
				sReceiverInfo);

		ClientPrintAll(HUD_PRINTTALK, EW_PREFIX "Admin\x02 %s\x01 has transferred%s %s\x01 to\x02 %s\x01.",
					   pszCommandPlayerName,
					   g_pEWHandler->vecItems[iItemInstance]->sChatColor,
					   g_pEWHandler->vecItems[iItemInstance]->szItemName.c_str(),
					   pReceiver->GetPlayerName());
	}
}

void CEWHandler::AddUseHook(CBaseEntity* pEnt)
{
	static int offset = g_GameConfig->GetOffset("CBaseEntity::Use");
	if (offset == -1)
	{
		Panic("Failed to find CBaseEntity::Use offset in entwatch.cpp::AddUseHook() !!\n");
		return;
	}

	if (!V_strcmp(pEnt->GetClassname(), "func_button"))
	{
		if (iBaseBtnUseHookId == -1)
		{
			const auto pBaseButton_VTable = modules::server->FindVirtualTable("CBaseButton");
			SH_MANUALHOOK_RECONFIGURE(CBaseButton_Use, offset, 0, 0);
			iBaseBtnUseHookId = SH_ADD_MANUALDVPHOOK(CBaseButton_Use, pBaseButton_VTable, SH_MEMBER(this, &CEWHandler::Hook_Use), false);
		}
	}
	else if (!V_strcmp(pEnt->GetClassname(), "func_physbox"))
	{
		if (iPhysboxUseHookId == -1)
		{
			const auto pPhysbox_VTable = modules::server->FindVirtualTable("CPhysBox");
			SH_MANUALHOOK_RECONFIGURE(CPhysBox_Use, offset, 0, 0);
			iPhysboxUseHookId = SH_ADD_MANUALDVPHOOK(CPhysBox_Use, pPhysbox_VTable, SH_MEMBER(this, &CEWHandler::Hook_Use), false);
		}
	}
	else if (!V_strcmp(pEnt->GetClassname(), "func_rot_button"))
	{
		if (iRotBtnUseHookId == -1)
		{
			const auto pCRotButton_VTable = modules::server->FindVirtualTable("CRotButton");
			SH_MANUALHOOK_RECONFIGURE(CRotButton_Use, offset, 0, 0);
			iRotBtnUseHookId = SH_ADD_MANUALDVPHOOK(CRotButton_Use, pCRotButton_VTable, SH_MEMBER(this, &CEWHandler::Hook_Use), false);
		}
	}
	else if (!V_strcmp(pEnt->GetClassname(), "momentary_rot_button"))
	{
		if (iMomRotBtnUseHookId == -1)
		{
			const auto pCMomentaryRotButton_VTable = modules::server->FindVirtualTable("CMomentaryRotButton");
			SH_MANUALHOOK_RECONFIGURE(CMomentaryRotButton_Use, offset, 0, 0);
			iMomRotBtnUseHookId = SH_ADD_MANUALDVPHOOK(CMomentaryRotButton_Use, pCMomentaryRotButton_VTable, SH_MEMBER(this, &CEWHandler::Hook_Use), false);
		}
	}
	else if (!V_strcmp(pEnt->GetClassname(), "func_physical_button"))
	{
		if (iPhysicalBtnUseHookId == -1)
		{
			const auto pCPhysicalButton_VTable = modules::server->FindVirtualTable("CPhysicalButton");
			SH_MANUALHOOK_RECONFIGURE(CPhysicalButton_Use, offset, 0, 0);
			iPhysicalBtnUseHookId = SH_ADD_MANUALDVPHOOK(CPhysicalButton_Use, pCPhysicalButton_VTable, SH_MEMBER(this, &CEWHandler::Hook_Use), false);
		}
	}

	vecUseHookedEntities.push_back(pEnt->GetHandle());
}

void CEWHandler::RemoveUseHook(CBaseEntity* pEnt)
{
	for (int i = 0; i < (vecUseHookedEntities).size(); i++)
	{
		if (vecUseHookedEntities[i] == pEnt->GetHandle())
		{
			vecUseHookedEntities.erase(vecUseHookedEntities.begin() + i);
			return;
		}
	}
}

void CEWHandler::RemoveAllUseHooks()
{
	Message("[EntWatch] Fully unhooking use hooks\n");

	SH_REMOVE_HOOK_ID(iBaseBtnUseHookId);
	iBaseBtnUseHookId = -1;

	SH_REMOVE_HOOK_ID(iPhysboxUseHookId);
	iPhysboxUseHookId = -1;

	SH_REMOVE_HOOK_ID(iRotBtnUseHookId);
	iRotBtnUseHookId = -1;

	SH_REMOVE_HOOK_ID(iMomRotBtnUseHookId);
	iMomRotBtnUseHookId = -1;

	SH_REMOVE_HOOK_ID(iPhysicalBtnUseHookId);
	iPhysicalBtnUseHookId = -1;

	vecUseHookedEntities.clear();
}

void CEWHandler::Hook_Use(InputData_t* pInput)
{
	// Message("#####    USE HOOK    #####\n");
	CBaseEntity* pEntity = META_IFACEPTR(CBaseEntity);
	if (!pEntity)
		RETURN_META(MRES_IGNORED);

	bool bFound = false;
	for (int i = 0; i < (vecUseHookedEntities).size(); i++)
	{
		if (vecUseHookedEntities[i] == pEntity->GetHandle())
		{
			bFound = true;
			break;
		}
	}

	if (!bFound)
		RETURN_META(MRES_IGNORED);

	int index = pEntity->entindex();
	int itemIndex = -1;
	int handlerIndex = -1;
	for (int i = 0; i < (vecItems).size(); i++)
	{
		int j = vecItems[i]->FindHandlerByEntIndex(index);
		if (j != -1)
		{
			itemIndex = i;
			handlerIndex = j;
			break;
		}
	}

	if (itemIndex == -1 || handlerIndex == -1)
		RETURN_META(MRES_IGNORED);

	// Prevent uses from non item owners if we are filtering
	META_RES resVal = MRES_IGNORED;
	if (g_cvarEnableFiltering.Get())
		resVal = MRES_SUPERCEDE;

	CBaseEntity* pActivator = pInput->pActivator;

	if (!pActivator || !pActivator->IsPawn())
		RETURN_META(resVal);

	std::shared_ptr<EWItemInstance> pItem = vecItems[itemIndex];
	CCSPlayerPawn* pPawn = (CCSPlayerPawn*)pActivator;
	CCSPlayerController* pController = pPawn->GetOriginalController();

	if (!pController || pController->GetPlayerSlot() != pItem->iOwnerSlot)
		RETURN_META(resVal);

	const char* classname = pEntity->GetClassname();

	if (!strcmp(classname, "func_button") || !strcmp(classname, "func_rot_button") || !strcmp(classname, "momentary_rot_button") || !strcmp(classname, "func_physical_button"))
	{
		CBaseButton* pButton = (CBaseButton*)pEntity;
		if (pButton->m_bLocked || pButton->m_bDisabled)
			RETURN_META(resVal);
	}

	//
	// WE SHOW USE MESSAGE IN FireOutput
	// This is just to prevent unnecessary stuff with buttons like movement
	//

	RETURN_META(MRES_IGNORED);
}

// Update cd and uses of all held items
float EW_UpdateHud()
{
	if (!GetGlobals())
		return EW_HUD_TICKRATE;

	std::string sHudText = "";
	std::string sHudTextNoPlayerNames = "";
	static bool bWasEmptyPreviously = false;

	bool bFirst = true;
	for (int i = 0; i < (g_pEWHandler->vecItems).size(); i++)
	{
		std::shared_ptr<EWItemInstance> pItem = g_pEWHandler->vecItems[i];
		if (!pItem)
			continue;

		if (pItem->iOwnerSlot == -1 || pItem->iWeaponEnt == -1)
			continue;

		if (!pItem->bShowHud)
			continue;

		CCSPlayerController* pOwner = CCSPlayerController::FromSlot(CPlayerSlot(pItem->iOwnerSlot));
		if (!pOwner)
			continue;

		std::string sItemText = pItem->GetHandlerStateText();

		// TODO: std::format not supported in clang16 by default
		// sHudText.append(std::format("\n[{}]{}: {}", sItemText, pItem->szShortName, pOwner->GetPlayerName()));
		if (!bFirst)
		{
			sHudText.append("\n");
			sHudTextNoPlayerNames.append("\n");
		}
		else
			bFirst = false;

		sHudText.append("[");
		sHudText.append(sItemText);
		sHudText.append("]");
		sHudText.append(pItem->szShortName);
		sHudText.append(": ");
		sHudText.append(pOwner->GetPlayerName());

		// sHudTextNoPlayerNames.append(std::format("\n[{}]{}", sItemText, pItem->szShortName));
		sHudTextNoPlayerNames.append("[");
		sHudTextNoPlayerNames.append(sItemText);
		sHudTextNoPlayerNames.append("]");
		sHudTextNoPlayerNames.append(pItem->szShortName);
	}

	if (sHudText != "")
	{
		bWasEmptyPreviously = false;
	}
	else
	{
		if (bWasEmptyPreviously)
			return EW_HUD_TICKRATE;
		bWasEmptyPreviously = true;
	}

	for (int i = 0; i < GetGlobals()->maxClients; i++)
	{
		CCSPlayerController* pController = CCSPlayerController::FromSlot(i);
		if (!pController)
			continue;
		auto pPawn = pController->GetPawn();
		if (!pPawn)
			continue;
		ZEPlayer* zpPlayer = g_playerManager->GetPlayer(CPlayerSlot(i));
		if (!zpPlayer)
			continue;

		EWHudMode mode = (EWHudMode)(zpPlayer->GetEntwatchHudMode());

		CPointWorldText* pText = zpPlayer->GetEntwatchHud();
		if (!pText)
			continue;

		if (mode == EWHudMode::Hud_On)
			pText->AcceptInput("SetMessage", sHudText.c_str());
		else if (mode == EWHudMode::Hud_ItemOnly)
			pText->AcceptInput("SetMessage", sHudTextNoPlayerNames.c_str());
		else
			pText->AcceptInput("SetMessage", "");
	}

	return EW_HUD_TICKRATE;
}

void EW_OnLevelInit(const char* sMapName)
{
	g_pEWHandler->UnLoadConfig();
	g_pEWHandler->LoadMapConfig(sMapName);
}

void EW_RoundPreStart()
{
	if (!g_pEWHandler)
		return;

	g_pEWHandler->ResetAllClantags();
	g_pEWHandler->ClearItems();
	g_pEWHandler->m_bHudTicking = false;
}

void EW_OnEntitySpawned(CEntityInstance* pEntity)
{
	if (!g_pEWHandler || !g_pEWHandler->bConfigLoaded)
		return;

	CBaseEntity* pEnt = (CBaseEntity*)pEntity;
	if (!pEnt)
		return;

	if (!V_strcmp(pEnt->m_sUniqueHammerID().Get(), ""))
		return;

	const char* classname = pEnt->GetClassname();
	if (!strncmp(classname, "weapon_", 7))
	{
		int itemindex = g_pEWHandler->RegisterItem((CBasePlayerWeapon*)pEntity);
		if (itemindex != -1)
		{
			std::shared_ptr<EWItemInstance> item = g_pEWHandler->vecItems[itemindex];
			new CTimer(0.5, false, false, [item] {
				if (item)
					item->FindExistingHandlers();
				return -1.0f;
			});
		}
		return;
	}

	if (!strncmp(classname, "trigger_", 8))
		if (g_pEWHandler->RegisterTrigger(pEnt))
			return;

	// dont check lights
	if (!strncmp(classname, "light_", 6))
		return;

	// delay it cuz stupid spawn orders

	CHandle<CBaseEntity> hEntity = pEnt->GetHandle();
	new CTimer(0.25, false, false, [hEntity] {
		if (hEntity.Get())
			g_pEWHandler->RegisterHandler(hEntity.Get());
		return -1.0;
	});
}

void EW_OnEntityDeleted(CEntityInstance* pEntity)
{
	if (!g_pEWHandler || !g_pEWHandler->bConfigLoaded)
		return;

	CBaseEntity* pEnt = (CBaseEntity*)pEntity;
	if (!V_strcmp(pEnt->m_sUniqueHammerID().Get(), ""))
		return;

	const char* classname = pEnt->GetClassname();
	if (!strncmp(classname, "weapon_", 7))
	{
		EW_OnWeaponDeleted(pEnt);
		return;
	}

	if (!strncmp(classname, "trigger_", 8))
		if (g_pEWHandler->RemoveTrigger(pEnt))
			return;

	// dont check lights
	if (!strncmp(classname, "light_", 6))
		return;

	g_pEWHandler->RemoveHandler(pEnt);
}

void EW_OnWeaponDeleted(CBaseEntity* pEntity)
{
	int id = g_pEWHandler->FindItemInstanceByWeapon(pEntity->entindex());
	if (id != -1 && id < g_pEWHandler->vecItems.size())
		g_pEWHandler->RemoveWeaponFromItem(id);
}

bool EW_Detour_CCSPlayer_WeaponServices_CanUse(CCSPlayer_WeaponServices* pWeaponServices, CBasePlayerWeapon* pPlayerWeapon)
{
	// false=block it, true=dont block it
	if (!g_pEWHandler || !g_pEWHandler->bConfigLoaded)
		return true;

	CCSPlayerPawn* pPawn = pWeaponServices->GetPawn();
	if (!pPawn)
		return true;

	// We don't care about weapons that don't have a hammerid
	if (!V_strcmp(pPlayerWeapon->m_sUniqueHammerID().Get(), ""))
		return true;

	CCSPlayerController* ccsPlayer = pPawn->GetOriginalController();
	if (!ccsPlayer)
		return false;
	ZEPlayer* zpPlayer = ccsPlayer->GetZEPlayer();
	if (!zpPlayer || zpPlayer->IsEbanned())
		return false;

	return true;
}

void EW_Detour_CCSPlayer_WeaponServices_EquipWeapon(CCSPlayer_WeaponServices* pWeaponServices, CBasePlayerWeapon* pWeapon)
{
	if (!g_pEWHandler || !g_pEWHandler->bConfigLoaded)
		return;

	CCSPlayerPawn* pPawn = pWeaponServices->GetPawn();
	if (!pPawn)
		return;

	// We don't care about weapons that don't have a hammerid
	if (!V_strcmp(pWeapon->m_sUniqueHammerID().Get(), ""))
		return;

	int i = g_pEWHandler->FindItemInstanceByWeapon(pWeapon->entindex());
	if (i == -1)
		return;

	g_pEWHandler->PlayerPickup(pPawn, i);
}

void EW_DropWeapon(CCSPlayer_WeaponServices* pWeaponServices, CBasePlayerWeapon* pWeapon)
{
	if (!g_pEWHandler || !g_pEWHandler->bConfigLoaded)
		return;

	CCSPlayerPawn* pPawn = pWeaponServices->GetPawn();
	if (!pPawn)
		return;

	// We don't care about weapons that don't have a hammerid
	if (!V_strcmp(pWeapon->m_sUniqueHammerID().Get(), ""))
		return;

	// Check if this weapon is in the config
	int i = g_pEWHandler->FindItemInstanceByWeapon(pWeapon->entindex());
	if (i == -1)
		return;

	CCSPlayerController* pController = pPawn->GetOriginalController();
	// Message("EWDROP: m_iConnected:%d  team:%d  isalive:%d\n", pController->m_iConnected(), pController->m_iTeamNum(), pPawn->IsAlive());

	// If team is no longer the same, we have been infected
	if (g_pEWHandler->vecItems[i]->iTeamNum != pPawn->m_iTeamNum())
		return;

	// Players who have died or disconnected are not alive when dropping
	if (!pPawn->IsAlive())
		return;

	// Player has disconnected
	if (!pController->IsConnected())
		return;

	CHandle<CCSPlayerController> hController = pPawn->m_hOriginalController;

	g_pEWHandler->PlayerDrop(EWDropReason::Drop, i, hController.Get());
}

void EW_PlayerDeath(IGameEvent* pEvent)
{
	CCSPlayerController* pVictim = (CCSPlayerController*)pEvent->GetPlayerController("userid");
	if (!pVictim)
		return;
	// Message("EWDEATH: m_iConnected:%d  team:%d\n", pVictim->m_iConnected(), pVictim->m_iTeamNum());
	//  Disconnecting players return false from IsConnected
	if (!pVictim || !pVictim->IsConnected())
		return;

	if (pEvent->GetBool("infected"))
		g_pEWHandler->PlayerDrop(EWDropReason::Infected, -1, pVictim);
	else
		g_pEWHandler->PlayerDrop(EWDropReason::Death, -1, pVictim);
}

// Needed for when weapons are force dropped just before death
void EW_PlayerDeathPre(CCSPlayerController* pController)
{
	// Message("EWDEATHPRE: m_iConnected:%d  team:%d\n", pController->m_iConnected(), pController->m_iTeamNum());
	if (!g_pEWHandler->IsConfigLoaded())
		return;

	if (!pController->IsConnected())
		return;

	g_pEWHandler->PlayerDrop(EWDropReason::Death, -1, pController);
}

void EW_PlayerDisconnect(int slot)
{
	auto i = g_pEWHandler->mapTransfers.find(slot);
	if (i != g_pEWHandler->mapTransfers.end())
		g_pEWHandler->mapTransfers.erase(slot);

	CCSPlayerController* pController = CCSPlayerController::FromSlot(slot);
	if (!pController)
		return;

	// Message("EWDISCONNECT: m_iConnected:%d  team:%d \n", pController->m_iConnected(), pController->m_iTeamNum());
	g_pEWHandler->PlayerDrop(EWDropReason::Disconnect, -1, pController);
}

void EW_SendBeginNewMatchEvent()
{
	if (!GetGlobals())
		return;

	IGameEvent* pEvent = g_gameEventManager->CreateEvent("begin_new_match");
	if (!pEvent)
	{
		Panic("Failed to create begin_new_match event\n");
		return;
	}

	INetworkMessageInternal* pMsg = g_pNetworkMessages->FindNetworkMessageById(GE_Source1LegacyGameEvent);
	if (!pMsg)
	{
		Panic("Failed to create Source1LegacyGameEvent\n");
		return;
	}
	CNetMessagePB<CMsgSource1LegacyGameEvent>* data = pMsg->AllocateMessage()->ToPB<CMsgSource1LegacyGameEvent>();
	g_gameEventManager->SerializeEvent(pEvent, data);

	CRecipientFilter filter;
	for (int i = 0; i < GetGlobals()->maxClients; i++)
	{
		ZEPlayer* pPlayer = g_playerManager->GetPlayer(i);

		if (!pPlayer || pPlayer->IsFakeClient() || !pPlayer->IsAuthenticated())
			continue;

		if (pPlayer->GetEntwatchClangtags())
			filter.AddRecipient(pPlayer->GetPlayerSlot());
	}

	g_gameEventSystem->PostEventAbstract(-1, false, &filter, pMsg, data, 0);
	delete data;
}

bool EW_IsFireOutputHooked()
{
	return std::any_of(mapIOFunctions.begin(), mapIOFunctions.end(), [](const auto& p) { return p.first == "entwatch"; });
}

void EW_FireOutput(const CEntityIOOutput* pThis, CEntityInstance* pActivator, CEntityInstance* pCaller, const CVariant* value, float flDelay)
{
	if (!EW_IsFireOutputHooked() || !pCaller)
		return;

	for (int i = 0; i < (g_pEWHandler->vecItems).size(); i++)
	{
		if (g_pEWHandler->vecItems[i]->iWeaponEnt == -1)
			continue;

		if (g_pEWHandler->vecItems[i]->vecHandlers.size() <= 0)
			continue;

		for (int j = 0; j < (g_pEWHandler->vecItems[i]->vecHandlers).size(); j++)
		{
			std::shared_ptr<EWItemHandler> handler = g_pEWHandler->vecItems[i]->vecHandlers[j];
			if (pCaller->GetEntityIndex().Get() != handler->iEntIndex)
				continue;

			// Message("item(%d) handler(%d) ent had an output: %s fire\n", i, j, pThis->m_pDesc->m_pName);
			if (V_stricmp(pThis->m_pDesc->m_pName, handler->szOutput.c_str()))
				continue;

			// Message("Output for item %s (instance:%d)  handler:%d outputname:%s\n", g_pEWHandler->vecItems[i]->szItemName, i, j, pThis->m_pDesc->m_pName);
			if (handler->type == EWHandlerType::CounterDown || handler->type == EWHandlerType::CounterUp)
				handler->Use(value->m_float32);
			else
				handler->Use(0.0);
		}
	}
}

/* Gets the trailing number on a given string in the form XXXXXX_1
   Used ingame as the template suffix
   which gets added to entities spawned from a template
 * Returns -1 if not found
 */
int GetTemplateSuffixNumber(const char* szName)
{
	size_t len = strlen(szName);

	// needs at least 3 characters to include the suffix
	if (len < 3)
		return -1;

	int i = len - 1;

	// doesnt end with a number so wasnt templated
	if (!isdigit(szName[i]))
		return -1;

	while (i >= 0 && isdigit(szName[i]))
		i--;

	// if the first character is the first non-number then it wasnt templated
	if (i <= 0)
		return -1;

	if (szName[i] == '_')
		return V_StringToInt64(szName + i + 1, -1);

	return -1;
}

CON_COMMAND_CHAT_FLAGS(ew_reload, "- Reloads the current map's entwatch config", ADMFLAG_CONFIG)
{
	if (!g_cvarEnableEntWatch.Get() || !GetGlobals())
		return;
	if (!g_pEWHandler)
	{
		ClientPrint(player, HUD_PRINTTALK, EW_PREFIX "There has been an error initialising entwatch.");
		return;
	}

	// LoadConfig unloads the current config already
	g_pEWHandler->LoadMapConfig(GetGlobals()->mapname.ToCStr());

	if (!g_pEWHandler->bConfigLoaded)
	{
		ClientPrint(player, HUD_PRINTTALK, EW_PREFIX "Error reloading config, check console log for details.");
		return;
	}

	ClientPrint(player, HUD_PRINTTALK, EW_PREFIX "Config reloaded successfully.");
}

CON_COMMAND_CHAT_FLAGS(etransfer, "<owner/$itemname> <receiver> - Transfer an EntWatch item", ADMFLAG_GENERIC)
{
	if (!g_cvarEnableEntWatch.Get() || !GetGlobals())
		return;

	if (!g_pEWHandler->IsConfigLoaded())
	{
		ClientPrint(player, HUD_PRINTTALK, EW_PREFIX "No EntWatch config is currently loaded!");
		return;
	}

	const char* pszCommandPlayerName = player ? player->GetPlayerName() : "Console";

	bool bHasOngoing = false;
	auto ongoingTransfer = g_pEWHandler->mapTransfers.find(player->GetPlayerSlot());
	if (ongoingTransfer != g_pEWHandler->mapTransfers.end())
	{
		bHasOngoing = true;
		Message("Ongoing transfer found...\n");
		std::shared_ptr<ETransferInfo> transferInfo = ongoingTransfer->second;
		if (GetGlobals()->curtime - transferInfo->flTime > 60.0)
		{
			// Transfer was not been completed within 60 seconds, cancel automatically
			g_pEWHandler->mapTransfers.erase(player->GetPlayerSlot());
			bHasOngoing = false;
		}
		else
		{
			// A transfer was previously started, check if they entered a number
			if (args.ArgC() == 2)
			{
				int id = Q_atoi(args[1]);
				if (id <= 0 || id > transferInfo->itemIds.size())
				{
					ClientPrint(player, HUD_PRINTTALK, EW_PREFIX "Number must be between 1 and %d, try again", transferInfo->itemIds.size());
					return;
				}
				else
				{
					// Valid number, try and do the transfer
					id = id - 1;
					g_pEWHandler->Transfer(player, transferInfo->itemIds[id], transferInfo->hReceiver);
					g_pEWHandler->mapTransfers.erase(player->GetPlayerSlot());
					return;
				}
			}
		}
	}

	if (args.ArgC() < 3)
	{
		ClientPrint(player, HUD_PRINTTALK, EW_PREFIX "Usage: !etransfer <owner>/$<itemname> <receiver>");
		return;
	}

	int iNumClients = 0;
	int pSlots[MAXPLAYERS];

	// Find receiver
	if (!g_playerManager->CanTargetPlayers(player, args[2], iNumClients, pSlots, NO_MULTIPLE | NO_SPECTATOR | NO_DEAD))
	{
		// CanTargetPlayers prints error string if false
		return;
	}

	if (CCSPlayerController::FromSlot(pSlots[0])->GetZEPlayer()->IsEbanned())
	{
		ClientPrint(player, HUD_PRINTTALK, EW_PREFIX "Specified receiver is ebanned.");
		return;
	}

	CCSPlayerController* pReceiver = CCSPlayerController::FromSlot(pSlots[0]);
	if (!pReceiver)
	{
		ClientPrint(player, HUD_PRINTTALK, EW_PREFIX "Error getting receiver.");
		return;
	}

	CCSPlayerPawn* pReceiverPawn = pReceiver->GetPlayerPawn();
	if (!pReceiverPawn || !pReceiverPawn->m_pWeaponServices)
	{
		ClientPrint(player, HUD_PRINTTALK, EW_PREFIX "Error getting receiver.");
		return;
	}
	// Receiver found

	// Transfer by item name
	if (args[1][0] == '$')
	{
		// double $ for EXACT name matching
		bool bExact = false;
		if (strlen(args[1]) > 1)
			bExact = (args[1][1] == '$');

		char sItemName[64];
		if (bExact)
			V_strncpy(sItemName, args[1] + 2, sizeof(sItemName));
		else
			V_strncpy(sItemName, args[1] + 1, sizeof(sItemName));

		std::vector<int> itemIds;
		int itemCount = 0;
		int iItemInstance = g_pEWHandler->FindItemInstanceByName(sItemName, true, bExact, 0);

		while (iItemInstance != -1)
		{
			itemCount++;
			itemIds.push_back(iItemInstance);
			iItemInstance = g_pEWHandler->FindItemInstanceByName(sItemName, true, bExact, iItemInstance + 1);
		}

		if (itemCount == 0)
		{
			if (bExact)
				ClientPrint(player, HUD_PRINTTALK, EW_PREFIX "Could not find an item with the exact name:\x04 %s", sItemName);
			else
				ClientPrint(player, HUD_PRINTTALK, EW_PREFIX "Could not find an item with the name:\x04 %s", sItemName);
			return;
		}
		else if (itemCount == 1)
		{
			g_pEWHandler->Transfer(player, itemIds[0], pReceiver->GetHandle());
			return;
		}

		// itemCount > 1
		ClientPrint(player, HUD_PRINTTALK, EW_PREFIX "Found %d items matching\x04 \"%s\"", itemCount, sItemName);
		ClientPrint(player, HUD_PRINTTALK, EW_PREFIX "Enter\x02 !etransfer <number>\x01 to complete the transfer");

		std::shared_ptr<ETransferInfo> transferInfo = std::make_shared<ETransferInfo>(pReceiver->GetHandle(), GetGlobals()->curtime);

		for (int i = 0; i < itemIds.size(); i++)
		{
			std::shared_ptr<EWItemInstance> pItem = g_pEWHandler->vecItems[itemIds[i]];
			std::string sItemText = pItem->GetHandlerStateText();
			std::string sOwnerInfo = "\x08(No owners)";
			if (pItem->iOwnerSlot != -1)
			{
				CCSPlayerController* pOwner = CCSPlayerController::FromSlot(CPlayerSlot(pItem->iOwnerSlot));
				if (!pOwner)
					sOwnerInfo = "\x05(Owner:\x02 ERROR\x05)";
				else
				{
					sOwnerInfo = "\x05(Owner: ";
					sOwnerInfo.append(pOwner->GetPlayerName());
					sOwnerInfo.append(")");
				}
			}
			else if (pItem->sLastOwnerName != "")
			{
				sOwnerInfo = "\x10(Previous owner: ";
				sOwnerInfo.append(pItem->sLastOwnerName);
				sOwnerInfo.append(")");
			}

			ClientPrint(player, HUD_PRINTTALK, EW_PREFIX "\x0E%d.\x01 %s [%s] %s", i + 1, pItem->szItemName.c_str(), sItemText.c_str(), sOwnerInfo.c_str());

			transferInfo->itemIds.push_back(itemIds[i]);
		}

		if (bHasOngoing)
			g_pEWHandler->mapTransfers.erase(player->GetPlayerSlot());

		g_pEWHandler->mapTransfers[player->GetPlayerSlot()] = transferInfo;
		return;
	}

	// Player to player transfer
	iNumClients = 0;

	// Find owner
	if (!g_playerManager->CanTargetPlayers(player, args[1], iNumClients, pSlots, NO_MULTIPLE | NO_SPECTATOR | NO_DEAD))
	{
		// CanTargetPlayers prints error string if false
		return;
	}
	if (!iNumClients)
	{
		ClientPrint(player, HUD_PRINTTALK, EW_PREFIX "Item owner not found.");
		return;
	}
	CCSPlayerController* pOwner = CCSPlayerController::FromSlot(pSlots[0]);

	if (!pOwner)
	{
		ClientPrint(player, HUD_PRINTTALK, EW_PREFIX "Error getting item owner.");
		return;
	}
	CCSPlayerPawn* pOwnerPawn = pOwner->GetPlayerPawn();
	if (!pOwnerPawn || !pOwnerPawn->m_pWeaponServices)
	{
		ClientPrint(player, HUD_PRINTTALK, EW_PREFIX "Error getting item owner.");
		return;
	}
	// Owner found

	std::vector<int> itemIds;
	int itemCount = 0;
	int iItemInstance = g_pEWHandler->FindItemInstanceByOwner(pOwner->GetPlayerSlot(), true, 0);

	while (iItemInstance != -1)
	{
		itemCount++;
		itemIds.push_back(iItemInstance);
		iItemInstance = g_pEWHandler->FindItemInstanceByOwner(pOwner->GetPlayerSlot(), true, iItemInstance + 1);
	}

	if (itemCount == 0)
	{
		ClientPrint(player, HUD_PRINTTALK, EW_PREFIX "%s does not have an item that can be transferred.", pOwner->GetPlayerName());
		return;
	}
	else if (itemCount == 1)
	{
		g_pEWHandler->Transfer(player, itemIds[0], pReceiver->GetHandle());
		return;
	}

	// itemCount > 1
	ClientPrint(player, HUD_PRINTTALK, EW_PREFIX "Found %d items owned by\x04 \"%s\"", itemCount, pOwner->GetPlayerName());
	ClientPrint(player, HUD_PRINTTALK, EW_PREFIX "Enter\x02 !etransfer <number>\x01 to complete the transfer");

	std::shared_ptr<ETransferInfo> transferInfo = std::make_shared<ETransferInfo>(pReceiver->GetHandle(), GetGlobals()->curtime);

	for (int i = 0; i < itemIds.size(); i++)
	{
		std::shared_ptr<EWItemInstance> pItem = g_pEWHandler->vecItems[itemIds[i]];
		std::string sItemText = pItem->GetHandlerStateText();
		std::string sOwnerInfo = "\x05(Owner: ";
		sOwnerInfo.append(pOwner->GetPlayerName());
		sOwnerInfo.append(")");

		ClientPrint(player, HUD_PRINTTALK, EW_PREFIX "\x0E%d.\x01 %s [%s] %s", i + 1, pItem->szItemName.c_str(), sItemText.c_str(), sOwnerInfo.c_str());

		transferInfo->itemIds.push_back(itemIds[i]);
	}

	if (bHasOngoing)
		g_pEWHandler->mapTransfers.erase(player->GetPlayerSlot());

	g_pEWHandler->mapTransfers[player->GetPlayerSlot()] = transferInfo;
}

CON_COMMAND_CHAT(ew_dump, "- Prints the currently loaded config to console")
{
	if (!g_cvarEnableEntWatch.Get())
		return;

	if (!g_pEWHandler)
	{
		ClientPrint(player, HUD_PRINTTALK, EW_PREFIX "There has been an error initialising entwatch.");
		return;
	}

	g_pEWHandler->PrintLoadedConfig(player->GetPlayerSlot());
}

CON_COMMAND_CHAT(etag, "- Toggle EntWatch clantags on the scoreboard")
{
	if (!g_cvarEnableEntWatch.Get())
		return;

	if (!player)
	{
		ClientPrint(player, HUD_PRINTTALK, EW_PREFIX "Only usable in game.");
		return;
	}

	ZEPlayer* zpPlayer = g_playerManager->GetPlayer(player->GetPlayerSlot());
	if (!zpPlayer)
	{
		ClientPrint(player, HUD_PRINTTALK, EW_PREFIX "Something went wrong, try again later.");
		return;
	}

	bool bCurrentStatus = zpPlayer->GetEntwatchClangtags();
	bCurrentStatus = !bCurrentStatus;
	zpPlayer->SetEntwatchClangtags(bCurrentStatus);

	if (bCurrentStatus)
		ClientPrint(player, HUD_PRINTTALK, EW_PREFIX "You have\x04 Enabled\x01 EntWatch clantag updates");
	else
		ClientPrint(player, HUD_PRINTTALK, EW_PREFIX "You have\x07 Disabled\x01 EntWatch clantag updates");
}

CON_COMMAND_CHAT(hud, "- Toggle EntWatch HUD")
{
	if (!g_cvarEnableEntWatch.Get())
		return;

	if (!player)
	{
		ClientPrint(player, HUD_PRINTTALK, EW_PREFIX "Only usable in game.");
		return;
	}

	ZEPlayer* zpPlayer = g_playerManager->GetPlayer(player->GetPlayerSlot());
	if (!zpPlayer)
	{
		ClientPrint(player, HUD_PRINTTALK, EW_PREFIX "Something went wrong, try again later.");
		return;
	}

	EWHudMode mode = (EWHudMode)(zpPlayer->GetEntwatchHudMode());
	if (mode == EWHudMode::Hud_None)
		mode = EWHudMode::Hud_On;
	else if (mode == EWHudMode::Hud_On)
		mode = EWHudMode::Hud_ItemOnly;
	else
		mode = EWHudMode::Hud_None;

	zpPlayer->SetEntwatchHudMode((int)mode);

	if (mode == EWHudMode::Hud_None)
		ClientPrint(player, HUD_PRINTTALK, EW_PREFIX "EntWatch HUD\x07 disabled.");
	else if (mode == EWHudMode::Hud_On)
		ClientPrint(player, HUD_PRINTTALK, EW_PREFIX "EntWatch HUD\x04 enabled.");
	else
		ClientPrint(player, HUD_PRINTTALK, EW_PREFIX "EntWatch HUD\x04 enabled\x10 (Item names only).");
}

CON_COMMAND_CHAT(hudpos, "<x> <y> - Sets the position of the EntWatch hud.")
{
	if (!g_cvarEnableEntWatch.Get())
		return;

	if (!player)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "You cannot use this command from the server console.");
		return;
	}

	ZEPlayer* zpPlayer = g_playerManager->GetPlayer(player->GetPlayerSlot());
	if (!zpPlayer)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Something went wrong, try again later.");
		return;
	}

	if (args.ArgC() < 3)
	{
		ClientPrint(player, HUD_PRINTTALK, EW_PREFIX "Usage: !hudpos <x> <y>");
		ClientPrint(player, HUD_PRINTTALK, EW_PREFIX "Current hudpos: %.1fx %.1fy", zpPlayer->GetEntwatchHudX(), zpPlayer->GetEntwatchHudY());
		return;
	}

	float x = std::atof(args[1]);
	float y = std::atof(args[2]);

	zpPlayer->SetEntwatchHudPos(x, y);
	ClientPrint(player, HUD_PRINTTALK, EW_PREFIX "Set hudpos to: %.1fx %.1fy", x, y);
}

CON_COMMAND_CHAT(hudcolor, "<r> <g> <b> [a] - Set color (and transparency) of the Entwatch hud")
{
	if (!g_cvarEnableEntWatch.Get())
		return;

	if (!player)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "You cannot use this command from the server console.");
		return;
	}

	ZEPlayer* zpPlayer = g_playerManager->GetPlayer(player->GetPlayerSlot());
	if (!zpPlayer)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Something went wrong, try again later.");
		return;
	}

	Color clrCur = zpPlayer->GetEntwatchHudColor();
	if (args.ArgC() < 4)
	{
		ClientPrint(player, HUD_PRINTTALK, EW_PREFIX "Usage: !hudcolor <r> <g> <b> [Optional: alpha]");
		ClientPrint(player, HUD_PRINTTALK, EW_PREFIX "Current hudcolor:\x07 %d\x04 %d\x0C %d\x01 %d", clrCur.r(), clrCur.g(), clrCur.b(), clrCur.a());
		return;
	}

	int r = std::atoi(args[1]);
	int g = std::atoi(args[2]);
	int b = std::atoi(args[3]);
	int a = clrCur.a();
	Color newColor;
	if (args.ArgC() > 4)
	{
		a = std::atoi(args[4]);
		newColor.SetColor(r, g, b, a);
		ClientPrint(player, HUD_PRINTTALK, EW_PREFIX "Set hudcolor to:\x07 %d\x04 %d\x0C %d\x01 %d", newColor.r(), newColor.g(), newColor.b(), newColor.a());
	}
	else
	{
		newColor.SetColor(r, g, b, a);
		ClientPrint(player, HUD_PRINTTALK, EW_PREFIX "Set hudcolor to:\x07 %d\x04 %d\x0C %d", newColor.r(), newColor.g(), newColor.b());
	}

	zpPlayer->SetEntwatchHudColor(newColor);
}

CON_COMMAND_CHAT(hudsize, "<size> - Set font size of the EntWatch hud")
{
	if (!g_cvarEnableEntWatch.Get())
		return;

	if (!player)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "You cannot use this command from the server console.");
		return;
	}

	ZEPlayer* zpPlayer = g_playerManager->GetPlayer(player->GetPlayerSlot());
	if (!zpPlayer)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Something went wrong, try again later.");
		return;
	}

	if (args.ArgC() < 2)
	{
		ClientPrint(player, HUD_PRINTTALK, EW_PREFIX "Usage: !hudsize <size>");
		float flSize = zpPlayer->GetEntwatchHudSize();
		ClientPrint(player, HUD_PRINTTALK, EW_PREFIX "Current hud size:\x04 %.2f", flSize);
		return;
	}

	float size = std::atof(args[1]);
	if (size < 20.0f) // size 20 with 0.005 units per pixel is small
		size = 20.0f;
	else if (size > 255.0f) // game has a max of 255
		size = 255.0f;

	ClientPrint(player, HUD_PRINTTALK, EW_PREFIX "Hud font size set to:\x04 %.2f", size);
	zpPlayer->SetEntwatchHudSize(size);
}
