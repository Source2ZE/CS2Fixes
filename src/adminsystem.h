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

#pragma once
#include "platform.h"
#include "utlvector.h"
#include "playermanager.h"
#include <ctime>

#define ADMFLAG_RESERVATION			(1<<0)		/**< Convenience macro for Admin_Reservation as a FlagBit */
#define ADMFLAG_GENERIC				(1<<1)		/**< Convenience macro for Admin_Generic as a FlagBit */
#define ADMFLAG_KICK				(1<<2)		/**< Convenience macro for Admin_Kick as a FlagBit */
#define ADMFLAG_BAN					(1<<3)		/**< Convenience macro for Admin_Ban as a FlagBit */
#define ADMFLAG_UNBAN				(1<<4)		/**< Convenience macro for Admin_Unban as a FlagBit */
#define ADMFLAG_SLAY				(1<<5)		/**< Convenience macro for Admin_Slay as a FlagBit */
#define ADMFLAG_CHANGEMAP			(1<<6)		/**< Convenience macro for Admin_Changemap as a FlagBit */
#define ADMFLAG_CONVARS				(1<<7)		/**< Convenience macro for Admin_Convars as a FlagBit */
#define ADMFLAG_CONFIG				(1<<8)		/**< Convenience macro for Admin_Config as a FlagBit */
#define ADMFLAG_CHAT				(1<<9)		/**< Convenience macro for Admin_Chat as a FlagBit */
#define ADMFLAG_VOTE				(1<<10)		/**< Convenience macro for Admin_Vote as a FlagBit */
#define ADMFLAG_PASSWORD			(1<<11)		/**< Convenience macro for Admin_Password as a FlagBit */
#define ADMFLAG_RCON				(1<<12)		/**< Convenience macro for Admin_RCON as a FlagBit */
#define ADMFLAG_CHEATS				(1<<13)		/**< Convenience macro for Admin_Cheats as a FlagBit */
#define ADMFLAG_ROOT				(1<<14)		/**< Convenience macro for Admin_Root as a FlagBit */
#define ADMFLAG_CUSTOM1				(1<<15)		/**< Convenience macro for Admin_Custom1 as a FlagBit */
#define ADMFLAG_CUSTOM2				(1<<16)		/**< Convenience macro for Admin_Custom2 as a FlagBit */
#define ADMFLAG_CUSTOM3				(1<<17)		/**< Convenience macro for Admin_Custom3 as a FlagBit */
#define ADMFLAG_CUSTOM4				(1<<18)		/**< Convenience macro for Admin_Custom4 as a FlagBit */
#define ADMFLAG_CUSTOM5				(1<<19)		/**< Convenience macro for Admin_Custom5 as a FlagBit */
#define ADMFLAG_CUSTOM6				(1<<20)		/**< Convenience macro for Admin_Custom6 as a FlagBit */


class CInfractionBase
{
public:
	CInfractionBase(int duration, uint64 steamId) : m_iSteamID(steamId)
	{
		// The duration is in minutes here
		m_iTimestamp = duration != 0 ? std::time(nullptr) + (duration * 60) : 0;
	}
	enum EInfractionType
	{
		Ban,
		Mute,
		Gag
	};

	virtual EInfractionType GetType() = 0;
	virtual void ApplyInfraction(ZEPlayer*) = 0;
	virtual void UndoInfraction(ZEPlayer *) = 0;
	int GetTimestamp() { return m_iTimestamp; }
	uint64 GetSteamId64() { return m_iSteamID; }

private:
	int m_iTimestamp;
	uint64 m_iSteamID;
};

class CBanInfraction : public CInfractionBase
{
public:
	using CInfractionBase::CInfractionBase;
	
	EInfractionType GetType() override { return Ban; }
	void ApplyInfraction(ZEPlayer*) override;

	// This isn't needed as we'll just not kick the player when checking infractions upon joining
	void UndoInfraction(ZEPlayer *) override {}
};

class CMuteInfraction :public CInfractionBase
{
public:
	using CInfractionBase::CInfractionBase;
	
	EInfractionType GetType() override { return Mute; }
	void ApplyInfraction(ZEPlayer*) override;
	void UndoInfraction(ZEPlayer *) override;
};

class CGagInfraction : public CInfractionBase
{
public:
	using CInfractionBase::CInfractionBase;

	EInfractionType GetType() override { return Gag; }
	void ApplyInfraction(ZEPlayer *) override;
	void UndoInfraction(ZEPlayer *) override;
};

class CAdmin
{
public:
	CAdmin(const char* pszName, uint64 iSteamID, uint64 iFlags) : 
		m_pszName(pszName), m_iSteamID(iSteamID), m_iFlags(iFlags)
	{}

	const char* GetName() { return m_pszName; };
	uint64 GetSteamID() { return m_iSteamID; };
	uint64 GetFlags() { return m_iFlags; };

private:
	const char* m_pszName;
	uint64 m_iSteamID;
	uint64 m_iFlags;
};

class CAdminSystem
{
public:
	CAdminSystem()
	{
		LoadAdmins();
		LoadInfractions();
	}
	bool LoadAdmins();
	bool LoadInfractions();
	void AddInfraction(CInfractionBase*);
	void SaveInfractions();
	void ApplyInfractions(ZEPlayer *player);
	bool FindAndRemoveInfraction(ZEPlayer *player, CInfractionBase::EInfractionType type);
	CAdmin *FindAdmin(uint64 iSteamID);

private:
	uint64 ParseFlags(const char* pszFlags);

	CUtlVector<CAdmin> m_vecAdmins;
	CUtlVector<CInfractionBase*> m_vecInfractions;
};

extern CAdminSystem* g_pAdminSystem;
