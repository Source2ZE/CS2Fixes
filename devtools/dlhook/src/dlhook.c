#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>

void *dlopen(const char *filename, int flags)
{
	printf("CALLED DLOPEN!!\n");
	typedef void *(*dlopen_t)(const char *filename, int flags);
	static dlopen_t func;

	if(!func)
			func = (dlopen_t)dlsym(RTLD_NEXT, "dlopen");

	return(func(filename, flags & ~RTLD_DEEPBIND));
}