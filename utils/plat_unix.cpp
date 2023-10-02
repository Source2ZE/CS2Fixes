#ifdef __linux__
#include "plat.h"
#include <dlfcn.h>
#include <libgen.h>
#include <stdio.h>
#include <string.h>
#include "sys/mman.h"
#include <locale>

#include "tier0/memdbgon.h"

//returns 0 if successful
int GetModuleInformation(const char *name, void **base, size_t *length)
{
	// this is the only way to do this on linux, lol
	FILE *f = fopen("/proc/self/maps", "r");
	if (!f)
		return 1;
	
	char buf[PATH_MAX+100];
	while (!feof(f))
	{
		if (!fgets(buf, sizeof(buf), f))
			break;
		
		char *tmp = strrchr(buf, '\n');
		if (tmp)
			*tmp = '\0';
		
		char *mapname = strchr(buf, '/');
		if (!mapname)
			continue;
		
		char perm[5];
		unsigned long begin, end;
		sscanf(buf, "%lx-%lx %4s", &begin, &end, perm);
		
		// this used to compare with basename(mapname) but since we're using gameinfo to load metamod we have to use the full path
		// this is because there are 2 libserver.so loaded, metamod's and the actual server
		if (strcmp(mapname, name) == 0 && perm[0] == 'r' && perm[2] == 'x')
		{
			*base = (void*)begin;
			*length = (size_t)end-begin;
			fclose(f);
			return 0;
		}
	}
	
	fclose(f);
	return 2;
}

static int parse_prot(const char* s)
{
	int prot = 0;

	for (; *s; s++)
	{
		switch (*s)
		{
		case '-':
			break;
		case 'r':
			prot |= PROT_READ;
			break;
		case 'w':
			prot |= PROT_WRITE;
			break;
		case 'x':
			prot |= PROT_EXEC;
			break;
		case 's':
			break;
		case 'p':
			break;
		default:
			break;
		}
	}

	return prot;
}

static int get_prot(void* pAddr, size_t nSize)
{
	FILE* f = fopen("/proc/self/maps", "r");

	uintptr_t nAddr = (uintptr_t)pAddr;

	char line[512];
	while (fgets(line, sizeof(line), f))
	{
		char start[16];
		char end[16];
		char prot[16];

		const char* src = line;

		char* dst = start;
		while (*src != '-')
			*dst++ = *src++;
		*dst = 0;

		src++; // skip "-""

		dst = end;
		while (!isspace(*src))
			*dst++ = *src++;
		*dst = 0;

		src++; // skip space

		dst = prot;
		while (!isspace(*src))
			*dst++ = *src++;
		*dst = 0;

		uintptr_t nStart = (uintptr_t)strtoul(start, NULL, 16);
		uintptr_t nEnd = (uintptr_t)strtoul(end, NULL, 16);

		if (nStart < nAddr && nEnd >(nAddr + nSize))
		{
			fclose(f);
			return parse_prot(prot);
		}
	}

	fclose(f);
	return 0;
}

void Plat_WriteMemory(void* pPatchAddress, uint8_t* pPatch, int iPatchSize)
{
	int old_prot = get_prot(pPatchAddress, iPatchSize);

	uintptr_t page_size = sysconf(_SC_PAGESIZE);
	uint8_t* align_addr = (uint8_t*)((uintptr_t)pPatchAddress & ~(page_size - 1));

	uint8_t* end = (uint8_t*)pPatchAddress + iPatchSize;
	uintptr_t align_size = end - align_addr;

	int result = mprotect(align_addr, align_size, PROT_READ | PROT_WRITE);

	memcpy(pPatchAddress, pPatch, iPatchSize);

	result = mprotect(align_addr, align_size, old_prot);
}
#endif

