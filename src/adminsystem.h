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

#pragma once
#include "platform.h"
#include "playermanager.h"
#include "utlvector.h"
#include <ctime>

// clang-format off
#define ADMFLAG_NONE		(0)
#define ADMFLAG_RESERVATION (1 << 0)  // a
#define ADMFLAG_GENERIC		(1 << 1)  // b
#define ADMFLAG_KICK		(1 << 2)  // c
#define ADMFLAG_BAN			(1 << 3)  // d
#define ADMFLAG_UNBAN		(1 << 4)  // e
#define ADMFLAG_SLAY		(1 << 5)  // f
#define ADMFLAG_CHANGEMAP	(1 << 6)  // g
#define ADMFLAG_CONVARS		(1 << 7)  // h
#define ADMFLAG_CONFIG		(1 << 8)  // i
#define ADMFLAG_CHAT		(1 << 9)  // j
#define ADMFLAG_VOTE		(1 << 10) // k
#define ADMFLAG_PASSWORD	(1 << 11) // l
#define ADMFLAG_RCON		(1 << 12) // m
#define ADMFLAG_CHEATS		(1 << 13) // n
#define ADMFLAG_CUSTOM1		(1 << 14) // o
#define ADMFLAG_CUSTOM2		(1 << 15) // p
#define ADMFLAG_CUSTOM3		(1 << 16) // q
#define ADMFLAG_CUSTOM4		(1 << 17) // r
#define ADMFLAG_CUSTOM5		(1 << 18) // s
#define ADMFLAG_CUSTOM6		(1 << 19) // t
#define ADMFLAG_CUSTOM7		(1 << 20) // u
#define ADMFLAG_CUSTOM8		(1 << 21) // v
#define ADMFLAG_CUSTOM9		(1 << 22) // w
#define ADMFLAG_CUSTOM10	(1 << 23) // x
#define ADMFLAG_CUSTOM11	(1 << 24) // y
#define ADMFLAG_ROOT		(1 << 25) // z
// clang-format on

#define ADMIN_PREFIX "Admin %s has "
#define CONSOLE_NAME "\2CONSOLE\1" // color it to indicate that it isnt a regular player using the command

void PrintSingleAdminAction(const char* pszAdminName, const char* pszTargetName, const char* pszAction, const char* pszAction2, const char* prefix);
void PrintMultiAdminAction(ETargetType nType, const char* pszAdminName, const char* pszAction, const char* pszAction2, const char* prefix);

enum GrammarTense
{
	PresentOrNoun,
	Past,
	Continuous
};

class CInfractionBase
{
public:
	CInfractionBase(time_t duration, uint64 steamId, bool bEndTime = false) :
		m_iSteamID(steamId)
	{
		// The duration is in minutes here
		if (!bEndTime)
			m_iTimestamp = duration != 0 ? std::time(nullptr) + (duration * 60) : 0;
		else
			m_iTimestamp = duration;
	}
	enum EInfractionType
	{
		Ban,
		Mute,
		Gag
	};

	virtual EInfractionType GetType() = 0;
	virtual void ApplyInfraction(ZEPlayer*) = 0;
	virtual void UndoInfraction(ZEPlayer*) = 0;
	time_t GetTimestamp() { return m_iTimestamp; }
	uint64 GetSteamId64() { return m_iSteamID; }

private:
	time_t m_iTimestamp;
	uint64 m_iSteamID;
};

class CBanInfraction : public CInfractionBase
{
public:
	using CInfractionBase::CInfractionBase;

	EInfractionType GetType() override { return Ban; }
	void ApplyInfraction(ZEPlayer*) override;

	// This isn't needed as we'll just not kick the player when checking infractions upon joining
	void UndoInfraction(ZEPlayer*) override {}
};

class CMuteInfraction : public CInfractionBase
{
public:
	using CInfractionBase::CInfractionBase;

	EInfractionType GetType() override { return Mute; }
	void ApplyInfraction(ZEPlayer*) override;
	void UndoInfraction(ZEPlayer*) override;
};

class CGagInfraction : public CInfractionBase
{
public:
	using CInfractionBase::CInfractionBase;

	EInfractionType GetType() override { return Gag; }
	void ApplyInfraction(ZEPlayer*) override;
	void UndoInfraction(ZEPlayer*) override;
};

class CAdmin
{
public:
	CAdmin(const char* pszName, uint64 iSteamID, uint64 iFlags, int iAdminImmunity) :
		m_pszName(pszName), m_iSteamID(iSteamID), m_iFlags(iFlags), m_iAdminImmunity(iAdminImmunity)
	{}

	const char* GetName() { return m_pszName; }
	uint64 GetSteamID() { return m_iSteamID; }
	uint64 GetFlags() { return m_iFlags; }
	int GetImmunity() { return m_iAdminImmunity; }

private:
	const char* m_pszName;
	uint64 m_iSteamID;
	uint64 m_iFlags;
	int m_iAdminImmunity;
};

class CAdminSystem
{
public:
	CAdminSystem();
	bool LoadAdmins();
	bool LoadInfractions();
	void AddInfraction(CInfractionBase*);
	void SaveInfractions();
	bool ApplyInfractions(ZEPlayer* player);
	bool FindAndRemoveInfraction(ZEPlayer* player, CInfractionBase::EInfractionType type);
	bool FindAndRemoveInfractionSteamId64(uint64 steamid64, CInfractionBase::EInfractionType type);
	CAdmin* FindAdmin(uint64 iSteamID);
	uint64 ParseFlags(const char* pszFlags);
	void AddDisconnectedPlayer(const char* pszName, uint64 xuid, const char* pszIP);
	void ShowDisconnectedPlayers(CCSPlayerController* const pAdmin);

private:
	CUtlVector<CAdmin> m_vecAdmins;
	CUtlVector<CInfractionBase*> m_vecInfractions;

	// Implemented as a circular buffer.
	std::tuple<std::string, uint64, std::string> m_rgDCPly[20];
	int m_iDCPlyIndex;
};

extern CAdminSystem* g_pAdminSystem;

// Given a formatted time entered by an admin, return the minutes
int ParseTimeInput(std::string strTime);

// Given a time in seconds/minutes, returns a formatted string of the largest (floored) unit of time this exceeds, up to months.
// Example: FormatTime(70) == "1 minute"
std::string FormatTime(std::time_t wTime, bool bInSeconds = true);

// Gets reason from a user command such as mute, gag, ban, etc.
std::string GetReason(const CCommand& args, int iArgsBefore, bool bStripUnicode);