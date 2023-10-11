/**
 * =============================================================================
 * CS2Fixes
 * Copyright (C) 2023 Source2ZE
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

#include "detours.h"
#include "common.h"
#include "utlstring.h"
#include "recipientfilters.h"
#include "commands.h"
#include "utils/entity.h"
#include "entity/cbaseentity.h"
#include "entity/ccsweaponbase.h"
#include "entity/ccsplayercontroller.h"
#include "entity/ccsplayerpawn.h"
#include "entity/cbasemodelentity.h"
#include "playermanager.h"
#include "adminsystem.h"
#include "ctimer.h"

#include "tier0/memdbgon.h"


extern CEntitySystem *g_pEntitySystem;

void ParseChatCommand(const char *pMessage, CCSPlayerController *pController)
{
	if (!pController)
		return;

	CCommand args;
	args.Tokenize(pMessage + 1);

	uint16 index = g_CommandList.Find(hash_32_fnv1a_const(args[0]));

	if (g_CommandList.IsValidIndex(index))
	{
		g_CommandList[index](args, pController);
	}
	else
	{
		ParseWeaponCommand(pController, args[0]);
	}
}

void ClientPrintAll(int hud_dest, const char *msg, ...)
{
	va_list args;
	va_start(args, msg);

	char buf[256];
	V_vsnprintf(buf, sizeof(buf), msg, args);

	va_end(args);

	addresses::UTIL_ClientPrintAll(hud_dest, buf, nullptr, nullptr, nullptr, nullptr);
}

void ClientPrint(CBasePlayerController *player, int hud_dest, const char *msg, ...)
{
	va_list args;
	va_start(args, msg);

	char buf[256];
	V_vsnprintf(buf, sizeof(buf), msg, args);

	va_end(args);

	addresses::ClientPrint(player, hud_dest, buf, nullptr, nullptr, nullptr, nullptr);
}

CON_COMMAND_CHAT(say, "say something using console")
{
	ClientPrintAll(HUD_PRINTTALK, "%s", args.ArgS());
}

CON_COMMAND_CHAT(rs, "reset your score")
{
	if (!player)
		return;

	player->m_pActionTrackingServices->m_matchStats().m_iKills = 0;
	player->m_pActionTrackingServices->m_matchStats().m_iDeaths = 0;
	player->m_pActionTrackingServices->m_matchStats().m_iAssists = 0;
	player->m_pActionTrackingServices->m_matchStats().m_iDamage = 0;
	player->m_iScore = 0;
	player->m_iMVPs = 0;

	ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX"You successfully reset your score.");
}
