/**
 * =============================================================================
 * CS2Fixes
 * Copyright (C) 2023-2025 Source2ZE
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
#include "utllinkedlist.h"
#include <functional>

extern int g_iRoundNum;

class CTimerBase
{
public:
	CTimerBase(float flInitialInterval, bool bPreserveMapChange, bool bPreserveRoundChange) :
		m_flInterval(flInitialInterval), m_bPreserveMapChange(bPreserveMapChange), m_bPreserveRoundChange(bPreserveRoundChange)
	{
		m_iRoundNum = g_iRoundNum;
	}

	virtual bool Execute() = 0;

	float m_flInterval;
	float m_flLastExecute = -1;
	bool m_bPreserveMapChange;
	bool m_bPreserveRoundChange;
	int m_iRoundNum;
};

extern CUtlLinkedList<CTimerBase*> g_timers;

// Timer functions should return the time until next execution, or a negative value like -1.0f to stop
// Having an interval of 0 is fine, in this case it will run on every game frame
class CTimer : public CTimerBase
{
public:
	CTimer(float flInitialInterval, bool bPreserveMapChange, bool bPreserveRoundChange, std::function<float()> func) :
		CTimerBase(flInitialInterval, bPreserveMapChange, bPreserveRoundChange), m_func(func)
	{
		g_timers.AddToTail(this);
	};

	inline bool Execute() override
	{
		m_flInterval = m_func();

		return m_flInterval >= 0;
	}

	std::function<float()> m_func;
};

void RemoveTimers();
void RemoveMapTimers();