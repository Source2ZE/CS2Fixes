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