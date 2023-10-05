#pragma once
#include <cstdint>
#include "metamod_oslink.h"

#if defined(_WIN32)
#define FASTCALL __fastcall
#define THISCALL __thiscall
#else
#define FASTCALL __attribute__((fastcall))
#define THISCALL
#define strtok_s strtok_r
#endif

struct Module
{
#ifndef _WIN32
	void* pHandle;
#endif
	uint8_t* pBase;
	unsigned int nSize;
};

#ifndef _WIN32
int GetModuleInformation(const char* name, void** base, size_t* length);
#endif

#ifdef _WIN32
#define MODULE_PREFIX ""
#define MODULE_EXT ".dll"
#else
#define MODULE_PREFIX "lib"
#define MODULE_EXT ".so"
#endif

void Plat_WriteMemory(void* pPatchAddress, uint8_t *pPatch, int iPatchSize);