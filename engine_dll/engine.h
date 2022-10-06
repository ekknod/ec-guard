#ifndef ENGINE_H
#define ENGINE_H

#ifdef __linux__
#include <dlfcn.h>
typedef int BOOL;
typedef unsigned long ULONG_PTR;
#else
#include <windows.h>
#endif

namespace engine
{
	BOOL InstallHooks(void);
}

#endif /* ENGINE_H */

