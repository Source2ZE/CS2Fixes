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

#include "commands.h"
#include "playermanager.h"
#include "adminsystem.h"
#include "ctimer.h"
#include "rockthevote.h"

#include "tier0/memdbgon.h"

extern CEntitySystem* g_pEntitySystem;
extern IVEngineServer2* g_pEngineServer2;
extern CGlobalVars* gpGlobals;

ERTVState g_RTVState = ERTVState::MAP_START;
EExtendState g_ExtendState = EExtendState::MAP_START;
int g_ExtendsLeft = 1;

//TODO POSSIBLE CONVARS
float g_RTVSucceedRatio = 0.5f;
float g_ExtendSucceedRatio = 0.7f;
int g_ExtendTimeToAdd = 20;

#define ADMIN_PREFIX "Admin %s has "

int GetCurrentRTVCount()
{
	int iVoteCount = 0;

	for (int i = 0; i < gpGlobals->maxClients; i++)
	{
		ZEPlayer* pPlayer = g_playerManager->GetPlayer(i);

		if (pPlayer && pPlayer->GetRTVVote())
			iVoteCount++;
	}

	return iVoteCount;
}

int GetNeededRTVCount()
{
	int iOnlinePlayers = 0.0f;
	int iVoteCount = 0;

	for (int i = 0; i < gpGlobals->maxClients; i++)
	{
		ZEPlayer* pPlayer = g_playerManager->GetPlayer(i);

		if (pPlayer)
		{
			iOnlinePlayers++;
			if (pPlayer->GetRTVVote())
				iVoteCount++;
		}
	}

	return (int)(iOnlinePlayers * g_RTVSucceedRatio) + 1;
}

int GetCurrentExtendCount()
{
	int iVoteCount = 0;

	for (int i = 0; i < gpGlobals->maxClients; i++)
	{
		ZEPlayer* pPlayer = g_playerManager->GetPlayer(i);

		if (pPlayer && pPlayer->GetExtendVote())
			iVoteCount++;
	}

	return iVoteCount;
}

int GetNeededExtendCount()
{
	int iOnlinePlayers = 0.0f;
	int iVoteCount = 0;

	for (int i = 0; i < gpGlobals->maxClients; i++)
	{
		ZEPlayer* pPlayer = g_playerManager->GetPlayer(i);

		if (pPlayer)
		{
			iOnlinePlayers++;
			if (pPlayer->GetExtendVote())
				iVoteCount++;
		}
	}

	return (int)(iOnlinePlayers * g_ExtendSucceedRatio) + 1;
}

CON_COMMAND_CHAT(rtv, "Vote to end the current map sooner.")
{
	if (!player)
	{
		ClientPrint(player, HUD_PRINTCONSOLE, CHAT_PREFIX "You cannot use this command from the server console.");
		return;
	}

	int iPlayer = player->GetPlayerSlot();

	ZEPlayer* pPlayer = g_playerManager->GetPlayer(iPlayer);

	// Something has to really go wrong for this to happen
	if (!pPlayer)
	{
		Warning("%s Tried to access a null ZEPlayer!!\n", player->GetPlayerName());
		return;
	}

	switch (g_RTVState)
	{
	case ERTVState::MAP_START:
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "The vote to RTV the map has not started yet.");
		return;
	case ERTVState::POST_RTV_SUCCESSFULL:
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "RTV has already been voted.");
		return;
	case ERTVState::POST_LAST_ROUND_END:
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "You can't vote to RTV the map during next map selection.");
		return;
	case ERTVState::BLOCKED_BY_ADMIN:
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "The vote to RTV has been blocked by an Admin.");
		return;
	}

	int iCurrentRTVCount = GetCurrentRTVCount();
	int iNeededRTVCount = GetNeededRTVCount();

	if (pPlayer->GetRTVVote())
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "You have already Rocked The Vote (%i voted, %i required).", iCurrentRTVCount, iNeededRTVCount);
		return;
	}

	if (iCurrentRTVCount + 1 >= iNeededRTVCount)
	{
		g_RTVState = ERTVState::POST_RTV_SUCCESSFULL;
		ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX "RTV vote succeeded! This is the last round of the map!");
		g_pEngineServer2->ServerCommand("mp_timelimit 1");

		for (int i = 0; i < gpGlobals->maxClients; i++)
		{
			ZEPlayer* pPlayer2 = g_playerManager->GetPlayer(iPlayer);
			if (pPlayer2)
				pPlayer2->SetRTVVote(false);
		}

		return;
	}

	pPlayer->SetRTVVote(true);
	ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX "%s wants to RTV current map (%i voted, %i required).", player->GetPlayerName(), iCurrentRTVCount+1, iNeededRTVCount);
}

CON_COMMAND_CHAT(unrtv, "Remove your vote to end the current map sooner.")
{
	if (!player)
	{
		ClientPrint(player, HUD_PRINTCONSOLE, CHAT_PREFIX "You cannot use this command from the server console.");
		return;
	}

	int iPlayer = player->GetPlayerSlot();

	ZEPlayer* pPlayer = g_playerManager->GetPlayer(iPlayer);

	// Something has to really go wrong for this to happen
	if (!pPlayer)
	{
		Warning("%s Tried to access a null ZEPlayer!!\n", player->GetPlayerName());
		return;
	}

	if (!pPlayer->GetRTVVote())
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "You have not voted to RTV current map.");
		return;
	}

	pPlayer->SetRTVVote(false);
	ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "You no longer want to RTV current map.");
}


CON_COMMAND_CHAT(ve, "Vote to extend the current map.")
{
	if (!player)
	{
		ClientPrint(player, HUD_PRINTCONSOLE, CHAT_PREFIX "You cannot use this command from the server console.");
		return;
	}

	int iPlayer = player->GetPlayerSlot();

	ZEPlayer* pPlayer = g_playerManager->GetPlayer(iPlayer);

	// Something has to really go wrong for this to happen
	if (!pPlayer)
	{
		Warning("%s Tried to access a null ZEPlayer!!\n", player->GetPlayerName());
		return;
	}

	switch (g_ExtendState)
	{
	case EExtendState::MAP_START:
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "The vote to extend the map has not started yet.");
		return;
	case EExtendState::POST_EXTEND_COOLDOWN:
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "You can't vote to extend the map yet.");
		return;
	case EExtendState::POST_EXTEND_NO_EXTENDS_LEFT:
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "There are no more extends left for the current map.");
		return;
	case EExtendState::POST_LAST_ROUND_END:
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "You can't vote to extend the map during next map selection.");
		return;
	case EExtendState::NO_EXTENDS:
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Extend vote is not allowed for the current map.");
		return;
	}

	int iCurrentExtendCount = GetCurrentExtendCount();
	int iNeededExtendCount = GetNeededExtendCount();

	if (pPlayer->GetExtendVote())
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "You have already voted to extend the map (%i voted, %i required).", iCurrentExtendCount, iNeededExtendCount);
		return;
	}

	if (iCurrentExtendCount + 1 >= iNeededExtendCount)
	{
		// Extend vote succeeded
		ConVar* cvar = g_pCVar->GetConVar(g_pCVar->FindConVar("mp_timelimit"));

		float pflTimelimit;
		// le funny type punning
		memcpy(&pflTimelimit, &cvar->values, sizeof(pflTimelimit));

		char buf[32];
		V_snprintf(buf, sizeof(buf), "mp_timelimit %.4f", pflTimelimit + g_ExtendTimeToAdd);

		g_pEngineServer2->ServerCommand(buf);

		if (g_ExtendsLeft == 1)
			// there are no extends left after a successfull extend vote
			g_ExtendState = EExtendState::POST_EXTEND_NO_EXTENDS_LEFT;
		else
		{
			// there's an extend left after a successfull extend vote
			g_ExtendState = EExtendState::POST_EXTEND_COOLDOWN;

			// Allow another extend vote after 2 minutes
			new CTimer(120.0f, false, []()
			{
				g_ExtendState = EExtendState::EXTEND_ALLOWED;
				return -1.0f;
			});
		}

		for (int i = 0; i < gpGlobals->maxClients; i++)
		{
			ZEPlayer* pPlayer2 = g_playerManager->GetPlayer(iPlayer);
			if (pPlayer2)
				pPlayer2->SetExtendVote(false);
		}

		g_ExtendsLeft--;
		ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX "Extend vote succeeded! Current map has been extended by %i minutes.", g_ExtendTimeToAdd);

		return;
	}

	pPlayer->SetExtendVote(true);
	ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX "%s wants to extend current map (%i voted, %i required).", player->GetPlayerName(), iCurrentExtendCount+1, iNeededExtendCount);
}

CON_COMMAND_CHAT(unve, "Remove your vote to extend current map.")
{
	if (!player)
	{
		ClientPrint(player, HUD_PRINTCONSOLE, CHAT_PREFIX "You cannot use this command from the server console.");
		return;
	}

	int iPlayer = player->GetPlayerSlot();

	ZEPlayer* pPlayer = g_playerManager->GetPlayer(iPlayer);

	// Something has to really go wrong for this to happen
	if (!pPlayer)
	{
		Warning("%s Tried to access a null ZEPlayer!!\n", player->GetPlayerName());
		return;
	}

	if (!pPlayer->GetExtendVote())
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "You have not voted to extend current map.");
		return;
	}

	pPlayer->SetExtendVote(false);
	ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "You no longer want to extend current map.");
}

CON_COMMAND_CHAT_FLAGS(blockrtv, "Block the ability for players to vote to end current map sooner.", ADMFLAG_CHANGEMAP)
{
	const char* pszCommandPlayerName = player ? player->GetPlayerName() : "Console";

	g_RTVState = ERTVState::BLOCKED_BY_ADMIN;

	ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX ADMIN_PREFIX "blocked vote for RTV.", pszCommandPlayerName);
}

CON_COMMAND_CHAT_FLAGS(unblockrtv, "Restore the ability for players to vote to end current map sooner.", ADMFLAG_CHANGEMAP)
{
	const char* pszCommandPlayerName = player ? player->GetPlayerName() : "Console";

	g_RTVState = ERTVState::RTV_ALLOWED;

	ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX ADMIN_PREFIX "restored vote for RTV.", pszCommandPlayerName);
}

CON_COMMAND_CHAT_FLAGS(addextend, "Add another extend to the current map for players to vote.", ADMFLAG_CHANGEMAP)
{
	const char* pszCommandPlayerName = player ? player->GetPlayerName() : "Console";

	if (g_ExtendState == EExtendState::POST_EXTEND_NO_EXTENDS_LEFT || g_ExtendState == EExtendState::NO_EXTENDS)
		g_ExtendState = EExtendState::EXTEND_ALLOWED;
	
	g_ExtendsLeft += 1;

	ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX ADMIN_PREFIX "allowed for an additional extend.", pszCommandPlayerName);
}

CON_COMMAND_CHAT(extendsleft, "Display amount of extends left for the current map")
{
	char message[64];

	switch (g_ExtendsLeft)
	{
	case 0:
		strcpy(message, "There are no extends left.");
		break;
	case 1:
		strcpy(message, "There's 1 extend left");
		break;
	default:
		V_snprintf(message, sizeof(message), "There are %i extends left.", g_ExtendsLeft);
		break;
	}

	if (player)
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "%s", message);
	else
		ConMsg("%s", message);
}

// TODO POST ENTFIRE BRANCH MERGE
#if 0
CON_COMMAND_CHAT(timeleft, "Display time left to end of current map.")
{
	if (!player)
	{
		ClientPrint(player, HUD_PRINTCONSOLE, CHAT_PREFIX "You cannot use this command from the server console.");
		return;
	}

	int iPlayer = player->GetPlayerSlot();
	float flTimeLeft;

	ConVar* cvar = g_pCVar->GetConVar(g_pCVar->FindConVar("mp_timelimit"));

	float pflTimelimit;
	// le funny type punning
	memcpy(&pflTimelimit, &cvar->values, sizeof(pflTimelimit));

	ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "There's %f time left", gpGlobals->curtime);
}
#endif

// Set amount of extends for the current map based on the 'maps' config file
void SetExtendsLeft()
{
	KeyValues* pKV = new KeyValues("maps");
	KeyValues::AutoDelete autoDelete(pKV);

	const char* pszPath = "addons/cs2fixes/configs/maps.cfg";

	const char* pszMapname = gpGlobals->mapname.ToCStr();

	if (!pKV->LoadFromFile(g_pFullFileSystem, pszPath))
	{
		Warning("Failed to load %s, defaulting to 1 extend for current map.\n", pszPath);
		g_ExtendsLeft = 1;
		return;
	}
	for (KeyValues* pKey = pKV->GetFirstSubKey(); pKey; pKey = pKey->GetNextKey())
	{
		const char* pszName = pKey->GetName();
		
		if (strcmp(pszName, pszMapname) != 0)
			continue;
		
		int iExtendsLeft = pKey->GetInt("extends", -1);

		if (iExtendsLeft == -1)
		{
			ConMsg("Map entry %s is missing 'extends' key, defaulting to 1 extend\n", pszMapname);
			g_ExtendsLeft = 1;
			return;
		}

		if (iExtendsLeft == 0)
			g_ExtendState = EExtendState::NO_EXTENDS;

		ConMsg("Setting %i extend(s) for map: %s\n", iExtendsLeft, pszMapname);
		g_ExtendsLeft = iExtendsLeft;
		return;
	}

	ConMsg("Could not find map \"%s\" in %s, defaulting to 1 extend\n", pszMapname, pszPath);
	g_ExtendsLeft = 1;
}