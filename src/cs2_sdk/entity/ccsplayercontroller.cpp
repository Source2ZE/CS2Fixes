#include "ccsplayercontroller.h"
#undef CreateEvent
#include <igameevents.h>
#include <src/eventlistener.h>

extern IGameEventManager2* g_gameEventManager;

void CCSPlayerController::ShowRespawnStatus(const char* str)
{
	IGameEvent* pEvent = g_gameEventManager->CreateEvent("show_survival_respawn_status");

	if (!pEvent)
		return;

	pEvent->SetPlayer("userid", GetPlayerSlot());
	pEvent->SetUint64("duration", 10);
	pEvent->SetString("loc_token", str);

	GetClientEventListener(GetPlayerSlot())->FireGameEvent(pEvent);
	delete pEvent;
}