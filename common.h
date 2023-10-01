#pragma once

#include "platform.h"
#include "irecipientfilter.h"
#include "entitysystem.h"

extern CEntitySystem* g_pEntitySystem;

// #define HOOK_CONVARS
// #define HOOK_CONCOMMANDS
// #define USE_TICKRATE
// #define USE_DEBUG_CONSOLE

#ifdef _WIN32
#define ROOTBIN "/bin/win64/"
#define GAMEBIN "/csgo/bin/win64/"
#else
#define ROOTBIN "/bin/linuxsteamrt64/"
#define GAMEBIN "/csgo/bin/linuxsteamrt64/"
#endif

struct WeaponMapEntry_t
{
	const char *command;
	const char *szWeaponName;
	int iPrice;
};

void UnlockConVars();
void UnlockConCommands();
void ToggleLogs();
void SetMaxPlayers(byte);

void InitPatches();
void UndoPatches();
void InitLoggingDetours();

void Message(const char *, ...);
void Panic(const char *, ...);

void *FindSignature(void *, const byte *, size_t);


class CSingleRecipientFilter : public IRecipientFilter
{
public:
	CSingleRecipientFilter(int iRecipient, bool bReliable = true, bool bInitMessage = false) :
		m_iRecipient(iRecipient), m_bReliable(bReliable), m_bInitMessage(bInitMessage) {}

	~CSingleRecipientFilter() override {}

	bool IsReliable(void) const override { return m_bReliable; }
	bool IsInitMessage(void) const override { return m_bInitMessage; }
	int GetRecipientCount(void) const override { return 1; }
	CEntityIndex GetRecipientIndex(int slot) const override { return CEntityIndex(m_iRecipient); }

private:
	bool m_bReliable;
	bool m_bInitMessage;
	int m_iRecipient;
};

class CCopyRecipientFilter : public IRecipientFilter
{
public:
	CCopyRecipientFilter(IRecipientFilter *source, int iExcept)
	{
		m_bReliable = source->IsReliable();
		m_bInitMessage = source->IsInitMessage();
		m_Recipients.RemoveAll();

		for (int i = 0; i < source->GetRecipientCount(); i++)
		{
			if (source->GetRecipientIndex(i).Get() != iExcept)
				m_Recipients.AddToTail(source->GetRecipientIndex(i));
		}
	}

	~CCopyRecipientFilter() override {}

	bool IsReliable(void) const override { return m_bReliable; }
	bool IsInitMessage(void) const override { return m_bInitMessage; }
	int GetRecipientCount(void) const override { return m_Recipients.Count(); }

	CEntityIndex GetRecipientIndex(int slot) const override
	{
		if (slot < 0 || slot >= GetRecipientCount())
			return CEntityIndex(-1);

		return m_Recipients[slot];
	}

private:
	bool m_bReliable;
	bool m_bInitMessage;
	CUtlVectorFixed<CEntityIndex, 64> m_Recipients;
};
