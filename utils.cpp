#include "utils.h"

DWORD utils::get_current_thread_id(void)
{
	_TEB * teb = NtCurrentTeb();

#ifdef _WIN32
	return *(DWORD*)((ULONG_PTR)teb + 0x24);
#else
	return *(DWORD*)((ULONG_PTR)teb + 0x48);
#endif
}

PVOID utils::get_interface_factory(PCSTR dll_name)
{
	HMODULE target_dll = GetModuleHandleA(dll_name);

	if (target_dll == 0)
		return 0;

	
	return GetProcAddress(target_dll, "CreateInterface");
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

BOOL utils::MemCopy(PVOID dst, PVOID src, ULONG_PTR length)
{
	DWORD old_protection;
	if (!VirtualProtect(dst, length, PAGE_EXECUTE_READWRITE, &old_protection))
		return 0;

	memcpy(dst, src, length);

	VirtualProtect(dst, length, old_protection, &old_protection);

	return 1;
}

BOOL utils::Hook(PVOID dst, PVOID src)
{
#if _WIN32
	unsigned char bytes[] = { 0x68, 0x90, 0x29, 0x0B, 0x10, 0xC3 } ;
	*(ULONG_PTR*)(bytes + 1) = (ULONG_PTR)src;
	return MemCopy(dst, bytes, sizeof(bytes));
#else
	unsigned chat bytes[] = { 0xFF, 0x25, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
	*(ULONG_PTR*)(bytes + 6) = (ULONG_PTR)src;
	return MemCopy(dst, bytes, sizeof(bytes));
#endif
}

