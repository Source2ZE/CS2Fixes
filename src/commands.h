#pragma once
#include "entity/ccsplayercontroller.h"
#include "convar.h"

#define COMMAND_PREFIX "c_"

typedef void (*FnChatCommandCallback_t)(const CCommand &args, CCSPlayerController *player);

extern CUtlMap<uint32, FnChatCommandCallback_t> g_CommandList;

void ClientPrintAll(int destination, const char *msg, ...);
void ClientPrint(CBasePlayerController *player, int destination, const char *msg, ...);

// Just a wrapper class so we're able to insert the callback
class CChatCommand
{
public:
	CChatCommand(const char *cmd, FnChatCommandCallback_t callback)
	{
		g_CommandList.Insert(hash_32_fnv1a_const(cmd), callback);
	}
};

struct WeaponMapEntry_t
{
	const char *command;
	const char *szWeaponName;
	int iPrice;
	uint16 iItemDefIndex;
};

void ParseChatCommand(const char *, CCSPlayerController *);

#define CON_COMMAND_CHAT(name, description)																											\
	void name##_callback(const CCommand &args, CCSPlayerController *player);																		\
	static void name##_con_callback(const CCommandContext &context, const CCommand &args)															\
	{																																				\
		name##_callback(args, (CCSPlayerController *)g_pEntitySystem->GetBaseEntity((CEntityIndex)(context.GetPlayerSlot().Get() + 1)));			\
	}																																				\
	static CChatCommand name##_chat_command(#name, name##_callback);																				\
	static ConCommandRefAbstract name##_ref;																										\
	static ConCommand name##_command(&name##_ref, COMMAND_PREFIX #name, name##_con_callback,														\
									description, FCVAR_CLIENT_CAN_EXECUTE | FCVAR_LINKED_CONCOMMAND);												\
	void name##_callback(const CCommand &args, CCSPlayerController *player)
