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
#include "common.h"
#include "igameevents.h"
#include "utlstring.h"
#include "utlvector.h"

typedef void (*FnEventListenerCallback)(IGameEvent* event);

class CGameEventListener;

extern CUtlVector<CGameEventListener*> g_vecEventListeners;

class CGameEventListener : public IGameEventListener2
{
public:
	CGameEventListener(FnEventListenerCallback callback, const char* pszEventName) :
		m_Callback(callback), m_pszEventName(pszEventName)
	{
		g_vecEventListeners.AddToTail(this);
	}

	~CGameEventListener() override
	{
	}

	// FireEvent is called by EventManager if event just occured
	// KeyValue memory will be freed by manager if not needed anymore
	void FireGameEvent(IGameEvent* event) override
	{
		m_Callback(event);
	}

	const char* GetEventName() { return m_pszEventName; }

private:
	FnEventListenerCallback m_Callback;
	const char* m_pszEventName;
};

void RegisterEventListeners();
void UnregisterEventListeners();

#define GAME_EVENT_F(_event)                                          \
	void _event##_callback(IGameEvent*);                              \
	CGameEventListener _event##_listener(_event##_callback, #_event); \
	void _event##_callback(IGameEvent* pEvent)