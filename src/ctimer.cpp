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

#include "ctimer.h"

std::list<std::shared_ptr<CTimerBase>> g_timers;

void RunTimers()
{
	auto iterator = g_timers.begin();

	while (iterator != g_timers.end())
	{
		auto pTimer = *iterator;
		pTimer->Initialize();

		// Timer execute
		if (pTimer->GetLastExecute() + pTimer->GetInterval() <= g_flUniversalTime && !pTimer->Execute(true))
			iterator = g_timers.erase(iterator);
		else
			iterator++;
	}
}

void RemoveAllTimers()
{
	g_timers.clear();
}

void RemoveTimers(uint64 iTimerFlag)
{
	auto iterator = g_timers.begin();

	while (iterator != g_timers.end())
		if ((*iterator)->IsTimerFlagSet(iTimerFlag))
			iterator = g_timers.erase(iterator);
		else
			iterator++;
}

std::weak_ptr<CTimer> CTimer::Create(float flInitialInterval, uint64 nTimerFlags, std::function<float()> func)
{
	auto pTimer = std::make_shared<CTimer>(flInitialInterval, nTimerFlags, func, _timer_constructor_tag{});

	g_timers.push_back(pTimer);
	return pTimer;
}

bool CTimer::Execute(bool bAutomaticExecute)
{
	SetInterval(m_func());
	SetLastExecute(g_flUniversalTime);

	bool bContinue = GetInterval() >= 0;

	// Only scan the timer list if this isn't an automatic execute (RunTimers() already has the iterator to erase)
	if (!bAutomaticExecute && !bContinue)
		Cancel();

	return bContinue;
}

void CTimer::Cancel()
{
	auto iterator = g_timers.begin();

	while (iterator != g_timers.end())
	{
		if (*iterator == shared_from_this())
		{
			g_timers.erase(iterator);
			break;
		}

		iterator++;
	}
}