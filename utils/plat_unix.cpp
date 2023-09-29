#ifdef __linux__
#include "plat.h"
#include <dlfcn.h>
#include <libgen.h>
#include <stdio.h>
#include <string.h>

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
#endif