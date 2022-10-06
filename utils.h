#ifndef UTILS_H
#define UTILS_H


#include <windows.h>

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
	BOOL  Hook(PVOID dst, PVOID src);
}

#endif /* UTILS_H */

