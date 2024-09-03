#include "main.h"
#include <intrin.h>

QWORD FindPattern(QWORD base, unsigned char* pattern, unsigned char* mask);

namespace globals
{
	QWORD local_player;
}

static void MainThread(void)
{
	LOG("plugin is installed\n");
}

namespace resourcesystem
{
	QWORD get_access_time(unsigned int* junk)
	{
		_mm_lfence();
		QWORD ret = __rdtscp(junk);
		_mm_lfence();
		return ret;
	}


	char *resourcesystem_table_original;
	QWORD resource_system_original;
	QWORD resourcesystem_address;
	char *resource_system;



	#pragma section("PAGE",read,write,nopage)
	__declspec(allocate("PAGE"))
	char entitylist_cached[0x2000]{};

	BOOLEAN trap_set = 0;

	PVOID assemble_func(QWORD original_func)
	{
		unsigned char payload[] =
		{
			0x48, 0xB9, 0x00, 0x00, 0x95, 0xB9, 0xF7, 0x7F, 0x00, 0x00, 0xFF, 0x25, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
		};

		// movabs rcx, clone_vtable   ; we redirect vtable .data to somewhere else
		// jmp    QWORD PTR [rip+0x0] ; jmp to original vtable_func

		*(QWORD*)(payload + 0x02)        = (QWORD)resourcesystem_table_original;
		*(QWORD*)(payload + 0x0A + 0x06) = original_func;

		PVOID mem = VirtualAlloc(0, sizeof(payload), MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
		memcpy(mem, payload, sizeof(payload));
		return mem;
	}

	void __fastcall initialize_entitylist(QWORD rcx, QWORD rdx)
	{
		// entitylist_cached = (QWORD)VirtualAlloc(0, 0x2000, MEM_COMMIT, PAGE_READWRITE);  //(QWORD)malloc(0x1570);
		memcpy((void*)entitylist_cached, (const void*)rdx, 0x1570);
		*(QWORD*)(resourcesystem_table_original + 0x58) = rdx;
		*(QWORD*)(rcx + 0x58) = (QWORD)entitylist_cached;
		trap_set = 1;

		//
		// unhook
		//
		*(QWORD*)(resource_system + 0x110) = (QWORD)assemble_func(*(QWORD*)(resource_system_original + 0x110));
	}

	BOOL update_entitylist(void)
	{
		static UINT64 earlier_ms = 0;


		UINT64 ms = std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::system_clock::now().time_since_epoch()
		).count();


		if (ms - earlier_ms > 5000)
		{
			//
			// updating entitylist
			//
			memcpy((void*)entitylist_cached, (const void*)*(QWORD*)(resourcesystem_table_original + 0x58), 0x1570);
			earlier_ms = ms;

			_mm_clflush((const void *)(entitylist_cached + 0x10));

			return 1;
		}

		return 0;
	}

	BOOL is_alive(void)
	{
		if (!globals::local_player)
			return 0;

		QWORD controller = *(QWORD*)(globals::local_player);
		if (!controller)
			return 0;

		return *(BYTE*)(controller + 0x7F4) == 1;
	}

	void trap_thread(void)
	{
		BOOL  task  = 0;
		unsigned int junk  = 0;

		int   access_counter=0;


		while (1)
		{
			if (!trap_set)
			{
				Sleep(1);
				continue;
			}

			if (update_entitylist())
			{
				if (is_alive())
				{
					LOG("past 5 seconds, total of %ld memory accesses\n", access_counter);
				}
				access_counter = 0;
				continue;
			}

			_mm_clflush((const void *)(entitylist_cached + 0x10));

			Sleep(1);

			QWORD t1 = get_access_time(&junk);
			volatile DWORD not_used = *(DWORD*)(entitylist_cached + 0x10);
			QWORD t2 = get_access_time(&junk) - t1;

			if (t2 < 315)
			{
				access_counter++;
			}
		}
	}

	void initialize(QWORD engine2)
	{
		resource_system = (char *)malloc(0x168);


		resourcesystem_address =
			FindPattern(engine2,
				(PBYTE)"\x48\x89\x43\x40\x48\x8B\x05\x00\x00\x00\x00",
				(PBYTE)"xxxxxxx????"
				);

		if (!resourcesystem_address)
		{
			ExitProcess(0);
			return;
		}

		resourcesystem_address = resourcesystem_address + 0x04;
		resourcesystem_address = (resourcesystem_address + 7) + *(int*)(resourcesystem_address + 3);


		resource_system_original = *(QWORD*)resourcesystem_address;

		memcpy(resource_system, (const void *)resource_system_original, 0x168);

		//
		// hook pointer set
		//
		*(QWORD*)resourcesystem_address = (QWORD)resource_system;

		resourcesystem_table_original = (char *)malloc(0x60);
		memcpy(resourcesystem_table_original, (void*)resourcesystem_address, 0x60);
		
		for (QWORD i = 0; i < 0x160; i+= 8)
		{
			*(QWORD*)(resource_system + i) = (QWORD)assemble_func(*(QWORD*)(resource_system_original + i));
		}

		*(QWORD*)(resource_system + 0x110) = (QWORD)initialize_entitylist;


		CreateThread(0, 0, (LPTHREAD_START_ROUTINE)trap_thread, 0, 0, 0);

		LOG("memory monitor is ready\n");
		
		LOG("anti-cheat is running\n");
	}
}

VOID CALLBACK DllCallback(
  _In_      ULONG NotificationReason,
  _In_      PCLDR_DLL_NOTIFICATION_DATA NotificationData,
  _In_opt_  PVOID Context
)
{
	UNREFERENCED_PARAMETER(Context);
	if (NotificationReason == LDR_DLL_NOTIFICATION_REASON_LOADED)
	{
		if (!wcscmp(NotificationData->Loaded.BaseDllName->Buffer, L"client.dll"))
		{
			globals::local_player =
				FindPattern((QWORD)NotificationData->Loaded.DllBase,
					(PBYTE)"\x48\x83\x3D\x00\x00\x00\x00\x00\x0F\x95\xC0\xC3", (PBYTE)"xxx????xxxxx");

			if (globals::local_player)
				globals::local_player = (globals::local_player + 8) + *(int*)(globals::local_player + 3);
		}

		if (!wcscmp(NotificationData->Loaded.BaseDllName->Buffer, L"engine2.dll"))
		{
			resourcesystem::initialize((QWORD)NotificationData->Loaded.DllBase);

			/*
			LOG("Press F10 key to continue . . .\n");
			while (!GetAsyncKeyState(VK_F10))
			{
				Sleep(1);
			}
			*/
		}
	}
	else if (NotificationReason == LDR_DLL_NOTIFICATION_REASON_UNLOADED)
	{
	}
}

BOOL WINAPI DllMain(HMODULE hModule, DWORD dwReason, LPVOID Reserved)
{
	if (dwReason == DLL_PROCESS_ATTACH)
	{
		AllocConsole();
		freopen("CONOUT$", "w", stdout);
		CloseHandle(CreateThread(0, 0, (LPTHREAD_START_ROUTINE)MainThread, 0, 0, 0));
		NTSTATUS (NTAPI *LdrRegisterDllNotification)(
		  _In_     ULONG                          Flags,
		  _In_     PLDR_DLL_NOTIFICATION_FUNCTION NotificationFunction,
		  _In_opt_ PVOID                          Context,
		  _Out_    PVOID                          *Cookie
		);
		VOID *dll_callback_handle = 0;
		*(void**)&LdrRegisterDllNotification = (void*)GetProcAddress(LoadLibraryA("ntdll.dll"), "LdrRegisterDllNotification");
		LdrRegisterDllNotification(0, DllCallback, 0, &dll_callback_handle);
	}
	return 1;
}

static int CheckMask(unsigned char* base, unsigned char* pattern, unsigned char* mask)
{
	for (; *mask; ++base, ++pattern, ++mask)
		if (*mask == 'x' && *base != *pattern)
			return 0;
	return 1;
}

void *FindPatternEx(unsigned char* base, QWORD size, unsigned char* pattern, unsigned char* mask)
{
	size -= strlen((const char *)mask);
	for (QWORD i = 0; i <= size; ++i) {
		void* addr = &base[i];
		if (CheckMask((unsigned char *)addr, pattern, mask))
			return addr;
	}
	return 0;
}

QWORD FindPattern(QWORD base, unsigned char* pattern, unsigned char* mask)
{
	if (base == 0)
	{
		return 0;
	}

	QWORD nt_header = (QWORD)*(DWORD*)(base + 0x03C) + base;
	if (nt_header == base)
	{
		return 0;
	}

	WORD machine = *(WORD*)(nt_header + 0x4);
	QWORD section_header = machine == 0x8664 ?
		nt_header + 0x0108 :
		nt_header + 0x00F8;

	for (WORD i = 0; i < *(WORD*)(nt_header + 0x06); i++) {
		QWORD section = section_header + ((QWORD)i * 40);

		DWORD section_characteristics = *(DWORD*)(section + 0x24);

		if (section_characteristics & 0x00000020 && !(section_characteristics & 0x02000000))
		{
			QWORD virtual_address = base + (QWORD)*(DWORD*)(section + 0x0C);
			DWORD virtual_size = *(DWORD*)(section + 0x08);

			void *found_pattern = FindPatternEx( (unsigned char*)virtual_address, virtual_size, pattern, mask);
			if (found_pattern)
			{
				return (QWORD)found_pattern;
			}
		}
	}
	return 0;
}

