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
#include <functional>
#include "utllinkedlist.h"

class CTimerBase {
public:
    CTimerBase(float time, bool repeat, bool preserveMapChange) :
        m_flTime(time), m_bRepeat(repeat), m_bPreserveMapChange(preserveMapChange) {};

    virtual void Execute() = 0;

    float m_flTime;
    float m_flLastExecute = -1;
    bool m_bRepeat;
    bool m_bPreserveMapChange;
};

extern CUtlLinkedList<CTimerBase*> g_timers;


class CTimer : public CTimerBase
{
public:
    CTimer(float time, bool repeat, bool preserveMapChange, std::function<void()> func) :
		CTimerBase(time, repeat, preserveMapChange), m_func(func)
    {
        g_timers.AddToTail(this);
    };

    virtual void Execute() override
    {
	    m_func();
	}

    std::function<void()> m_func;
};


void RemoveTimers();
void RemoveMapTimers();