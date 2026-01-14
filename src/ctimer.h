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
#include "cs2fixes.h"
#include <functional>
#include <list>
#include <memory>

// clang-format off
#define TIMERFLAG_NONE		(0)
#define TIMERFLAG_MAP		(1 << 0) // Only valid for this map, cancels on map change
#define TIMERFLAG_ROUND		(1 << 1) // Only valid for this round, cancels on new round
// clang-format on

class CTimerBase
{
protected:
	CTimerBase(float flInitialInterval, uint64 nTimerFlags) :
		m_flInterval(flInitialInterval), m_nTimerFlags(nTimerFlags)
	{}

	void SetInterval(float flInterval) { m_flInterval = flInterval; }
	void SetLastExecute(float flLastExecute) { m_flLastExecute = flLastExecute; }

public:
	virtual bool Execute(bool bAutomaticExecute = false) = 0;
	virtual void Cancel() = 0;

	float GetInterval() { return m_flInterval; }
	float GetLastExecute() { return m_flLastExecute; }
	bool IsTimerFlagSet(uint64 iTimerFlag) { return !iTimerFlag || (m_nTimerFlags & iTimerFlag); }
	void Initialize()
	{
		if (m_flLastExecute == -1)
			m_flLastExecute = g_flUniversalTime;
	}

private:
	float m_flInterval;
	float m_flLastExecute = -1;
	uint64 m_nTimerFlags;
};

// Timer functions should return the time until next execution, or a negative value like -1.0f to stop
// Having an interval of 0 is fine, in this case it will run on every game frame
class CTimer : public CTimerBase, public std::enable_shared_from_this<CTimer>
{
private:
	// Silly workaround to achieve a "private constructor" only Create() can call
	struct _timer_constructor_tag
	{
		explicit _timer_constructor_tag() = default;
	};

public:
	CTimer(float flInitialInterval, uint64 nTimerFlags, std::function<float()> func, _timer_constructor_tag) :
		CTimerBase(flInitialInterval, nTimerFlags), m_func(func)
	{}

	static std::weak_ptr<CTimer> Create(float flInitialInterval, uint64 nTimerFlags, std::function<float()> func);
	bool Execute(bool bAutomaticExecute) override;
	void Cancel() override;

private:
	std::function<float()> m_func;
};

void RunTimers();
void RemoveAllTimers();
void RemoveTimers(uint64 iTimerFlag);