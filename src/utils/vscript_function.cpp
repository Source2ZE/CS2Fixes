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

#include "vscript_function.h"
#include "entityclass.h"
#include "entitysystem.h"

void* GetVScriptFunction(ScriptClassDesc_t* pScriptDesc, const char* pszFuncName)
{
	// https://github.com/Wend4r/sourcesdk/blob/758e43823f02fcd40498d33a42cd93243258fe1e/public/vscript/ivscript.h#L280
	struct ScriptFunctionBindingCurrent_t
	{
		ScriptFuncDescriptor_t m_desc;
		ScriptClassDesc_t* m_pClassDesc;
		ScriptBindingFunc_t m_pfnBinding;
		void* m_pFunction;
		ScriptFuncBindingFlags_t m_flags;
	};

	const auto& functionBindings = *reinterpret_cast<CUtlVector<ScriptFunctionBindingCurrent_t>*>(&pScriptDesc->m_FunctionBindings);
	FOR_EACH_VEC(functionBindings, i)
	{
		auto& functionBinding = functionBindings.Element(i);
		if (V_strcmp(functionBinding.m_desc.m_pszScriptName, pszFuncName) != 0)
			continue;

		return functionBinding.m_pFunction;
	}

	return nullptr;
}
