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

#pragma once

#include "common.h"
#include "ctimer.h"
#include "entity/ccsplayercontroller.h"
#include "entity/ccsplayerpawn.h"
#include "eventlistener.h"
#include "gamesystem.h"
#include "vendor/nlohmann/json_fwd.hpp"

using ordered_json = nlohmann::ordered_json;

#define EW_PREFIX " \4[EntWatch]\1 "

#define EW_PREF_HUD_MODE "entwatch_hud"
#define EW_PREF_CLANTAG "entwatch_clantag"
#define EW_PREF_HUDPOS_X "entwatch_hudpos_x"
#define EW_PREF_HUDPOS_Y "entwatch_hudpos_y"
#define EW_PREF_HUDCOLOR "entwatch_hudcolor"
#define EW_PREF_HUDSIZE "entwatch_hudfontsize"

#define EW_HUDPOS_X_DEFAULT -7.5f
#define EW_HUDPOS_Y_DEFAULT -2.0f

#define EW_HUDSIZE_DEFAULT 60.0f

#define EW_HUD_TICKRATE 0.5f

enum EWHandlerType
{
	Type_None,
	Button,		 // will auto hook +use
	CounterDown, // used for showing the value of a counter that stops when minimum reached
	CounterUp,	 // used for showing the value of a counter that stops when maximum reached
	Other,		 // anything else
};

enum EWHandlerMode
{
	EWMode_None = 1,
	Cooldown,		   /* Infinite uses, cooldown between each use, or R if ready to use */
	MaxUses,		   /* Limited uses, can have a cooldown between each use, or E if empty */
	CooldownAfterUses, /* Cooldown after all uses, allowing for more uses */
	CounterValue,	   /* show math_counter value, or E if empty */

	EWMode_Last,
};

enum EWDropReason
{
	Drop,
	Infected,
	Death,
	Disconnect,
	Deleted
};

enum EWHudMode
{
	Hud_None,
	Hud_On,
	Hud_ItemOnly,
};

enum EWAutoConfigOption
{
	EWCfg_Auto = -1,
	EWCfg_No,
	EWCfg_Yes,
};

struct EWItemInstance;
struct EWItemHandler
{
	// Config variables
	EWHandlerType type;
	EWHandlerMode mode;
	std::string szName;
	std::string szHammerid;
	std::string szOutput; /* Output name for when this is used e.g. OnPressed */
	float flCooldown;
	int iMaxUses;
	float flOffset;
	float flMaxOffset;
	bool bShowUse;				  /* Whether to show when this is used */
	bool bShowHud;				  /* Track this cd/uses on hud/scoreboard */
	EWAutoConfigOption templated; /* Is this entity templated (should we check for template suffix) */

	// Instance variables
	EWItemInstance* pItem;
	int iEntIndex;
	int iCurrentUses;
	float flCounterValue;
	float flCounterMax;
	std::string szHudText;
	float flLastUsed;	  // For tracking cd on the hud
	float flLastShownUse; // To prevent too much chat spam

	void SetDefaultValues();
	void Print();

public:
	EWItemHandler(std::shared_ptr<EWItemHandler> pOther);
	EWItemHandler(ordered_json jsonKeys);

	void RemoveHook();
	void RegisterEntity(CBaseEntity* pEnt);
	void Use(float flCounterValue);
	void UseCounter(float flCounterValue);
	void UpdateHudText();
};

struct EWItem
{
	int id;
	std::string szItemName;	 /* Name to show on pickup/drop/use */
	std::string szShortName; /* Name to show on hud/scoreboard */
	std::string szHammerid;	 /* Hammerid of the weapon */
	char sChatColor[2];
	bool bShowPickup;										 /* Whether to show pickup/drop messages in chat */
	bool bShowHud;											 /* Whether to show this item on hud/scoreboard */
	EWAutoConfigOption transfer;							 /* Can this item be transferred */
	EWAutoConfigOption templated;							 /* Is this item templated (should we check for template suffix) */
	std::vector<std::shared_ptr<EWItemHandler>> vecHandlers; /* List of item abilities */
	std::vector<std::string> vecTriggers;					 /* HammerIds of triggers associated with this item */

	void SetDefaultValues();
	void ParseColor(std::string value);

public:
	EWItem(std::shared_ptr<EWItem> pItem);
	EWItem(ordered_json jsonKeys, int _id);
};

struct EWItemInstance : EWItem /* Current instance of defined items */
{
	int iOwnerSlot; /* Slot of the current holder */
	int iWeaponEnt;
	int iTemplateNum;
	bool bAllowDrop; /* Whether this item should drop on death/disconnect only false for knife items */
	char sClantag[64];
	bool bHasThisClantag;
	int iTeamNum;
	std::string sLastOwnerName; // For etransfer info

public:
	EWItemInstance(int iWeapon, std::shared_ptr<EWItem> pItem) :
		EWItem(pItem),
		iOwnerSlot(-1),
		iWeaponEnt(iWeapon),
		iTemplateNum(-1),
		bAllowDrop(true),
		sClantag(""),
		bHasThisClantag(false),
		iTeamNum(CS_TEAM_NONE){};
	bool RegisterHandler(CBaseEntity* pEnt, int iHandlerTemplateNum);
	bool RemoveHandler(CBaseEntity* pEnt);
	int FindHandlerByEntIndex(int indexToFind);
	void FindExistingHandlers();

	void Pickup(int slot);
	void Drop(EWDropReason reason, CCSPlayerController* pController);
	std::string GetHandlerStateText();
};

struct ETransferInfo
{
	ETransferInfo(CHandle<CCSPlayerController> hRecv, float time)
	{
		hReceiver = hRecv;
		flTime = time;
	}

	CHandle<CCSPlayerController> hReceiver; // Player being given the item
	std::vector<int> itemIds;				// Item IDs that were targetted
	float flTime;							// The time when the command was initiated
};

class CEWHandler
{
public:
	CEWHandler()
	{
		bConfigLoaded = false;
		m_bHudTicking = false;

		iBaseBtnUseHookId = -1;
		iPhysboxUseHookId = -1;
		iPhysicalBtnUseHookId = -1;
		iRotBtnUseHookId = -1;
		iMomRotBtnUseHookId = -1;

		for (int i = 0; i < 3; i++)
		{
			iTriggerTeleportTouchHooks[i] = -1;
			iTriggerMultipleTouchHooks[i] = -1;
			iTriggerOnceTouchHooks[i] = -1;
		}
	}

	bool bConfigLoaded;
	bool IsConfigLoaded() { return bConfigLoaded; }

	void UnLoadConfig();
	void LoadMapConfig(const char* sMapName);
	void LoadConfig(const char* sFilePath);

	void PrintLoadedConfig(int iSlot) { PrintLoadedConfig(CPlayerSlot(iSlot)); };
	void PrintLoadedConfig(CPlayerSlot slot);

	void ClearItems();

	int FindItemInstanceByWeapon(int iWeaponEnt);
	int FindItemInstanceByOwner(int iOwnerSlot, bool bOnlyTransferrable, int iStartItem);
	int FindItemInstanceByName(std::string sItemName, bool bOnlyTransferrable, bool bExact, int iStartItem);

	void RegisterHandler(CBaseEntity* pEnt);
	bool RegisterTrigger(CBaseEntity* pEnt);
	void AddTouchHook(CBaseEntity* pEnt);
	void Hook_Touch(CBaseEntity* pOther);
	bool RemoveTrigger(CBaseEntity* pEnt);
	void RemoveAllTriggers();
	void RemoveHandler(CBaseEntity* pEnt);
	void ResetAllClantags();

	int RegisterItem(CBasePlayerWeapon* pWeapon);
	void RemoveWeaponFromItem(int itemId);
	void PlayerPickup(CCSPlayerPawn* pPawn, int iItemInstance);
	void PlayerDrop(EWDropReason reason, int iItemInstance, CCSPlayerController* pController);
	void Transfer(CCSPlayerController* pCaller, int iItemInstance, CHandle<CCSPlayerController> hReceiver);

	void AddUseHook(CBaseEntity* pEnt);
	void RemoveUseHook(CBaseEntity* pEnt);
	void RemoveAllUseHooks();
	void Hook_Use(InputData_t* pInput);

	std::map<uint32, std::shared_ptr<EWItem>> mapItemConfig; /* items defined in the config */
	std::vector<std::shared_ptr<EWItemInstance>> vecItems;	 /* all items found spawned */

	std::vector<CHandle<CBaseEntity>> vecHookedTriggers;
	int iTriggerTeleportTouchHooks[3];
	int iTriggerMultipleTouchHooks[3];
	int iTriggerOnceTouchHooks[3];

	std::vector<CHandle<CBaseEntity>> vecUseHookedEntities;
	int iBaseBtnUseHookId;
	int iPhysboxUseHookId;
	int iPhysicalBtnUseHookId;
	int iRotBtnUseHookId;
	int iMomRotBtnUseHookId;

	bool m_bHudTicking;

	std::map<int, std::shared_ptr<ETransferInfo>> mapTransfers; // Any etransfers that target multiple items
};

extern CEWHandler* g_pEWHandler;
extern bool g_bEnableEntWatch;

void EW_OnLevelInit(const char* sMapName);
void EW_RoundPreStart();
void EW_OnEntitySpawned(CEntityInstance* pEntity);
void EW_OnEntityDeleted(CEntityInstance* pEntity);
void EW_OnWeaponDeleted(CBaseEntity* pEntity);
bool EW_Detour_CCSPlayer_WeaponServices_CanUse(CCSPlayer_WeaponServices* pWeaponServices, CBasePlayerWeapon* pPlayerWeapon);
void EW_Detour_CCSPlayer_WeaponServices_EquipWeapon(CCSPlayer_WeaponServices* pWeaponServices, CBasePlayerWeapon* pPlayerWeapon);
void EW_DropWeapon(CCSPlayer_WeaponServices* pWeaponServices, CBasePlayerWeapon* pWeapon);
void EW_PlayerDeath(IGameEvent* pEvent);
void EW_PlayerDeathPre(CCSPlayerController* pController);
void EW_PlayerDisconnect(int slot);
void EW_SendBeginNewMatchEvent();
bool EW_IsFireOutputHooked();
void EW_FireOutput(const CEntityIOOutput* pThis, CEntityInstance* pActivator, CEntityInstance* pCaller, const CVariant* value, float flDelay);
int GetTemplateSuffixNumber(const char* szName);
float EW_UpdateHud();