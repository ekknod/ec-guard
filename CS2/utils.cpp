#include "utils.h"

#ifdef __linux__
#include <sys/types.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <string.h>
#include <sys/mman.h>
#endif

DWORD utils::get_current_thread_id(void)
{
#ifdef __linux__
	return syscall(SYS_gettid);
#else

	_TEB * teb = NtCurrentTeb();
#ifdef _WIN32
	return *(DWORD*)((ULONG_PTR)teb + 0x24);
#else
	return *(DWORD*)((ULONG_PTR)teb + 0x48);
#endif

#endif
}

PVOID utils::get_interface_factory(PCSTR dll_name)
{
#ifdef __linux__
	void *handle = dlopen(dll_name, RTLD_LAZY);
	if (!handle)
	{
		return NULL;
	}

	void *interface_factory = dlsym(handle, "CreateInterface");
	if (!interface_factory)
	{
		return NULL;
	}

	dlclose(handle);

	return interface_factory;
#else
	HMODULE target_dll = GetModuleHandleA(dll_name);

	if (target_dll == 0)
		return 0;

	
	return GetProcAddress(target_dll, "CreateInterface");
#endif
}

PVOID utils::get_interface(PVOID interface_factory, PCSTR interface_name)
{
	typedef void* (*CreateInterfaceFn)(const char *pName, int *pReturnCode);
	CreateInterfaceFn target_routine = (CreateInterfaceFn)interface_factory;
	return target_routine(interface_name, 0);
}

PVOID utils::get_interface_function(PVOID interface_address, int function_index)
{
	return *(PVOID*)(*(ULONG_PTR*)(interface_address) + function_index * sizeof(ULONG_PTR));
}

#ifdef __linux__
static void ProtectAddr(void* addr, int prot)
{
	long pagesize = sysconf(_SC_PAGESIZE);
	void* address = (void *)((long)(ULONG_PTR)addr & ~(pagesize - 1));
	mprotect(address, sizeof(address), prot);
}
#endif

BOOL utils::MemCopy(PVOID dst, PVOID src, ULONG_PTR length)
{
	#ifdef __linux__
		ProtectAddr(dst, PROT_READ | PROT_WRITE | PROT_EXEC);
		memcpy(dst, src, length);
		ProtectAddr(dst, PROT_READ | PROT_EXEC);
		return 1;
	#else
	DWORD old_protection;
	if (!VirtualProtect(dst, length, PAGE_EXECUTE_READWRITE, &old_protection))
		return 0;

	memcpy(dst, src, length);

	VirtualProtect(dst, length, old_protection, &old_protection);

	return 1;
	#endif
}

BOOL utils::hook(PVOID dst, PVOID src)
{
	if (src == 0)
	{
		return 0;
	}

#if _WIN32
	unsigned char bytes[] = { 0x68, 0x90, 0x29, 0x0B, 0x10, 0xC3 } ;
	*(ULONG_PTR*)(bytes + 1) = (ULONG_PTR)src;
	return MemCopy(dst, bytes, sizeof(bytes));
#else
	unsigned char bytes[] = { 0xFF, 0x25, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
	*(ULONG_PTR*)(bytes + 6) = (ULONG_PTR)src;
	return MemCopy(dst, bytes, sizeof(bytes));
#endif
}

