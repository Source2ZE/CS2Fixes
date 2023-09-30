#include "plat.h"

void Plat_WriteMemory(void* pPatchAddress, uint8_t* pPatch, int iPatchSize)
{
	WriteProcessMemory(GetCurrentProcess(), pPatchAddress, (void*)pPatch, iPatchSize, nullptr);
}