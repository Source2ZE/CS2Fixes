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

#include "networkbasetypes.pb.h"
#include "usercmd.pb.h"
#include "cs_usercmd.pb.h"

#include "cdetour.h"
#include "common.h"
#include "module.h"
#include "addresses.h"
#include "detours.h"
#include "irecipientfilter.h"
#include "entity/ccsplayercontroller.h"
#include "entity/ccsplayerpawn.h"
#include "entity/cbasemodelentity.h"
#include "entity/ccsweaponbase.h"
#include "entity/ctriggerpush.h"
#include "entity/cgamerules.h"
#include "entity/ctakedamageinfo.h"
#include "entity/services.h"
#include "playermanager.h"
#include "igameevents.h"
#include "gameconfig.h"
#include "serversideclient.h"
#include "networksystem/inetworkserializer.h"

#define VPROF_ENABLED
#include "tier0/vprof.h"

#include "tier0/memdbgon.h"

extern CGlobalVars *gpGlobals;
extern CGameEntitySystem *g_pEntitySystem;
extern IGameEventManager2 *g_gameEventManager;
extern CCSGameRules *g_pGameRules;

CUtlVector<CDetourBase *> g_vecDetours;

DECLARE_DETOUR(ProcessMovement, Detour_ProcessMovement);
DECLARE_DETOUR(TryPlayerMove, Detour_TryPlayerMove);
DECLARE_DETOUR(CategorizePosition, Detour_CategorizePosition);


void FASTCALL Detour_ProcessMovement(CCSPlayer_MovementServices *pThis, void *pMove)
{
	CCSPlayerPawn *pPawn = pThis->GetPawn();

	ZEPlayer *player = g_playerManager->GetPlayer(pPawn->m_hController()->GetPlayerSlot());
	if(!player) return ProcessMovement(pThis, pMove);
	
	player->currentMoveData = static_cast<CMoveData*>(pMove);
	player->didTPM = false;
	player->processingMovement = true;

	// Yes, this is what source1 does to scale player speed
	// Scale frametime during the entire movement processing step and revert right after
	float flStoreFrametime = gpGlobals->frametime;


	ProcessMovement(pThis, pMove);
	
	if(!player->didTPM)
		player->lastValidPlane = vec3_origin;

	gpGlobals->frametime = flStoreFrametime;
	
	player->processingMovement = false;
}

#define f32 float32
#define i32 int32_t
#define u32 uint32_t

void ClipVelocity(Vector &in, Vector &normal, Vector &out)
{
	// Determine how far along plane to slide based on incoming direction.
	f32 backoff = DotProduct(in, normal);

	for (i32 i = 0; i < 3; i++)
	{
		f32 change = normal[i] * backoff;
		out[i] = in[i] - change;
	}
	float adjust = DotProduct(out, normal);
	if (adjust < 0.0f)
	{
		adjust = MIN(adjust, -1 / 128);
		out -= (normal * adjust);
	}
}

bool IsValidMovementTrace(trace_t_s2 &tr, bbox_t bounds, CTraceFilterPlayerMovementCS *filter)
{
	trace_t_s2 stuck;
	// Maybe we don't need this one.
	// if (tr.fraction < FLT_EPSILON)
	//{
	//	return false;
	//}

	if (tr.startsolid)
	{
		return false;
	}

	// We hit something but no valid plane data?
	if (tr.fraction < 1.0f && fabs(tr.planeNormal.x) < FLT_EPSILON && fabs(tr.planeNormal.y) < FLT_EPSILON && fabs(tr.planeNormal.z) < FLT_EPSILON)
	{
		return false;
	}

	// Is the plane deformed?
	if (fabs(tr.planeNormal.x) > 1.0f || fabs(tr.planeNormal.y) > 1.0f || fabs(tr.planeNormal.z) > 1.0f)
	{
		return false;
	}

	// Do an unswept trace and a backward trace just to be sure.
	addresses::TracePlayerBBox(tr.endpos, tr.endpos, bounds, filter, stuck);
	if (stuck.startsolid || stuck.fraction < 1.0f - FLT_EPSILON)
	{
		return false;
	}

	addresses::TracePlayerBBox(tr.endpos, tr.startpos, bounds, filter, stuck);
	// For whatever reason if you can hit something in only one direction and not the other way around.
	// Only happens since Call to Arms update, so this fraction check is commented out until it is fixed.
	if (stuck.startsolid /*|| stuck.fraction < 1.0f - FLT_EPSILON*/)
	{
		return false;
	}

	return true;
}


#define MAX_BUMPS 4
#define RAMP_PIERCE_DISTANCE 1.0f
#define RAMP_BUG_THRESHOLD 0.99f
#define NEW_RAMP_THRESHOLD 0.95f
void TryPlayerMovePre(CCSPlayer_MovementServices *ms, Vector *pFirstDest, trace_t_s2 *pFirstTrace)
{
	CCSPlayerPawn *pawn = ms->GetPawn();
	ZEPlayer *player = g_playerManager->GetPlayer(pawn->m_hController()->GetPlayerSlot());
	player->overrideTPM = false;
	player->didTPM = true;

	f32 timeLeft = gpGlobals->frametime;

	Vector start, velocity, end;
	player->GetOrigin(&start);
	player->GetVelocity(&velocity);

	if (velocity.Length() == 0.0f)
	{
		// No move required.
		return;
	}
	Vector primalVelocity = velocity;
	bool validPlane {};

	f32 allFraction {};
	trace_t_s2 pm;
	u32 bumpCount {};
	Vector planes[5];
	u32 numPlanes {};
	trace_t_s2 pierce;

	bbox_t bounds;
	bounds.mins = {-16, -16, 0};
	bounds.maxs = {16, 16, 72};

	if (ms->m_bDucked())
	{
		bounds.maxs.z = 54;
	}

	CTraceFilterPlayerMovementCS filter;
	addresses::InitPlayerMovementTraceFilter(filter, pawn, pawn->m_Collision().m_collisionAttribute().m_nInteractsWith(),
											  COLLISION_GROUP_PLAYER_MOVEMENT);
	for (bumpCount = 0; bumpCount < MAX_BUMPS; bumpCount++)
	{
		// Assume we can move all the way from the current origin to the end point.
		VectorMA(start, timeLeft, velocity, end);
		// See if we can make it from origin to end point.
		// If their velocity Z is 0, then we can avoid an extra trace here during WalkMove.
		if (pFirstDest && end == *pFirstDest)
		{
			pm = *pFirstTrace;
		}
		else
		{
			addresses::TracePlayerBBox(start, end, bounds, &filter, pm);
			if (end == start)
			{
				continue;
			}
			if (IsValidMovementTrace(pm, bounds, &filter) && pm.fraction == 1.0f)
			{
				// Player won't hit anything, nothing to do.
				break;
			}
			if (player->lastValidPlane.Length() > FLT_EPSILON
				&& (!IsValidMovementTrace(pm, bounds, &filter) || pm.planeNormal.Dot(player->lastValidPlane) < RAMP_BUG_THRESHOLD))
			{
				// We hit a plane that will significantly change our velocity. Make sure that this plane is significant
				// enough.
				Vector direction = velocity.Normalized();
				Vector offsetDirection;
				f32 offsets[] = {0.0f, -1.0f, 1.0f};
				bool success {};
				for (u32 i = 0; i < 3 && !success; i++)
				{
					for (u32 j = 0; j < 3 && !success; j++)
					{
						for (u32 k = 0; k < 3 && !success; k++)
						{
							if (i == 0 && j == 0 && k == 0)
							{
								offsetDirection = player->lastValidPlane;
							}
							else
							{
								offsetDirection = {offsets[i], offsets[j], offsets[k]};
								// Check if this random offset is even valid.
								if (player->lastValidPlane.Dot(offsetDirection) <= 0.0f)
								{
									continue;
								}
								trace_t_s2 test;
								addresses::TracePlayerBBox(start + offsetDirection * RAMP_PIERCE_DISTANCE, start, bounds, &filter, test);
								if (!IsValidMovementTrace(test, bounds, &filter))
								{
									continue;
								}
							}
							bool goodTrace {};
							f32 ratio {};
							bool hitNewPlane {};
							for (ratio = 0.025f; ratio <= 1.0f; ratio += 0.025f)
							{
								addresses::TracePlayerBBox(start + offsetDirection * RAMP_PIERCE_DISTANCE * ratio,
															end + offsetDirection * RAMP_PIERCE_DISTANCE * ratio, bounds, &filter, pierce);
								if (!IsValidMovementTrace(pierce, bounds, &filter))
								{
									continue;
								}
								// Try until we hit a similar plane.
								// clang-format off
								validPlane = pierce.fraction < 1.0f && pierce.fraction > 0.1f 
											 && pierce.planeNormal.Dot(player->lastValidPlane) >= RAMP_BUG_THRESHOLD;

								hitNewPlane = pm.planeNormal.Dot(pierce.planeNormal) < NEW_RAMP_THRESHOLD 
											  && player->lastValidPlane.Dot(pierce.planeNormal) > NEW_RAMP_THRESHOLD;
								// clang-format on
								goodTrace = CloseEnough(pierce.fraction, 1.0f, FLT_EPSILON) || validPlane;
								if (goodTrace)
								{
									break;
								}
							}
							if (goodTrace || hitNewPlane)
							{
								// Trace back to the original end point to find its normal.
								trace_t_s2 test;
								addresses::TracePlayerBBox(pierce.endpos, end, bounds, &filter, test);
								pm = pierce;
								pm.startpos = start;
								pm.fraction = Clamp((pierce.endpos - pierce.startpos).Length() / (end - start).Length(), 0.0f, 1.0f);
								pm.endpos = test.endpos;
								if (pierce.planeNormal.Length() > 0.0f)
								{
									pm.planeNormal = pierce.planeNormal;
									player->lastValidPlane = pierce.planeNormal;
								}
								else
								{
									pm.planeNormal = test.planeNormal;
									player->lastValidPlane = test.planeNormal;
								}
								success = true;
								player->overrideTPM = true;
							}
						}
					}
				}
			}
			if (pm.planeNormal.Length() > 0.99f)
			{
				player->lastValidPlane = pm.planeNormal;
			}
		}

		if (pm.fraction * velocity.Length() > 0.03125f)
		{
			allFraction += pm.fraction;
			start = pm.endpos;
			numPlanes = 0;
		}
		if (allFraction == 1.0f)
		{
			break;
		}
		timeLeft -= gpGlobals->frametime * pm.fraction;
		planes[numPlanes] = pm.planeNormal;
		numPlanes++;
		if (numPlanes == 1 && pawn->m_MoveType() == MOVETYPE_WALK && pawn->m_hGroundEntity().Get() == nullptr)
		{
			ClipVelocity(velocity, planes[0], velocity);
		}
		else
		{
			u32 i, j;
			for (i = 0; i < numPlanes; i++)
			{
				ClipVelocity(velocity, planes[i], velocity);
				for (j = 0; j < numPlanes; j++)
				{
					if (j != i)
					{
						// Are we now moving against this plane?
						if (velocity.Dot(planes[j]) < 0)
						{
							break; // not ok
						}
					}
				}

				if (j == numPlanes) // Didn't have to clip, so we're ok
				{
					break;
				}
			}
			// Did we go all the way through plane set
			if (i != numPlanes)
			{ // go along this plane
				// pmove.velocity is set in clipping call, no need to set again.
				;
			}
			else
			{ // go along the crease
				if (numPlanes != 2)
				{
					VectorCopy(vec3_origin, velocity);
					break;
				}
				Vector dir;
				f32 d;
				CrossProduct(planes[0], planes[1], dir);
				dir.NormalizeInPlace();
				d = dir.Dot(velocity);
				VectorScale(dir, d, velocity);

				if (velocity.Dot(primalVelocity) <= 0)
				{
					velocity = vec3_origin;
					break;
				}
			}
		}
	}
	player->tpmOrigin = pm.endpos;
	player->tpmVelocity = velocity;
}

void TryPlayerMovePost(CCSPlayer_MovementServices *ms, Vector *pFirstDest, trace_t_s2 *pFirstTrace)
{
	ZEPlayer *player = g_playerManager->GetPlayer(ms->GetPawn()->m_hController()->GetPlayerSlot());
	if(!player)
		return;
	if (player->overrideTPM)
	{
		player->SetOrigin(player->tpmOrigin);
		player->SetVelocity(player->tpmVelocity);
	}
}

void FASTCALL Detour_TryPlayerMove(CCSPlayer_MovementServices *ms, CMoveData *mv, Vector *pFirstDest, trace_t_s2 *pFirstTrace)
{
	TryPlayerMovePre(ms, pFirstDest, pFirstTrace);
	TryPlayerMove(ms, mv, pFirstDest, pFirstTrace);
	TryPlayerMovePost(ms, pFirstDest, pFirstTrace);
}

void CategorizePositionPre(CCSPlayer_MovementServices *ms,bool bStayOnGround)
{
	CCSPlayerPawn *pawn = ms->GetPawn();
	ZEPlayer *player = g_playerManager->GetPlayer(pawn->m_hController()->GetPlayerSlot());
	// Already on the ground?
	// If we are already colliding on a standable valid plane, we don't want to do the check.
	if (bStayOnGround || player->lastValidPlane.Length() < 0.000001f|| player->lastValidPlane.z > 0.7f)
	{
		return;
	}
	Vector velocity;
	player->GetVelocity(&velocity);
	//Only attempt to fix rampbugs while going down significantly enough.
	if (velocity.z > -64.0f)
	{
		return;
	}
	bbox_t bounds;
	bounds.mins = {-16, -16, 0};
	bounds.maxs = {16, 16, 72};
                  
	if (ms->m_bDucked)
	{
		bounds.maxs.z = 54;
	}

	CTraceFilterPlayerMovementCS filter;
	addresses::InitPlayerMovementTraceFilter(filter, pawn,
											  pawn->m_Collision().m_collisionAttribute().m_nInteractsWith(),
											  COLLISION_GROUP_PLAYER_MOVEMENT);

	trace_t_s2 trace;
	addresses::InitGameTrace(&trace);

	Vector origin, groundOrigin;
	player->GetOrigin(&origin);
	groundOrigin = origin;
	groundOrigin.z -= 2.0f;

	addresses::TracePlayerBBox(origin, groundOrigin, bounds, &filter, trace);

	if (trace.fraction == 1.0f)
	{
		return;
	}
	// Is this something that you should be able to actually stand on?
	if (trace.fraction < 0.95f && trace.planeNormal.z > 0.7f && player->lastValidPlane.Dot(trace.planeNormal) < RAMP_BUG_THRESHOLD)
	{
		origin += player->lastValidPlane * 0.0625f;
		groundOrigin = origin;
		groundOrigin.z -= 2.0f;
		addresses::TracePlayerBBox(origin, groundOrigin, bounds, &filter, trace);
		if (trace.startsolid)
		{
			return;
		}
		if (trace.fraction == 1.0f || player->lastValidPlane.Dot(trace.planeNormal) >= RAMP_BUG_THRESHOLD)
		{
			player->SetOrigin(origin);
		}
	}
}

void FASTCALL Detour_CategorizePosition(CCSPlayer_MovementServices *ms, CMoveData *mv, bool bStayOnGround)
{
	CategorizePositionPre(ms, bStayOnGround);
	CategorizePosition(ms, mv, bStayOnGround);
}

bool InitDetours(CGameConfig *gameConfig)
{
	bool success = true;

	FOR_EACH_VEC(g_vecDetours, i)
	{
		if (!g_vecDetours[i]->CreateDetour(gameConfig))
			success = false;
		
		g_vecDetours[i]->EnableDetour();
	}

	return success;
}

void FlushAllDetours()
{
	g_vecDetours.Purge();
}
