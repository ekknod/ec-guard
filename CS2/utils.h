#ifndef UTILS_H
#define UTILS_H


#ifdef __linux__
#include <dlfcn.h>
typedef int BOOL;
typedef unsigned long ULONG_PTR;
typedef void *PVOID;
typedef const char *PCSTR;
typedef unsigned int DWORD;
#else
#include <windows.h>
#endif

typedef struct 
{
	float x,y,z;
} vec3 ;

namespace utils
{
	DWORD get_current_thread_id(void);

	PVOID get_interface_factory(PCSTR dll_name);
	PVOID get_interface(PVOID interface_factory, PCSTR interface_name);
	PVOID get_interface_function(PVOID interface_address, int function_index);


	BOOL  MemCopy(PVOID dst, PVOID src, ULONG_PTR length);
	BOOL  hook(PVOID dst, PVOID src);
}

#endif /* UTILS_H */

