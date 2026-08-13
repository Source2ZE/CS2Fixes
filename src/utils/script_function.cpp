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

#include "script_function.h"
#include "entitysystem.h"
#include "entityclass.h"

void* GetScriptFunction(ScriptClassDesc_t* pScriptDesc, const char* pszFuncName)
{
	if (!pScriptDesc)
		return nullptr;

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
		if (!functionBinding.m_desc.m_pszScriptName || V_strcmp(functionBinding.m_desc.m_pszScriptName, pszFuncName) != 0)
			continue;

		return functionBinding.m_pFunction;
	}

	return nullptr;
}

int ExtractTeleportIndexFromSetOrigin(void* pSetOrigin)
{
	if (!pSetOrigin)
		return -1;

	constexpr int MaxScanSize = 0x40;

	uint8* pCode = static_cast<uint8*>(pSetOrigin);

	for (int i = 0; i + 9 <= MaxScanSize; i++)
	{
		int p = i;
		uint8 rex = 0;

		// Optional x86-64 REX prefix.
		if ((pCode[p] & 0xF0) == 0x40)
			rex = pCode[p++];

		// Pattern #1:
		//
		//   mov  rax, [this]
		//   ...
		//   call [rax + offset]
		//
		// or:
		//
		//   jmp  [rax + offset]
		//
		// FF /2 = CALL
		// FF /4 = JMP
		if (pCode[p] == 0xFF)
		{
			uint8 modrm = pCode[p + 1];

			int mod = modrm >> 6;
			int op = (modrm >> 3) & 7;

			// mod == 2 means [register + disp32].
			// rm == 4 would require a SIB byte.
			if (mod == 2 && (op == 2 || op == 4) && (modrm & 7) != 4)
			{
				int32 disp = *reinterpret_cast<int32*>(pCode + p + 2);

				if (disp >= 0 && (disp & 7) == 0)
					return disp >> 3;
			}
		}

		// Pattern #2:
		//
		//   mov rax, [rax + offset]
		//   jmp rax
		//
		// or:
		//
		//   mov rax, [rax + offset]
		//   call rax
		//
		// Require REX.W because we are loading a 64-bit function pointer.
		if ((rex & 8) && pCode[p] == 0x8B)
		{
			uint8 modrm = pCode[p + 1];

			int mod = modrm >> 6;
			int dst = ((modrm >> 3) & 7) | ((rex & 4) ? 8 : 0);
			int base = (modrm & 7) | ((rex & 1) ? 8 : 0);

			// mov reg, [same_reg + disp32]
			if (mod == 2 && dst == base && (modrm & 7) != 4)
			{
				int32 disp = *reinterpret_cast<int32*>(pCode + p + 2);

				if (disp < 0 || (disp & 7) != 0)
					continue;

				// The MOV instruction is 7 bytes:
				// REX + 8B + ModRM + disp32
				int next = i + 7;

				uint8 nextRex = 0;

				if ((pCode[next] & 0xF0) == 0x40)
					nextRex = pCode[next++];

				if (pCode[next] != 0xFF)
					continue;

				uint8 nextModrm = pCode[next + 1];

				int nextMod = nextModrm >> 6;
				int nextOp = (nextModrm >> 3) & 7;
				int nextReg = (nextModrm & 7) | ((nextRex & 1) ? 8 : 0);

				// mod == 3 means CALL/JMP directly through a register.
				//
				// FF /2 = CALL reg
				// FF /4 = JMP reg
				if (nextMod == 3 && (nextOp == 2 || nextOp == 4) && nextReg == dst)
					return disp >> 3;
			}
		}
	}

	return -1;
}

bool CVScriptTeleportFunction::Initialize(void* pSetOrigin)
{
	int iOffset = ExtractTeleportIndexFromSetOrigin(pSetOrigin);
	if (iOffset == -1)
		return false;

	m_iOffset = iOffset;
	m_bVirtual = true;

	return true;
}
