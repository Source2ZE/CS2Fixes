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

#include "common.h"
#include "entitysystem.h"

// Stupid, but hopefully temporary hack
#define private public
#define protected public
#include "igamesystemfactory.h"
#undef protected
#undef private

bool InitGameSystems();
bool UnregisterGameSystem();

template <class T, class U = T>
class CGameSystemStaticCustomFactory : public CGameSystemStaticFactory<T, U>
{
public:
	CGameSystemStaticCustomFactory(const char* pName, T* pActualGlobal, U** ppGlobalPointer = nullptr) :
		CGameSystemStaticFactory<T, U>(pName, pActualGlobal, ppGlobalPointer)
	{
	}

	void Destroy()
	{
		CBaseGameSystemFactory* pFactoryCurrent = *CBaseGameSystemFactory::sm_pFirst;
		CBaseGameSystemFactory* pFactoryPrevious = nullptr;
		while (pFactoryCurrent)
		{
			if (strcmp(pFactoryCurrent->m_pName, this->m_pName) == 0)
			{
				if (pFactoryPrevious == nullptr)
					*CBaseGameSystemFactory::sm_pFirst = pFactoryCurrent->m_pNext;
				else
					pFactoryPrevious->m_pNext = pFactoryCurrent->m_pNext;
				delete pFactoryCurrent;
				return;
			}
			pFactoryPrevious = pFactoryCurrent;
			pFactoryCurrent = pFactoryCurrent->m_pNext;
		}
	}
};

class CGameSystem : public CBaseGameSystem
{
public:
	GS_EVENT(BuildGameSessionManifest);
	GS_EVENT(ServerPreEntityThink);
	GS_EVENT(ServerPostEntityThink);
	GS_EVENT(GameShutdown);

	void Shutdown() override
	{
		Message("CGameSystem::Shutdown\n");
		delete sm_Factory;
	}

	void SetGameSystemGlobalPtrs(void* pValue) override
	{
		if (sm_Factory)
			sm_Factory->SetGlobalPtr(pValue);
	}

	bool DoesGameSystemReallocate() override
	{
		return sm_Factory->ShouldAutoAdd();
	}

	static CGameSystemStaticCustomFactory<CGameSystem>* sm_Factory;
};

// Quick and dirty definition
// MSVC for whatever reason flips overload ordering, and this has three of them
// So this is based on the linux bin which is correct, and MSVC will flip it to match the windows bin, fun
class IEntityResourceManifest
{
public:
	virtual void AddResource(const char*) = 0;
	virtual void AddResource(const char*, void*) = 0;
	virtual void AddResource(const char*, void*, void*, void*) = 0;
	virtual void unk_04() = 0;
	virtual void unk_05() = 0;
	virtual void unk_06() = 0;
	virtual void unk_07() = 0;
	virtual void unk_08() = 0;
	virtual void unk_09() = 0;
	virtual void unk_10() = 0;
};

abstract_class IGameSystemEventDispatcher{
	public :
		virtual ~IGameSystemEventDispatcher(){}
};

class CGameSystemEventDispatcher : public IGameSystemEventDispatcher
{
public:
	CUtlVector<CUtlVector<IGameSystem*>>* m_funcListeners;
};

struct AddedGameSystem_t
{
	IGameSystem* m_pGameSystem;
	int m_nPriority;
	int m_nInsertionOrder;
};

extern CGameSystem g_GameSystem;
