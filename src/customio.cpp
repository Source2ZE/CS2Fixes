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

#include "customio.h"

#include "entity.h"
#include "entity/cbaseentity.h"
#include "entity/ccsplayercontroller.h"
#include "entity/cenventitymaker.h"
#include "entity/cphysthruster.h"

#include "ctimer.h"

#include <entity/cbasetrigger.h>
#include <string>
#include <vector>

extern CGlobalVars* gpGlobals;

struct AddOutputKey_t
{
	AddOutputKey_t(const char* pName, int32_t parts) :
		m_pName(pName)
	{
		m_nLength = strlen(pName);
		m_nParts = parts;
	}

	AddOutputKey_t(const AddOutputKey_t& other) :
		m_pName(other.m_pName),
		m_nLength(other.m_nLength),
		m_nParts(other.m_nParts) {}

	const char* m_pName;
	size_t m_nLength;
	int32_t m_nParts;
};

using AddOutputHandler_t = void (*)(CBaseEntity* pInstance,
									CEntityInstance* pActivator,
									CEntityInstance* pCaller,
									const std::vector<std::string>& vecArgs);

struct AddOutputInfo_t
{
	AddOutputInfo_t(const AddOutputKey_t& key, const AddOutputHandler_t& handler) :
		m_Key(key), m_Handler(handler) {}

	AddOutputKey_t m_Key;
	AddOutputHandler_t m_Handler;
};

static void AddOutputCustom_Targetname(CBaseEntity* pInstance, CEntityInstance* pActivator,
									   CEntityInstance* pCaller,
									   const std::vector<std::string>& vecArgs)
{
	pInstance->SetName(vecArgs[1].c_str());

#ifdef _DEBUG
	Message("SetName %s to %d", vecArgs[1].c_str(), pInstance->GetHandle().GetEntryIndex());
#endif
}

static void AddOutputCustom_Origin(CBaseEntity* pInstance,
								   CEntityInstance* pActivator,
								   CEntityInstance* pCaller,
								   const std::vector<std::string>& vecArgs)
{
	Vector origin(clamp(Q_atof(vecArgs[1].c_str()), -16384.f, 16384.f),
				  clamp(Q_atof(vecArgs[2].c_str()), -16384.f, 16384.f),
				  clamp(Q_atof(vecArgs[3].c_str()), -16384.f, 16384.f));
	pInstance->Teleport(&origin, nullptr, nullptr);

#ifdef _DEBUG
	Message("SetOrigin %f %f %f for %s", origin.x, origin.y, origin.z, pInstance->GetName());
#endif
}

static void AddOutputCustom_Angles(CBaseEntity* pInstance,
								   CEntityInstance* pActivator,
								   CEntityInstance* pCaller,
								   const std::vector<std::string>& vecArgs)
{
	QAngle angles(clamp(Q_atof(vecArgs[1].c_str()), -360.f, 360.f),
				  clamp(Q_atof(vecArgs[2].c_str()), -360.f, 360.f),
				  clamp(Q_atof(vecArgs[3].c_str()), -360.f, 360.f));
	pInstance->Teleport(nullptr, &angles, nullptr);

#ifdef _DEBUG
	Message("SetAngles %f %f %f for %s", angles.x, angles.y, angles.z, pInstance->GetName());
#endif
}

static void AddOutputCustom_MaxHealth(CBaseEntity* pInstance,
									  CEntityInstance* pActivator,
									  CEntityInstance* pCaller,
									  const std::vector<std::string>& vecArgs)
{
	pInstance->m_iMaxHealth(clamp(Q_atoi(vecArgs[1].c_str()), 0, INT_MAX));

#ifdef _DEBUG
	const int m_iMaxHealth = pInstance->m_iMaxHealth;
	Message("SetMaxHealth %d for %s", m_iMaxHealth, pInstance->GetName());
#endif
}

static void AddOutputCustom_Health(CBaseEntity* pInstance,
								   CEntityInstance* pActivator,
								   CEntityInstance* pCaller,
								   const std::vector<std::string>& vecArgs)
{
	pInstance->m_iHealth(clamp(Q_atoi(vecArgs[1].c_str()), 0, INT_MAX));

#ifdef _DEBUG
	const int m_iHealth = pInstance->m_iHealth;
	Message("SetHealth %d for %s", m_iHealth, pInstance->GetName());
#endif
}

static void AddOutputCustom_MoveType(CBaseEntity* pInstance,
									 CEntityInstance* pActivator,
									 CEntityInstance* pCaller,
									 const std::vector<std::string>& vecArgs)
{
	static Vector stopVelocity(0, 0, 0);
	const auto value = clamp(Q_atoi(vecArgs[1].c_str()), MOVETYPE_NONE, MOVETYPE_LAST);
	const auto type = static_cast<MoveType_t>(value);

	pInstance->SetMoveType(type);

#ifdef _DEBUG
	Message("SetMoveType %d for %s", type, pInstance->GetName());
#endif
}

static void AddOutputCustom_EntityTemplate(CBaseEntity* pInstance,
										   CEntityInstance* pActivator,
										   CEntityInstance* pCaller,
										   const std::vector<std::string>& vecArgs)
{
	if (strcmp(pInstance->GetClassname(), "env_entity_maker") == 0)
	{
		const auto pEntity = reinterpret_cast<CEnvEntityMaker*>(pInstance);
		const auto pValue = g_pEntitySystem->AllocPooledString(vecArgs[1].c_str());
		pEntity->m_iszTemplate(pValue);

#ifdef _DEBUG
		Message("Set EntityTemplate to %s for %s\n", pValue.String(), pInstance->GetName());
#endif
	}
	else
		Message("Only env_entity_maker is supported\n");
}

static void AddOutputCustom_BaseVelocity(CBaseEntity* pInstance,
										 CEntityInstance* pActivator,
										 CEntityInstance* pCaller,
										 const std::vector<std::string>& vecArgs)
{
	const Vector velocity(clamp(Q_atof(vecArgs[1].c_str()), -4096.f, 4096.f),
						  clamp(Q_atof(vecArgs[2].c_str()), -4096.f, 4096.f),
						  clamp(Q_atof(vecArgs[3].c_str()), -4096.f, 4096.f));

	pInstance->SetBaseVelocity(velocity);

#ifdef _DEBUG
	Message("SetOrigin %f %f %f for %s", velocity.x, velocity.y, velocity.z, pInstance->GetName());
#endif
}

static void AddOutputCustom_AbsVelocity(CBaseEntity* pInstance,
										CEntityInstance* pActivator,
										CEntityInstance* pCaller,
										const std::vector<std::string>& vecArgs)
{
	Vector velocity(clamp(Q_atof(vecArgs[1].c_str()), -4096.f, 4096.f),
					clamp(Q_atof(vecArgs[2].c_str()), -4096.f, 4096.f),
					clamp(Q_atof(vecArgs[3].c_str()), -4096.f, 4096.f));

	pInstance->Teleport(nullptr, nullptr, &velocity);

#ifdef _DEBUG
	Message("SetOrigin %f %f %f for %s", velocity.x, velocity.y, velocity.z, pInstance->GetName());
#endif
}

static void AddOutputCustom_Target(CBaseEntity* pInstance,
								   CEntityInstance* pActivator,
								   CEntityInstance* pCaller,
								   const std::vector<std::string>& vecArgs)
{
	if (const auto pTarget = UTIL_FindEntityByName(nullptr, vecArgs[1].c_str()))
	{
		const auto pEntity = pInstance;
		pEntity->m_target(pTarget->m_pEntity->m_name);

#ifdef _DEBUG
		Message("Set Target to %s for %s\n", pTarget->m_pEntity->m_name.String(), pEntity->m_pEntity->m_name.String());
#endif
	}
}

static void AddOutputCustom_FilterName(CBaseEntity* pInstance,
									   CEntityInstance* pActivator,
									   CEntityInstance* pCaller,
									   const std::vector<std::string>& vecArgs)
{
	if (V_strncasecmp(pInstance->GetClassname(), "trigger_", 8) == 0)
	{
		if (const auto pTarget = UTIL_FindEntityByName(nullptr, vecArgs[1].c_str()))
		{
			if (V_strncasecmp(pTarget->GetClassname(), "filter_", 7) == 0)
			{
				const auto pTrigger = reinterpret_cast<CBaseTrigger*>(pInstance);
				pTrigger->m_iFilterName(pTarget->GetName());
				pTrigger->m_hFilter(pTarget->GetRefEHandle());

#ifdef _DEBUG
				Message("Set FilterName to %s for %s\n", pTarget->GetName(),
						pTrigger->GetName());
#endif
			}
		}
	}
}

static void AddOutputCustom_Force(CBaseEntity* pInstance,
								  CEntityInstance* pActivator,
								  CEntityInstance* pCaller,
								  const std::vector<std::string>& vecArgs)
{
	const auto value = Q_atof(vecArgs[1].c_str());
	const auto pEntity = reinterpret_cast<CPhysThruster*>(pInstance);
	if (V_strcasecmp(pEntity->GetClassname(), "phys_thruster") == 0)
	{
		pEntity->m_force(value);

#ifdef _DEBUG
		Message("Set force to %f for %s\n", value, pEntity->GetName());
#endif
	}
}

static void AddOutputCustom_Gravity(CBaseEntity* pInstance,
									CEntityInstance* pActivator,
									CEntityInstance* pCaller,
									const std::vector<std::string>& vecArgs)
{
	const auto value = Q_atof(vecArgs[1].c_str());

	pInstance->m_flGravityScale = value;

#ifdef _DEBUG
	Message("Set gravity to %f for %s\n", value, pInstance->GetName());
#endif
}

static void AddOutputCustom_Timescale(CBaseEntity* pInstance,
									  CEntityInstance* pActivator,
									  CEntityInstance* pCaller,
									  const std::vector<std::string>& vecArgs)
{
	const auto value = Q_atof(vecArgs[1].c_str());

	pInstance->m_flTimeScale = value;

#ifdef _DEBUG
	Message("Set timescale to %f for %s\n", value, pInstance->GetName());
#endif
}

static void AddOutputCustom_Friction(CBaseEntity* pInstance,
									 CEntityInstance* pActivator,
									 CEntityInstance* pCaller,
									 const std::vector<std::string>& vecArgs)
{
	const auto value = Q_atof(vecArgs[1].c_str());

	pInstance->m_flFriction = value;

#ifdef _DEBUG
	Message("Set friction to %f for %s\n", value, pInstance->GetName());
#endif
}

static void AddOutputCustom_Speed(CBaseEntity* pInstance,
								  CEntityInstance* pActivator,
								  CEntityInstance* pCaller,
								  const std::vector<std::string>& vecArgs)
{
	if (!pInstance->IsPawn())
		return;

	const auto pPawn = reinterpret_cast<CCSPlayerPawn*>(pInstance);
	const auto pController = pPawn->GetOriginalController();

	if (!pController || !pController->IsConnected())
		return;

	const auto value = Q_atof(vecArgs[1].c_str());

	pController->GetZEPlayer()->SetSpeedMod(value);

#ifdef _DEBUG
	Message("Set speed to %f for %s\n", value, pInstance->GetName());
#endif
}

static void AddOutputCustom_RunSpeed(CBaseEntity* pInstance,
									 CEntityInstance* pActivator,
									 CEntityInstance* pCaller,
									 const std::vector<std::string>& vecArgs)
{
	if (!pInstance->IsPawn())
		return;

	const auto pPawn = reinterpret_cast<CCSPlayerPawn*>(pInstance);

	const auto value = Q_atof(vecArgs[1].c_str());

	pPawn->m_flVelocityModifier = value;

#ifdef _DEBUG
	Message("Set runspeed to %f for %s\n", value, pInstance->GetName());
#endif
}

const std::vector<AddOutputInfo_t> s_AddOutputHandlers = {
	{{"targetname", 2},		AddOutputCustom_Targetname	  },
	{{"origin", 4},			AddOutputCustom_Origin		  },
	{{"angles", 4},			AddOutputCustom_Angles		  },
	{{"max_health", 2},		AddOutputCustom_MaxHealth	 },
	{{"health", 2},			AddOutputCustom_Health		  },
	{{"movetype", 2},		  AddOutputCustom_MoveType	  },
	{{"EntityTemplate", 2}, AddOutputCustom_EntityTemplate},
	{{"basevelocity", 4},	  AddOutputCustom_BaseVelocity  },
	{{"absvelocity", 4},	 AddOutputCustom_AbsVelocity	},
	{{"target", 2},			AddOutputCustom_Target		  },
	{{"filtername", 2},		AddOutputCustom_FilterName	  },
	{{"force", 2},		   AddOutputCustom_Force			},
	{{"gravity", 2},		 AddOutputCustom_Gravity		},
	{{"timescale", 2},	   AddOutputCustom_Timescale		},
	{{"friction", 2},		  AddOutputCustom_Friction	  },
	{{"speed", 2},		   AddOutputCustom_Speed			},
	{{"runspeed", 2},		  AddOutputCustom_RunSpeed	  },
};

inline std::vector<std::string> StringSplit(const char* str, const char* delimiter)
{
	std::vector<std::string> result;
	std::string_view strV(str);
	size_t pos;

	while ((pos = strV.find(delimiter)) != std::string_view::npos)
	{
		result.emplace_back(strV.substr(0, pos));
		strV.remove_prefix(pos + std::string_view(delimiter).size());
	}

	result.emplace_back(strV);
	return result;
}

bool CustomIO_HandleInput(CEntityInstance* pInstance,
						  const char* param,
						  CEntityInstance* pActivator,
						  CEntityInstance* pCaller)
{
	for (auto& [input, handler] : s_AddOutputHandlers)
	{
		if (V_strncasecmp(param, input.m_pName, input.m_nLength) == 0)
		{
			if (const auto split = StringSplit(param, " ");
				split.size() == input.m_nParts)
			{
				handler(reinterpret_cast<CBaseEntity*>(pInstance), pActivator, pCaller, split);
				return true;
			}

			break;
		}
	}

	return false;
}

std::string g_sBurnParticle = "particles/burning_fx/burning_character_b.vpcf";
FAKE_STRING_CVAR(cs2f_burn_particle, "The particle to use for burning entities", g_sBurnParticle, false);

float g_flBurnDamage = 1.f;
FAKE_FLOAT_CVAR(cs2f_burn_damage, "The amount of each burn damage ticks", g_flBurnDamage, 1.f, false);

float g_flBurnSlowdown = 0.6f;
FAKE_FLOAT_CVAR(cs2f_burn_slowdown, "The slowdown of each burn damage tick as a multiplier of base speed", g_flBurnSlowdown, 0.6f, false);

float g_flBurnInterval = 0.3f;
FAKE_FLOAT_CVAR(cs2f_burn_interval, "The interval between burn damage ticks", g_flBurnInterval, 0.3f, false);

bool IgnitePawn(CCSPlayerPawn* pPawn, float flDuration, CBaseEntity* pInflictor, CBaseEntity* pAttacker, CBaseEntity* pAbility, DamageTypes_t nDamageType)
{
	auto pParticleEnt = reinterpret_cast<CParticleSystem*>(pPawn->m_hEffectEntity().Get());

	// This guy is already burning, don't ignite again
	if (pParticleEnt)
	{
		// Override the end time instead of just adding to it so players who get a ton of ignite inputs don't burn forever
		pParticleEnt->m_flDissolveStartTime = gpGlobals->curtime + flDuration;
		return true;
	}

	const auto vecOrigin = pPawn->GetAbsOrigin();

	pParticleEnt = CreateEntityByName<CParticleSystem>("info_particle_system");

	pParticleEnt->m_bStartActive(true);
	pParticleEnt->m_iszEffectName(g_sBurnParticle.c_str());
	pParticleEnt->m_hControlPointEnts[0] = pPawn;
	pParticleEnt->m_flDissolveStartTime = gpGlobals->curtime + flDuration; // Store the end time in the particle itself so we can increment if needed
	pParticleEnt->Teleport(&vecOrigin, nullptr, nullptr);

	pParticleEnt->DispatchSpawn();

	pParticleEnt->SetParent(pPawn);

	pPawn->m_hEffectEntity = pParticleEnt;

	CHandle<CCSPlayerPawn> hPawn(pPawn);
	CHandle<CBaseEntity> hInflictor(pInflictor);
	CHandle<CBaseEntity> hAttacker(pAttacker);
	CHandle<CBaseEntity> hAbility(pAbility);

	new CTimer(0.f, false, false, [hPawn, hInflictor, hAttacker, hAbility, nDamageType]() {
		CCSPlayerPawn* pPawn = hPawn.Get();

		if (!pPawn)
			return -1.f;

		const auto pParticleEnt = reinterpret_cast<CParticleSystem*>(pPawn->m_hEffectEntity().Get());

		if (!pParticleEnt)
			return -1.f;

		if (V_strncmp(pParticleEnt->GetClassname(), "info_part", 9) != 0)
		{
			// This should never happen but just in case
			Panic("Found unexpected entity %s while burning a pawn!\n", pParticleEnt->GetClassname());
			return -1.f;
		}

		if (pParticleEnt->m_flDissolveStartTime() <= gpGlobals->curtime || !pPawn->IsAlive())
		{
			pParticleEnt->AcceptInput("Stop");
			UTIL_AddEntityIOEvent(pParticleEnt, "Kill"); // Kill on the next frame

			return -1.f;
		}

		CTakeDamageInfo info(hInflictor, hAttacker, hAbility, g_flBurnDamage, nDamageType);

		// Damage doesn't apply if the inflictor is null
		if (!hInflictor.Get())
			info.m_hInflictor.Set(hAttacker);

		pPawn->TakeDamage(info);

		pPawn->m_flVelocityModifier = g_flBurnSlowdown;

		return g_flBurnInterval;
	});

	return true;
}
