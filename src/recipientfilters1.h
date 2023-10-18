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
#include "irecipientfilter.h"

class CSingleRecipientFilter : public IRecipientFilter
{
public:
	CSingleRecipientFilter(int iRecipient, bool bReliable = true, bool bInitMessage = false) :
		m_iRecipient(iRecipient), m_bReliable(bReliable), m_bInitMessage(bInitMessage) {}

	~CSingleRecipientFilter() override {}

	bool IsReliable(void) const override { return m_bReliable; }

	bool IsInitMessage(void) const override { return m_bInitMessage; }

	int GetRecipientCount(void) const override { return 1; }

	CPlayerSlot GetRecipientIndex(int slot) const override { return CPlayerSlot(m_iRecipient); }

private:
	bool m_bReliable;
	bool m_bInitMessage;
	int m_iRecipient;
};

class CCopyRecipientFilter : public IRecipientFilter
{
public:
	CCopyRecipientFilter(IRecipientFilter *source, int iExcept)
	{
		m_bReliable = source->IsReliable();
		m_bInitMessage = source->IsInitMessage();
		m_Recipients.RemoveAll();

		for (int i = 0; i < source->GetRecipientCount(); i++)
		{
			if (source->GetRecipientIndex(i).Get() != iExcept)
				m_Recipients.AddToTail(source->GetRecipientIndex(i));
		}
	}

	~CCopyRecipientFilter() override {}

	bool IsReliable(void) const override { return m_bReliable; }

	bool IsInitMessage(void) const override { return m_bInitMessage; }

	int GetRecipientCount(void) const override { return m_Recipients.Count(); }

	CPlayerSlot GetRecipientIndex(int slot) const override
	{
		if (slot < 0 || slot >= GetRecipientCount())
			return CPlayerSlot(-1);

		return m_Recipients[slot];
	}

private:
	bool m_bReliable;
	bool m_bInitMessage;
	CUtlVectorFixed<CPlayerSlot, 64> m_Recipients;
};