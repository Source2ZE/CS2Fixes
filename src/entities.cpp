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

#include "entities.h"

#include "ctimer.h"
#include "entity.h"
#include "entity/cbaseplayercontroller.h"
#include "entity/ccsplayercontroller.h"
#include "entity/ccsplayerpawn.h"
#include "entity/cgameplayerequip.h"
#include "entity/cgamerules.h"
#include "entity/clogiccase.h"
#include "entity/cpointviewcontrol.h"

// #define ENTITY_HANDLER_ASSERTION

extern CCSGameRules* g_pGameRules;

class InputData_t
{
public:
	CBaseEntity* pActivator;
	CBaseEntity* pCaller;
	variant_t value;
	int nOutputID;
};

inline bool StripPlayer(CCSPlayerPawn* pPawn)
{
	const auto pItemServices = pPawn->m_pItemServices();

	if (!pItemServices)
		return false;

	pItemServices->StripPlayerWeapons(true);

	return true;
}

// Must be called in GameFramePre
inline void DelayInput(CBaseEntity* pCaller, const char* input, const char* param = "")
{
	const auto eh = pCaller->GetHandle();

	new CTimer(0.f, false, false, [eh, input, param]() {
		if (const auto entity = reinterpret_cast<CBaseEntity*>(eh.Get()))
			entity->AcceptInput(input, param, nullptr, entity);

		return -1.f;
	});
}

// Must be called in GameFramePre
inline void DelayInput(CBaseEntity* pCaller, CBaseEntity* pActivator, const char* input, const char* param = "")
{
	const auto eh = pCaller->GetHandle();
	const auto ph = pActivator->GetHandle();

	new CTimer(0.f, false, false, [eh, ph, input, param]() {
		const auto player = reinterpret_cast<CBaseEntity*>(ph.Get());
		if (const auto entity = reinterpret_cast<CBaseEntity*>(eh.Get()))
			entity->AcceptInput(input, param, player, entity);

		return -1.f;
	});
}

namespace CGamePlayerEquipHandler
{
	void Use(CGamePlayerEquip* pEntity, InputData_t* pInput)
	{
		const auto pCaller = pInput->pActivator;

		if (!pCaller || !pCaller->IsPawn())
			return;

		const auto pPawn = reinterpret_cast<CCSPlayerPawn*>(pCaller);

		const auto flags = pEntity->m_spawnflags();

		if (flags & CGamePlayerEquip::SF_PLAYEREQUIP_STRIPFIRST)
		{
			if (!StripPlayer(pPawn))
				return;
		}
		else if (flags & ::CGamePlayerEquip::SF_PLAYEREQUIP_ONLYSTRIPSAME)
		{
			// TODO support strip same flags
		}
	}

	void TriggerForAllPlayers(CGamePlayerEquip* pEntity, InputData_t* pInput)
	{
		const auto flags = pEntity->m_spawnflags();

		if (flags & CGamePlayerEquip::SF_PLAYEREQUIP_STRIPFIRST)
		{
			CCSPlayerPawn* pPawn = nullptr;
			while ((pPawn = reinterpret_cast<CCSPlayerPawn*>(UTIL_FindEntityByClassname(pPawn, "player"))) != nullptr)
				if (pPawn->IsPawn() && pPawn->IsAlive())
					StripPlayer(pPawn);
		}
		else if (flags & CGamePlayerEquip::SF_PLAYEREQUIP_ONLYSTRIPSAME)
		{
			// TODO support strip same flags
		}
	}

	bool TriggerForActivatedPlayer(CGamePlayerEquip* pEntity, InputData_t* pInput)
	{
		const auto pCaller = pInput->pActivator;
		const auto pszWeapon = ((pInput->value.m_type == FIELD_CSTRING || pInput->value.m_type == FIELD_STRING) && pInput->value.m_pszString) ? pInput->value.m_pszString : nullptr;

		if (!pCaller || !pCaller->IsPawn())
			return true;

		const auto pPawn = reinterpret_cast<CCSPlayerPawn*>(pCaller);
		const auto flags = pEntity->m_spawnflags();

		if (flags & CGamePlayerEquip::SF_PLAYEREQUIP_STRIPFIRST)
		{
			if (!StripPlayer(pPawn))
				return true;
		}
		else if (flags & CGamePlayerEquip::SF_PLAYEREQUIP_ONLYSTRIPSAME)
		{
			// TODO
		}

		const auto pItemServices = pPawn->m_pItemServices();

		if (!pItemServices)
			return true;

		if (pszWeapon && V_strcmp(pszWeapon, "(null)"))
		{
			pItemServices->GiveNamedItem(pszWeapon);
			// Don't execute game function (we fixed string param)
			return false;
		}

		return true;
	}
} // namespace CGamePlayerEquipHandler

namespace CGameUIHandler
{
	constexpr uint64 BAD_BUTTONS = ~0;

	struct CGameUIState
	{
		CHandle<CCSPlayerPawn> m_pPlayer;
		uint64 m_nButtonState;

		CGameUIState() :
			m_pPlayer(CBaseHandle()), m_nButtonState(BAD_BUTTONS) {}
		CGameUIState(const CGameUIState& other) = delete;

		CGameUIState(CCSPlayerPawn* pawn, uint64 buttons) :
			m_pPlayer(pawn), m_nButtonState(buttons) {}

		[[nodiscard]] CCSPlayerPawn* GetPlayer() const { return m_pPlayer.Get(); }

		void UpdateButtons(uint64 buttons) { m_nButtonState = buttons; }
	};

	static std::unordered_map<uint32, CGameUIState> s_repository;

	inline uint64 GetButtons(CPlayer_MovementServices* pMovement)
	{
		const auto buttonStates = pMovement->m_nButtons().m_pButtonStates();
		const auto buttons = buttonStates[0];
		return buttons;
	}

	inline uint64 GameUIThink(CGameUI* pEntity, CCSPlayerPawn* pPlayer, uint32 lastButtons)
	{
		const auto pMovement = pPlayer->m_pMovementServices();
		if (!pMovement)
			return BAD_BUTTONS;

		const auto spawnFlags = pEntity->m_spawnflags();
		const auto buttons = GetButtons(pMovement);

		if ((spawnFlags & CGameUI::SF_GAMEUI_JUMP_DEACTIVATE) != 0 && (buttons & IN_JUMP) != 0)
		{
			DelayInput(pEntity, pPlayer, "Deactivate");
			return BAD_BUTTONS;
		}

		const auto nButtonsChanged = buttons ^ lastButtons;

		// W
		if ((nButtonsChanged & IN_FORWARD) != 0)
			pEntity->AcceptInput("InValue", (lastButtons & IN_FORWARD) != 0 ? "UnpressedForward" : "PressedForward", pPlayer, pEntity);

		// A
		if ((nButtonsChanged & IN_MOVELEFT) != 0)
			pEntity->AcceptInput("InValue", (lastButtons & IN_MOVELEFT) != 0 ? "UnpressedMoveLeft" : "PressedMoveLeft", pPlayer, pEntity);

		// S
		if ((nButtonsChanged & IN_BACK) != 0)
			pEntity->AcceptInput("InValue", (lastButtons & IN_BACK) != 0 ? "UnpressedBack" : "PressedBack", pPlayer, pEntity);

		// D
		if ((nButtonsChanged & IN_MOVERIGHT) != 0)
			pEntity->AcceptInput("InValue", (lastButtons & IN_MOVERIGHT) != 0 ? "UnpressedMoveRight" : "PressedMoveRight", pPlayer, pEntity);

		// Attack
		if ((nButtonsChanged & IN_ATTACK) != 0)
			pEntity->AcceptInput("InValue", (lastButtons & IN_ATTACK) != 0 ? "UnpressedAttack" : "PressedAttack", pPlayer, pEntity);

		// Attack2
		if ((nButtonsChanged & IN_ATTACK2) != 0)
			pEntity->AcceptInput("InValue", (lastButtons & IN_ATTACK2) != 0 ? "UnpressedAttack2" : "PressedAttack2", pPlayer, pEntity);

		// Speed
		if ((nButtonsChanged & IN_SPEED) != 0)
			pEntity->AcceptInput("InValue", (lastButtons & IN_SPEED) != 0 ? "UnpressedSpeed" : "PressedSpeed", pPlayer, pEntity);

		// Duck
		if ((nButtonsChanged & IN_DUCK) != 0)
			pEntity->AcceptInput("InValue", (lastButtons & IN_DUCK) != 0 ? "UnpressedDuck" : "PressedDuck", pPlayer, pEntity);

		return buttons;
	}

	// [Kxnrl]: Must be called on game frame pre, and timer done in post!
	void RunThink(int tick)
	{
		// validate
		for (auto it = s_repository.begin(); it != s_repository.end();)
		{
			const auto entity = CHandle<CGameUI>(it->first).Get();
			if (!entity)
			{
				it = s_repository.erase(it);
#ifdef ENTITY_HANDLER_ASSERTION
				Message("Remove Entity %d due to invalid.\n", CBaseHandle(it->first).GetEntryIndex());
#endif
			}
			else
			{
				++it;
			}
		}

		// think every 4 tick
		if ((tick & 4) != 0)
			return;

		for (auto& [key, state] : s_repository)
		{
			const auto entity = CHandle<CGameUI>(key).Get();
			const auto player = state.GetPlayer();

			if (!player || !player->IsPawn())
			{
				DelayInput(entity, "Deactivate");
#ifdef ENTITY_HANDLER_ASSERTION
				Message("Deactivate Entity %d due to invalid player.\n", entity->entindex());
#endif
				continue;
			}

			if (!player->IsAlive())
			{
				DelayInput(entity, player, "Deactivate");
#ifdef ENTITY_HANDLER_ASSERTION
				Message("Deactivate Entity %d due to player dead.\n", entity->entindex());
#endif
				continue;
			}

			const auto newButtons = GameUIThink(entity, player, state.m_nButtonState);

			if (newButtons != BAD_BUTTONS)
				state.UpdateButtons(newButtons);
		}
	}

	bool OnActivate(CGameUI* pEntity, CBaseEntity* pActivator)
	{
		if (!pActivator || !pActivator->IsPawn())
			return false;

		const auto pPlayer = reinterpret_cast<CCSPlayerPawn*>(pActivator);

		const auto pMovement = pPlayer->m_pMovementServices();
		if (!pMovement)
			return false;

		if ((pEntity->m_spawnflags() & CGameUI::SF_GAMEUI_FREEZE_PLAYER) != 0)
			pPlayer->m_fFlags(pPlayer->m_fFlags() | FL_ATCONTROLS);

		const CBaseHandle handle = pEntity->GetHandle();
		const auto key = static_cast<uint>(handle.ToInt());

		DelayInput(pEntity, pPlayer, "InValue", "PlayerOn");

		s_repository[key] = CGameUIState(pPlayer, GetButtons(pMovement) & ~IN_USE);

#ifdef ENTITY_HANDLER_ASSERTION
		Message("Activate Entity %d<%u> -> %s\n", pEntity->entindex(), key, pPlayer->GetController()->GetPlayerName());
#endif

		return true;
	}

	bool OnDeactivate(CGameUI* pEntity, CBaseEntity* pActivator)
	{
		const CBaseHandle handle = CHandle(pEntity);
		const auto key = static_cast<uint>(handle.ToInt());
		const auto it = s_repository.find(key);

		if (it == s_repository.end())
		{
#ifdef ENTITY_HANDLER_ASSERTION
			Message("Deactivate Entity %d -> but does not exists <%u>\n", pEntity->entindex(), key);
#endif
			return false;
		}

		if (const auto pPlayer = it->second.GetPlayer())
		{
			if ((pEntity->m_spawnflags() & CGameUI::SF_GAMEUI_FREEZE_PLAYER) != 0)
				pPlayer->m_fFlags(pPlayer->m_fFlags() & ~FL_ATCONTROLS);

			DelayInput(pEntity, pPlayer, "InValue", "PlayerOff");

#ifdef ENTITY_HANDLER_ASSERTION
			Message("Deactivate Entity %d -> %s\n", pEntity->entindex(), pPlayer->GetController()->GetPlayerName());
#endif
		}
		else
		{
#ifdef ENTITY_HANDLER_ASSERTION
			Message("Deactivate Entity %d -> nullptr\n", pEntity->entindex());
#endif
		}

		s_repository.erase(it);

		return true;
	}

} // namespace CGameUIHandler

namespace CPointViewControlHandler
{
	struct ViewControl
	{
		CUtlVector<CHandle<CCSPlayerPawn>> m_players;
		std::string m_viewTarget;
		std::string m_name;
	};

	static std::unordered_map<uint32, ViewControl> s_repository;
	static constexpr uint INVALID_FOV = 0xFFFFFFFF;
	static constexpr uint RESET_FOV = 0xFFFFFFFE;
	static CHandle<CBaseEntity> INVALID_HANDLE(0xFFFFFFFF);

	inline void UpdatePlayerState(CCSPlayerPawn* pPawn, const CHandle<CBaseEntity>& target, bool frozen, uint fov = INVALID_FOV, bool disarm = false)
	{
		if (!pPawn)
			return;

		if (const auto pCameraService = pPawn->GetCameraService())
		{
			pCameraService->m_hViewEntity(target);
			pCameraService->m_hZoomOwner(INVALID_HANDLE);

			if (fov != INVALID_FOV)
			{
				if (const auto pController = pPawn->GetController())
				{
					if (fov == RESET_FOV)
						pCameraService->m_iFOV(pController->m_iDesiredFOV());
					else
						pCameraService->m_iFOV(fov);
				}
			}
		}

		if (disarm)
		{
			if (const auto pWeaponService = pPawn->m_pWeaponServices())
			{
				if (const auto pActiveWeapon = pWeaponService->m_hActiveWeapon().Get())
					pActiveWeapon->Disarm();
			}
		}

		auto flags = pPawn->m_fFlags();

		if (g_pGameRules && g_pGameRules->m_bFreezePeriod())
			frozen = true;

		if (frozen)
			flags |= FL_FROZEN;
		else
			flags &= ~FL_FROZEN;

		pPawn->m_fFlags(flags);
	}

	void OnCreated(CBaseEntity* pEntity)
	{
		const auto pViewControl = pEntity->AsPointViewControl();
		if (!pViewControl)
			return;

		if (!pViewControl->HasTargetCameraEntity())
		{
			Warning("PointViewControl %s has no target camera entity\n", pViewControl->GetName());
			return;
		}

		ViewControl vc{};
		vc.m_viewTarget = pViewControl->m_target().String();
		vc.m_name = pViewControl->GetName();
		s_repository[pEntity->GetHandle().ToInt()] = vc;
	}
	bool OnEnable(CPointViewControl* pEntity, CBaseEntity* pActivator)
	{
		if (!pActivator || !pActivator->IsPawn() || !pActivator->IsAlive())
			return false;

		const auto key = pEntity->GetHandle().ToInt();
		const auto it = s_repository.find(key);
		if (it == s_repository.end())
			return false;

		const auto pPawn = reinterpret_cast<CCSPlayerPawn*>(pActivator);
		const auto pController = reinterpret_cast<CCSPlayerController*>(pPawn->GetController());
		if (!pController)
			return false;

		if (pController->IsBot() || pController->m_bIsHLTV())
		{
			Warning("PointViewControl %s try enable for bot or HLTV: %s\n", it->second.m_name.c_str(), pController->GetPlayerName());
			return false;
		}

		const auto handle = CHandle<CCSPlayerPawn>(pPawn->GetHandle());

		for (auto& [vk, vc] : s_repository)
		{
			if (const auto index = vc.m_players.Find(handle); index > -1)
			{
				if (vk == static_cast<uint>(key))
				{
					Warning("PointViewControl %s was enabled twice in a row! player: %s\n", vc.m_name.c_str(), pController->GetPlayerName());
					return false;
				}

				vc.m_players.Remove(index);
				UpdatePlayerState(pPawn, INVALID_HANDLE, false, RESET_FOV);
				Warning("PointViewControl %s already enabled for %s\n", vc.m_name.c_str(), pController->GetPlayerName());
				break;
			}
		}

		return it->second.m_players.AddToTail(handle) >= 0;
	}
	bool OnDisable(CPointViewControl* pEntity, CBaseEntity* pActivator)
	{
		if (!pActivator || !pActivator->IsPawn() || !pActivator->IsAlive())
			return false;

		const auto key = pEntity->GetHandle().ToInt();
		const auto it = s_repository.find(key);
		if (it == s_repository.end())
			return false;

		const auto pPawn = reinterpret_cast<CCSPlayerPawn*>(pActivator);
		const auto pController = reinterpret_cast<CCSPlayerController*>(pPawn->GetController());
		if (!pController)
			return false;

		if (pController->IsBot() || pController->m_bIsHLTV())
		{
			Warning("PointViewControl %s try disable for bot or HLTV: %s\n", it->second.m_name.c_str(), pController->GetPlayerName());
			return false;
		}

		const auto handle = CHandle<CCSPlayerPawn>(pPawn->GetHandle());

		UpdatePlayerState(pPawn, INVALID_HANDLE, false, RESET_FOV);

		return it->second.m_players.FindAndRemove(handle);
	}
	bool OnEnableAll(CPointViewControl* pEntity)
	{
		const auto key = pEntity->GetHandle().ToInt();
		const auto it = s_repository.find(key);
		if (it == s_repository.end())
			return false;

		for (auto i = 0; i < gpGlobals->maxClients; i++)
		{
			const auto pController = CCSPlayerController::FromSlot(i);
			if (!pController || !pController->IsConnected() || pController->IsBot() || pController->m_bIsHLTV())
				continue;

			const auto pPawn = pController->GetPlayerPawn();
			if (!pPawn || !pPawn->IsAlive())
				continue;

			const auto handle = CHandle<CCSPlayerPawn>(pPawn->GetHandle());

			for (auto& [vk, vc] : s_repository)
			{
				if (const auto index = vc.m_players.Find(handle); index > -1)
				{
					vc.m_players.Remove(index);
					if (vk == static_cast<uint>(key))
						continue;
					UpdatePlayerState(pPawn, INVALID_HANDLE, false, RESET_FOV);
					Warning("PointViewControl %s already enabled for %s\n", vc.m_name.c_str(), pController->GetPlayerName());
				}
			}

			it->second.m_players.AddToTail(handle);
		}

		return true;
	}
	bool OnDisableAll(CPointViewControl* pEntity)
	{
		const auto key = pEntity->GetHandle().ToInt();
		const auto it = s_repository.find(key);
		if (it == s_repository.end())
			return false;

		FOR_EACH_VEC(it->second.m_players, i)
		{
			const auto& handle = it->second.m_players.Element(i);

			if (const auto player = handle.Get())
				UpdatePlayerState(player, INVALID_HANDLE, false, RESET_FOV);
		}

		it->second.m_players.Purge();

		return true;
	}

	void RunThink(int tick)
	{
		// validate
		for (auto it = s_repository.begin(); it != s_repository.end();)
		{
			const auto entity = CHandle<CPointViewControl>(it->first).Get();
			if (!entity)
			{
				FOR_EACH_VEC(it->second.m_players, i)
				{
					const auto& handle = it->second.m_players.Element(i);

					if (const auto player = handle.Get())
						UpdatePlayerState(player, INVALID_HANDLE, false, RESET_FOV);
				}

				it = s_repository.erase(it);
			}
			else
			{
				++it;
			}
		}

		// think every tick

		for (auto& [vk, vc] : s_repository)
		{
			const auto entity = CHandle<CPointViewControl>(vk).Get();
			if (!entity)
			{
				Error("Why invalid entity here?");
				continue;
			}

			if (vc.m_players.Count() == 0)
				continue;

			const auto pTarget = entity->GetTargetCameraEntity();
			if (!pTarget)
			{
				FOR_EACH_VEC(vc.m_players, i)
				{
					const auto& handle = vc.m_players.Element(i);

					if (const auto player = handle.Get())
						UpdatePlayerState(player, INVALID_HANDLE, false, RESET_FOV);
				}
				vc.m_players.Purge();
				continue;
			}

			FOR_EACH_VEC(vc.m_players, i)
			{
				const auto& handle = vc.m_players.Element(i);
				const auto player = handle.Get();
				if (!player)
				{
					vc.m_players.Remove(i--);
					continue;
				}
				if (!player->IsAlive())
				{
					UpdatePlayerState(player, INVALID_HANDLE, false, RESET_FOV);
					vc.m_players.Remove(i--);
					continue;
				}

				UpdatePlayerState(player, pTarget->GetHandle(), entity->HasFrozen(), entity->HasFOV() ? entity->GetFOV() : INVALID_FOV, entity->HasDisarm());
			}
		}
	}
	bool IsViewControl(CCSPlayerPawn* pPawn)
	{
		const auto handle = pPawn->GetHandle().ToInt();
		for (const auto& [vk, vc] : s_repository)
		{
			FOR_EACH_VEC(vc.m_players, i)
			{
				if (vc.m_players.Element(i).ToInt() == handle)
					return true;
			}
		}
		return false;
	}
	void Shutdown()
	{
		for (auto& [vk, vc] : s_repository)
		{
			FOR_EACH_VEC(vc.m_players, i)
			{
				const auto& handle = vc.m_players.Element(i);

				if (const auto player = handle.Get())
					UpdatePlayerState(player, INVALID_HANDLE, false, RESET_FOV);
			}
			vc.m_players.Purge();
		}
		s_repository.clear();
	}
} // namespace CPointViewControlHandler

void EntityHandler_OnGameFramePre(bool simulate, int tick)
{
	if (!simulate)
		return;

	CGameUIHandler::RunThink(tick);
	CPointViewControlHandler::RunThink(tick);
}

void EntityHandler_OnGameFramePost(bool simulate, int tick)
{
	if (!simulate)
		return;
}

void EntityHandler_OnRoundRestart()
{
	CPointViewControlHandler::Shutdown();
}

void EntityHandler_OnEntitySpawned(CBaseEntity* pEntity)
{
	CPointViewControlHandler::OnCreated(pEntity);
}