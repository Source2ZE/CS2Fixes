/**
 * =============================================================================
 * CS2Fixes
 * Copyright (C) 2023-2026 Source2ZE
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
#include "../gameconfig.h"
#include "khook.hpp"

class CSigHookBase
{
public:
	CSigHookBase(const char* pSignature) :
		m_pSignatureName(pSignature)
	{
	}

	virtual void Configure() = 0;

protected:
	const char* m_pSignatureName;
};

extern std::vector<CSigHookBase*>& GetSigHookList();
void InitSigHooks();

template <typename RETURN, typename... ARGS>
class CSigHookFunction : public CSigHookBase
{
public:
	using fnCallback = KHook::Return<RETURN> (*)(ARGS...);

	CSigHookFunction(const char* pSignature, fnCallback cbPre = nullptr, fnCallback cbPost = nullptr) :
		CSigHookBase(pSignature), m_hook(cbPre, cbPost)
	{
		GetSigHookList().push_back(this);
	}

	virtual void Configure() override
	{
		auto address = g_GameConfig->ResolveSignature(m_pSignatureName);

		if (!address)
			g_bRequiredInitLoaded = false;

		m_hook.Configure(address);

		Message("Hooked %s at 0x%p\n", m_pSignatureName, address);
	}

	RETURN CallOriginal(ARGS... args)
	{
		return m_hook.CallOriginal(args...);
	}

private:
	KHook::Function<RETURN, ARGS...> m_hook;
};

template <typename RETURN, typename... ARGS>
auto MakeSigHookFunction(const char* pSignature, KHook::Return<RETURN> (*cbPre)(ARGS...), nullptr_t cbPost)
{
	return std::make_unique<CSigHookFunction<RETURN, ARGS...>>(pSignature, cbPre, nullptr);
}

template <typename RETURN, typename... ARGS>
auto MakeSigHookFunction(const char* pSignature, nullptr_t cbPre, KHook::Return<RETURN> (*cbPost)(ARGS...))
{
	return std::make_unique<CSigHookFunction<RETURN, ARGS...>>(pSignature, nullptr, cbPost);
}

template <typename RETURN, typename... ARGS>
auto MakeSigHookFunction(const char* pSignature, KHook::Return<RETURN> (*cbPre)(ARGS...), KHook::Return<RETURN> (*cbPost)(ARGS...))
{
	return std::make_unique<CSigHookFunction<RETURN, ARGS...>>(pSignature, cbPre, cbPost);
}

#define SIG_HOOK_FUNCTION(name, pre, post) \
	static auto hook##name = MakeSigHookFunction(#name, pre, post)

template <typename CLASS, typename RETURN, typename... ARGS>
class CSigHookMember : public CSigHookBase
{
public:
	using fnCallback = KHook::Return<RETURN> (*)(CLASS*, ARGS...);

	CSigHookMember(const char* pSignature, fnCallback cbPre = nullptr, fnCallback cbPost = nullptr) :
		CSigHookBase(pSignature), m_hook(cbPre, cbPost)
	{
		GetSigHookList().push_back(this);
	}

	virtual void Configure() override
	{
		auto address = g_GameConfig->ResolveSignature(m_pSignatureName);

		if (!address)
			g_bRequiredInitLoaded = false;

		m_hook.Configure(address);

		Message("Hooked %s at 0x%p\n", m_pSignatureName, address);
	}

	RETURN CallOriginal(CLASS* this_ptr, ARGS... args)
	{
		return m_hook.CallOriginal(this_ptr, args...);
	}

private:
	KHook::Member<CLASS, RETURN, ARGS...> m_hook;
};

template <typename CLASS, typename RETURN, typename... ARGS>
auto MakeSigHookMember(const char* pSignature, KHook::Return<RETURN> (*cbPre)(CLASS*, ARGS...), nullptr_t cbPost)
{
	return std::make_unique<CSigHookMember<CLASS, RETURN, ARGS...>>(pSignature, cbPre, nullptr);
}

template <typename CLASS, typename RETURN, typename... ARGS>
auto MakeSigHookMember(const char* pSignature, nullptr_t cbPre, KHook::Return<RETURN> (*cbPost)(CLASS*, ARGS...))
{
	return std::make_unique<CSigHookMember<CLASS, RETURN, ARGS...>>(pSignature, nullptr, cbPost);
}

template <typename CLASS, typename RETURN, typename... ARGS>
auto MakeSigHookMember(const char* pSignature, KHook::Return<RETURN> (*cbPre)(CLASS*, ARGS...), KHook::Return<RETURN> (*cbPost)(CLASS*, ARGS...))
{
	return std::make_unique<CSigHookMember<CLASS, RETURN, ARGS...>>(pSignature, cbPre, cbPost);
}

#define SIG_HOOK_MEMBER(name, pre, post) \
	static auto hook##name = MakeSigHookMember(#name, pre, post)
