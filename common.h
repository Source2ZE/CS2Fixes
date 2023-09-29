#pragma once

#include "platform.h"
#include "irecipientfilter.h"

// #define HOOK_CONVARS
// #define HOOK_CONCOMMANDS
// #define USE_TICKRATE
// #define USE_DEBUG_CONSOLE

#define ROOTBIN "/bin/win64/"
#define GAMEBIN "/csgo/bin/win64/"

#define SOURCE2SERVERCONFIG_INTERFACE_VERSION "Source2ServerConfig001"

#ifdef HOOK_CONVARS
struct ConVarInfo
{
	char *name;		  // 0x00
	char *desc;		  // 0x08
	uint64 flags;	  // 0x10
	char pad18[0x40]; // 0x18
	uint64 type;	  // 0x58
};

typedef void (*RegisterConVar)(void *, ConVarInfo *, void *, void *, void *);

void HookConVars();
#endif

#ifdef HOOK_CONCOMMANDS
struct ConCommandInfo
{
	char *name;	  // 0x00
	char *desc;	  // 0x08
	uint64 flags; // 0x10
};

typedef void *(*RegisterConCommand)(void *, void *, ConCommandInfo *, void *, void *);

void HookConCommands();
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
void InitLoggingDetours();

void Message(const char *, ...);
void Panic(const char *, ...);

void *FindSignature(void *, const byte *, size_t);


class CRecipientFilter : public IRecipientFilter
{
public:
	CRecipientFilter() { Reset(); }

	~CRecipientFilter() override {}

	bool IsReliable(void) const override;
	bool IsInitMessage(void) const override;

	int GetRecipientCount(void) const override;
	CEntityIndex GetRecipientIndex(int slot) const override;

public:
	void Reset();
	void MakeReliable();
	void AddRecipient(int entindex);

private:
	bool m_bReliable;
	bool m_bInitMessage;
	CUtlVectorFixedGrowable<int, 64> m_Recipients;
};
