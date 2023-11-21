#ifndef CLIENT_H
#define CLIENT_H

#ifdef __linux__
#include <dlfcn.h>
typedef int BOOL;
typedef unsigned long ULONG_PTR;
#else
#include <windows.h>
#endif

namespace client
{
	BOOL initialize(void);
}

#endif /* CLIENT_H */

