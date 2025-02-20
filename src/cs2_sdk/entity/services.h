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
#include "globaltypes.h"
#include "viewmodels.h"
#include <entity/ccsplayerpawn.h>
#include <entity/ccsweaponbase.h>
#include <platform.h>
#include <unordered_map>

#define AMMO_OFFSET_HEGRENADE 13
#define AMMO_OFFSET_FLASHBANG 14
#define AMMO_OFFSET_SMOKEGRENADE 15
#define AMMO_OFFSET_MOLOTOV 16
#define AMMO_OFFSET_DECOY 17

struct CSPerRoundStats_t
{
public:
	DECLARE_SCHEMA_CLASS_INLINE(CSPerRoundStats_t)

	SCHEMA_FIELD(int, m_iKills)
	SCHEMA_FIELD(int, m_iDeaths)
	SCHEMA_FIELD(int, m_iAssists)
	SCHEMA_FIELD(int, m_iDamage)
};

struct CSMatchStats_t : public CSPerRoundStats_t
{
public:
	DECLARE_SCHEMA_CLASS_INLINE(CSMatchStats_t)

	SCHEMA_FIELD(int32_t, m_iEntryWins);
};

class CCSPlayerController_ActionTrackingServices
{
public:
	DECLARE_SCHEMA_CLASS(CCSPlayerController_ActionTrackingServices)

	SCHEMA_FIELD(CSMatchStats_t, m_matchStats)
};

class CPlayerPawnComponent
{
public:
	DECLARE_SCHEMA_CLASS(CPlayerPawnComponent);

	SCHEMA_FIELD(CCSPlayerPawn*, __m_pChainEntity)

	CCSPlayerPawn* GetPawn() { return __m_pChainEntity; }
};

class CPlayer_MovementServices : public CPlayerPawnComponent
{
public:
	DECLARE_SCHEMA_CLASS(CPlayer_MovementServices);

	SCHEMA_FIELD(CInButtonState, m_nButtons)
	SCHEMA_FIELD(uint64_t, m_nQueuedButtonDownMask)
	SCHEMA_FIELD(uint64_t, m_nQueuedButtonChangeMask)
	SCHEMA_FIELD(uint64_t, m_nButtonDoublePressed)

	// m_pButtonPressedCmdNumber[64]
	SCHEMA_FIELD_POINTER(uint32_t, m_pButtonPressedCmdNumber)
	SCHEMA_FIELD(uint32_t, m_nLastCommandNumberProcessed)
	SCHEMA_FIELD(uint64_t, m_nToggleButtonDownMask)
	SCHEMA_FIELD(float, m_flMaxspeed)
};

class CPlayer_MovementServices_Humanoid : public CPlayer_MovementServices
{
public:
	DECLARE_SCHEMA_CLASS(CPlayer_MovementServices_Humanoid);

	SCHEMA_FIELD(float, m_flFallVelocity)
	SCHEMA_FIELD(float, m_bInCrouch)
	SCHEMA_FIELD(uint32_t, m_nCrouchState)
	SCHEMA_FIELD(bool, m_bInDuckJump)
	SCHEMA_FIELD(float, m_flSurfaceFriction)
};

class CCSPlayer_MovementServices : public CPlayer_MovementServices_Humanoid
{
public:
	DECLARE_SCHEMA_CLASS(CCSPlayer_MovementServices);

	SCHEMA_FIELD(float, m_flMaxFallVelocity)
	SCHEMA_FIELD(float, m_flJumpVel)
	SCHEMA_FIELD(float, m_flStamina)
	SCHEMA_FIELD(float, m_flDuckSpeed)
	SCHEMA_FIELD(bool, m_bDuckOverride)
};

class CPlayer_WeaponServices : public CPlayerPawnComponent
{
public:
	DECLARE_SCHEMA_CLASS(CPlayer_WeaponServices);

	SCHEMA_FIELD_POINTER(CUtlVector<CHandle<CBasePlayerWeapon>>, m_hMyWeapons)
	SCHEMA_FIELD(CHandle<CBasePlayerWeapon>, m_hActiveWeapon)
	SCHEMA_FIELD_POINTER(uint16_t, m_iAmmo)
};

class CCSPlayer_WeaponServices : public CPlayer_WeaponServices
{
public:
	DECLARE_SCHEMA_CLASS(CCSPlayer_WeaponServices);

	SCHEMA_FIELD(GameTime_t, m_flNextAttack)
	SCHEMA_FIELD(bool, m_bIsLookingAtWeapon)
	SCHEMA_FIELD(bool, m_bIsHoldingLookAtWeapon)

	SCHEMA_FIELD(CHandle<CBasePlayerWeapon>, m_hSavedWeapon)
	SCHEMA_FIELD(int32_t, m_nTimeToMelee)
	SCHEMA_FIELD(int32_t, m_nTimeToSecondary)
	SCHEMA_FIELD(int32_t, m_nTimeToPrimary)
	SCHEMA_FIELD(int32_t, m_nTimeToSniperRifle)
	SCHEMA_FIELD(bool, m_bIsBeingGivenItem)
	SCHEMA_FIELD(bool, m_bIsPickingUpItemWithUse)
	SCHEMA_FIELD(bool, m_bPickedUpWeapon)

	void DropWeapon(CBasePlayerWeapon* pWeapon, Vector* pVecTarget = nullptr, Vector* pVelocity = nullptr)
	{
		static int offset = g_GameConfig->GetOffset("CCSPlayer_WeaponServices::DropWeapon");
		CALL_VIRTUAL(void, offset, this, pWeapon, pVecTarget, pVelocity);
	}

	void SelectItem(CBasePlayerWeapon* pWeapon, int unk1 = 0)
	{
		static int offset = g_GameConfig->GetOffset("CCSPlayer_WeaponServices::SelectItem");
		CALL_VIRTUAL(void, offset, this, pWeapon, unk1);
	}
};

class CCSPlayerController_InGameMoneyServices
{
public:
	DECLARE_SCHEMA_CLASS(CCSPlayerController_InGameMoneyServices);

	SCHEMA_FIELD(int, m_iAccount)
};

struct WeaponInfo_t
{
	int32_t m_iItemDefinitionIndex;
	int32_t m_iTeamNum;
	uint32_t m_eSlot;

	WeaponInfo_t(int32_t index, int32_t team, uint32_t slot) :
		m_iItemDefinitionIndex(index), m_iTeamNum(team), m_eSlot(slot) {}
};
static std::unordered_map<std::string, WeaponInfo_t> s_WeaponMap = {
	{"weapon_deagle",				  {1, 0, GEAR_SLOT_PISTOL}	  },
	{"weapon_elite",				 {2, 0, GEAR_SLOT_PISTOL}	 },
	{"weapon_fiveseven",			 {3, 3, GEAR_SLOT_PISTOL}	 },
	{"weapon_glock",				 {4, 2, GEAR_SLOT_PISTOL}	 },
	{"weapon_ak47",					{7, 2, GEAR_SLOT_RIFLE}	   },
	{"weapon_aug",				   {8, 3, GEAR_SLOT_RIFLE}	  },
	{"weapon_awp",				   {9, 0, GEAR_SLOT_RIFLE}	  },
	{"weapon_famas",				 {10, 3, GEAR_SLOT_RIFLE}	 },
	{"weapon_g3sg1",				 {11, 2, GEAR_SLOT_RIFLE}	 },
	{"weapon_galilar",			   {13, 2, GEAR_SLOT_RIFLE}   },
	{"weapon_m249",					{14, 0, GEAR_SLOT_RIFLE}	},
	{"weapon_m4a1",					{16, 3, GEAR_SLOT_RIFLE}	},
	{"weapon_mac10",				 {17, 2, GEAR_SLOT_RIFLE}	 },
	{"weapon_p90",				   {19, 0, GEAR_SLOT_RIFLE}   },
	{"weapon_mp5sd",				 {23, 0, GEAR_SLOT_RIFLE}	 },
	{"weapon_ump45",				 {24, 0, GEAR_SLOT_RIFLE}	 },
	{"weapon_xm1014",				  {25, 0, GEAR_SLOT_RIFLE}	  },
	{"weapon_bizon",				 {26, 0, GEAR_SLOT_RIFLE}	 },
	{"weapon_mag7",					{27, 3, GEAR_SLOT_RIFLE}	},
	{"weapon_negev",				 {28, 0, GEAR_SLOT_RIFLE}	 },
	{"weapon_sawedoff",				{29, 2, GEAR_SLOT_RIFLE}	},
	{"weapon_tec9",					{30, 2, GEAR_SLOT_RIFLE}	},
	{"weapon_taser",				 {31, 0, GEAR_SLOT_KNIFE}	 },
	{"weapon_hkp2000",			   {32, 3, GEAR_SLOT_PISTOL}	},
	{"weapon_mp7",				   {33, 0, GEAR_SLOT_RIFLE}   },
	{"weapon_mp9",				   {34, 0, GEAR_SLOT_RIFLE}   },
	{"weapon_nova",					{35, 0, GEAR_SLOT_RIFLE}	},
	{"weapon_p250",					{36, 0, GEAR_SLOT_PISTOL}	 },
	{"weapon_scar20",				  {38, 3, GEAR_SLOT_RIFLE}	  },
	{"weapon_sg556",				 {39, 2, GEAR_SLOT_RIFLE}	 },
	{"weapon_ssg08",				 {40, 0, GEAR_SLOT_RIFLE}	 },
	{"weapon_knifegg",			   {41, 0, GEAR_SLOT_KNIFE}   },
	{"weapon_knife",				 {42, 0, GEAR_SLOT_KNIFE}	 },
	{"weapon_flashbang",			 {43, 0, GEAR_SLOT_GRENADES}},
	{"weapon_hegrenade",			 {44, 0, GEAR_SLOT_GRENADES}},
	{"weapon_smokegrenade",			{45, 0, GEAR_SLOT_GRENADES}},
	{"weapon_molotov",			   {46, 0, GEAR_SLOT_GRENADES}},
	{"weapon_decoy",				 {47, 0, GEAR_SLOT_GRENADES}},
	{"weapon_incgrenade",			  {48, 0, GEAR_SLOT_GRENADES}},
	{"weapon_c4",					{49, 0, GEAR_SLOT_C4}	   },
	{"weapon_knife_t",			   {59, 0, GEAR_SLOT_KNIFE}   },
	{"weapon_m4a1_silencer",		 {60, 3, GEAR_SLOT_RIFLE}	 },
	{"weapon_usp_silencer",			{61, 3, GEAR_SLOT_PISTOL}	 },
	{"weapon_cz75a",				 {63, 0, GEAR_SLOT_PISTOL}  },
	{"weapon_revolver",				{64, 0, GEAR_SLOT_PISTOL}	 },
	{"weapon_bayonet",			   {500, 0, GEAR_SLOT_KNIFE}	},
	{"weapon_knife_css",			 {503, 0, GEAR_SLOT_KNIFE}  },
	{"weapon_knife_flip",			  {505, 0, GEAR_SLOT_KNIFE}  },
	{"weapon_knife_gut",			 {506, 0, GEAR_SLOT_KNIFE}  },
	{"weapon_knife_karambit",		  {507, 0, GEAR_SLOT_KNIFE}  },
	{"weapon_knife_m9_bayonet",		{508, 0, GEAR_SLOT_KNIFE}	 },
	{"weapon_knife_tactical",		  {509, 0, GEAR_SLOT_KNIFE}  },
	{"weapon_knife_falchion",		  {512, 0, GEAR_SLOT_KNIFE}  },
	{"weapon_knife_survival_bowie",	{514, 0, GEAR_SLOT_KNIFE}	 },
	{"weapon_knife_butterfly",	   {515, 0, GEAR_SLOT_KNIFE}	},
	{"weapon_knife_push",			  {516, 0, GEAR_SLOT_KNIFE}  },
	{"weapon_knife_cord",			  {517, 0, GEAR_SLOT_KNIFE}  },
	{"weapon_knife_canis",		   {518, 0, GEAR_SLOT_KNIFE}	},
	{"weapon_knife_ursus",		   {519, 0, GEAR_SLOT_KNIFE}	},
	{"weapon_knife_gypsy_jackknife", {520, 0, GEAR_SLOT_KNIFE}  },
	{"weapon_knife_outdoor",		 {521, 0, GEAR_SLOT_KNIFE}  },
	{"weapon_knife_stiletto",		  {522, 0, GEAR_SLOT_KNIFE}  },
	{"weapon_knife_widowmaker",		{523, 0, GEAR_SLOT_KNIFE}	 },
	{"weapon_knife_skeleton",		  {525, 0, GEAR_SLOT_KNIFE}  },
	{"weapon_knife_kukri",		   {526, 0, GEAR_SLOT_KNIFE}	},

	// Gears
	{"item_kevlar",					{0, 0, GEAR_SLOT_INVALID}	 },
	{"item_assaultsuit",			 {0, 0, GEAR_SLOT_INVALID}  },
	{"item_heavyassaultsuit",		  {0, 0, GEAR_SLOT_INVALID}  },
	{"item_defuser",				 {0, 0, GEAR_SLOT_INVALID}  },
	{"ammo_50ae",					{0, 0, GEAR_SLOT_INVALID}  },
};
static bool s_AwsChangingTeam;

class CCSPlayer_ItemServices : public CPlayerPawnComponent
{
public:
	DECLARE_SCHEMA_CLASS(CCSPlayer_ItemServices);

	virtual ~CCSPlayer_ItemServices() = 0;

private:
	virtual void unk_01() = 0;
	virtual void unk_02() = 0;
	virtual void unk_03() = 0;
	virtual void unk_04() = 0;
	virtual void unk_05() = 0;
	virtual void unk_06() = 0;
	virtual void unk_07() = 0;
	virtual void unk_08() = 0;
	virtual void unk_09() = 0;
	virtual void unk_10() = 0;
	virtual void unk_11() = 0;
	virtual void unk_12() = 0;
	virtual void unk_13() = 0;
	virtual void unk_14() = 0;
	virtual void unk_15() = 0;
	virtual void unk_16() = 0;
	virtual CBaseEntity* _GiveNamedItem(const char* pchName) = 0;

public:
	virtual bool GiveNamedItemBool(const char* pchName) = 0;
	virtual CBaseEntity* GiveNamedItem(const char* pchName) = 0;
	// Recommended to use CCSPlayer_WeaponServices::DropWeapon instead (parameter is ignored here)
	virtual void DropActiveWeapon(CBasePlayerWeapon* pWeapon) = 0;
	virtual void StripPlayerWeapons(bool removeSuit) = 0;

	[[nodiscard]] static bool IsAwsProcessing() noexcept { return s_AwsChangingTeam; }
	static void ResetAwsProcessing() { s_AwsChangingTeam = false; }

	[[nodiscard]] static uint32_t GetItemGearSlot(const char* item) noexcept
	{
		const auto it = s_WeaponMap.find(item);
		return it == s_WeaponMap.end() ? GEAR_SLOT_INVALID : it->second.m_eSlot;
	}

	CBaseEntity* GiveNamedItemAws(const char* item) noexcept
	{
		auto pPawn = reinterpret_cast<CBaseEntity*>(GetPawn());
		if (!pPawn || pPawn->m_iTeamNum() != CS_TEAM_CT) // only for CT
			return GiveNamedItem(item);

		const auto it = s_WeaponMap.find(item);
		if (it == s_WeaponMap.end())
			return GiveNamedItem(item);

		if (it->second.m_iTeamNum == 0
			|| !(it->second.m_eSlot == GEAR_SLOT_RIFLE || it->second.m_eSlot == GEAR_SLOT_PISTOL)
			|| it->second.m_iTeamNum == pPawn->m_iTeamNum())
			return GiveNamedItem(item);
			
		const auto team = pPawn->m_iTeamNum();
		s_AwsChangingTeam = true;
		pPawn->m_iTeamNum(it->second.m_iTeamNum);
		const auto pWeapon = GiveNamedItem(item);
		pPawn->m_iTeamNum(team);
		s_AwsChangingTeam = false;
		return pWeapon;
	}
};

// We need an exactly sized class to be able to iterate the vector, our schema system implementation can't do this
class WeaponPurchaseCount_t
{
private:
	virtual void unk01() {};
	uint64_t unk1 = 0;	// 0x8
	uint64_t unk2 = 0;	// 0x10
	uint64_t unk3 = 0;	// 0x18
	uint64_t unk4 = 0;	// 0x20
	uint64_t unk5 = -1; // 0x28
public:
	uint16_t m_nItemDefIndex; // 0x30
	uint16_t m_nCount;		  // 0x32
private:
	uint32_t unk6 = 0;
};

struct WeaponPurchaseTracker_t
{
public:
	DECLARE_SCHEMA_CLASS_INLINE(WeaponPurchaseTracker_t)

	SCHEMA_FIELD_POINTER(CUtlVector<WeaponPurchaseCount_t>, m_weaponPurchases)
};

class CCSPlayer_ActionTrackingServices
{
public:
	DECLARE_SCHEMA_CLASS(CCSPlayer_ActionTrackingServices)

	SCHEMA_FIELD(WeaponPurchaseTracker_t, m_weaponPurchasesThisRound)
};

class CPlayer_ObserverServices
{
public:
	DECLARE_SCHEMA_CLASS(CPlayer_ObserverServices)

	SCHEMA_FIELD(ObserverMode_t, m_iObserverMode)
	SCHEMA_FIELD(CHandle<CBaseEntity>, m_hObserverTarget)
	SCHEMA_FIELD(ObserverMode_t, m_iObserverLastMode)
	SCHEMA_FIELD(bool, m_bForcedObserverMode)
};

class CPlayer_CameraServices
{
public:
	DECLARE_SCHEMA_CLASS(CPlayer_CameraServices)

	SCHEMA_FIELD(CHandle<CBaseEntity>, m_hViewEntity)
};

class CCSPlayerBase_CameraServices : public CPlayer_CameraServices
{
public:
	DECLARE_SCHEMA_CLASS(CCSPlayerBase_CameraServices)

	SCHEMA_FIELD(CHandle<CBaseEntity>, m_hZoomOwner)
	SCHEMA_FIELD(uint, m_iFOV)
};

class CCSPlayer_CameraServices : public CCSPlayerBase_CameraServices
{};

class CPlayer_ViewModelServices : public CPlayerPawnComponent
{
public:
	DECLARE_SCHEMA_CLASS(CPlayer_ViewModelServices)
};

class CCSPlayer_ViewModelServices : public CPlayer_ViewModelServices
{
public:
	DECLARE_SCHEMA_CLASS(CCSPlayer_ViewModelServices)
	SCHEMA_FIELD_POINTER(CHandle<CBaseViewModel>, m_hViewModel) // CHandle<CBaseViewModel> m_hViewModel[3]

	CBaseViewModel* GetViewModel(int iIndex = 0)
	{
		return m_hViewModel()[iIndex].Get();
	}

	void SetViewModel(int iIndex, CBaseViewModel* pViewModel)
	{
		m_hViewModel()[iIndex].Set(pViewModel);
		pViewModel->m_nViewModelIndex = iIndex;
	}
};
