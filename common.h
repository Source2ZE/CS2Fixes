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
