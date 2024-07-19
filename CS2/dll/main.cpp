#include "main.h"

//
// current components:
// - usermode input inject detection
//
// missing components:
// - validating mouse packets to game camera (this would cause harm for internal cheats)
//

std::vector<DEVICE_INFO> get_input_devices(void);
QWORD FindPattern(QWORD base, unsigned char* pattern, unsigned char* mask);

namespace globals
{
	std::vector<DEVICE_INFO> device_list;
	WNDPROC game_window_proc = 0;
	DWORD invalid_cnt = 0;
}

//
// missing component: validating incoming input to game camera
//
static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	//
	// detect injected messages
	// https://stackoverflow.com/questions/69193249/how-to-distinguish-mouse-and-touchpad-events-using-getcurrentinputmessagesource
	//
	if ((uMsg >= WM_MOUSEFIRST && uMsg <= WM_MOUSELAST) || (uMsg >= WM_KEYFIRST && uMsg <= WM_KEYLAST) || (uMsg >= WM_TOUCH && uMsg <= WM_POINTERWHEEL))
	{
		INPUT_MESSAGE_SOURCE src;
		if (GetCurrentInputMessageSource(&src))
		{
			if (src.originId == IMO_INJECTED)
			{
				LOG("invalid mouse input detected %d\n", ++globals::invalid_cnt);
				uMsg = WM_NULL;
			}
		}
	}
	return CallWindowProc(globals::game_window_proc, hwnd, uMsg, wParam, lParam );
}

__int64 (__fastcall *oWIN_HandleRawMouseInput)(QWORD timestamp, QWORD param1, HANDLE hDevice, RAWMOUSE *rawmouse);
__int64 __fastcall WIN_HandleRawMouseInput(QWORD timestamp, QWORD param1, HANDLE hDevice, RAWMOUSE *rawmouse)
{
	static DEVICE_INFO new_device{};

	//
	// block all non used devices
	//
	if (globals::device_list.size() > 1)
	{
		DEVICE_INFO primary_dev{};
		UINT64      max_calls = 0;

		for (DEVICE_INFO &dev : globals::device_list)
		{
			if (dev.total_calls > max_calls)
			{
				max_calls   = dev.total_calls;
				primary_dev = dev;
			}
		}

		if (max_calls > 50)
		{
			primary_dev.timestamp = timestamp;
			globals::device_list.clear();
			globals::device_list.push_back(primary_dev);
			LOG("primary input device has been now selected\n");
		}
	}


	//
	// validate incoming rawinput device
	//
	BOOLEAN found = 0;
	for (DEVICE_INFO& dev : globals::device_list)
	{
		if (dev.handle == hDevice)
		{
			found = 1;
			dev.total_calls++;
			if (timestamp - dev.timestamp < 500000) // if latency is less than 0.5ms. this is tested with 1000hz mouse.
			{
				LOG("Device: 0x%llx, timestamp: %lld, delta: [%lld]\n", (QWORD)hDevice, timestamp, timestamp - dev.timestamp);
			}
			dev.timestamp = timestamp;
			break;
		}
	}

	if (found == 0)
	{
		LOG("invalid mouse input detected %d\n", ++globals::invalid_cnt);
		memset(rawmouse, 0, sizeof(RAWMOUSE));

		if (new_device.handle == hDevice)
		{
			new_device.total_calls++;
		}
		else
		{
			if (new_device.handle)
			{
				new_device.total_calls = 0;
			}
		}

		//
		// initialize new device if invalid cnt reaches 150
		// - in case player decide to change mouse mid game
		// - this function is going to change the primary device
		//
		if (new_device.total_calls > 150)
		{
			std::vector<DEVICE_INFO> devices = get_input_devices();
			for (DEVICE_INFO &device : devices)
			{
				if (device.handle == hDevice)
				{
					//
					// select new primary device
					//
					device.timestamp = timestamp;
					globals::device_list.clear();
					globals::device_list.push_back(device);
					new_device.total_calls = 0;
					LOG("primary input device has been now selected\n");
				}
			}
		}
		new_device.handle = hDevice;
	}
	else
	{
		new_device.total_calls = 0;
	}
	return oWIN_HandleRawMouseInput(timestamp, param1, hDevice, rawmouse);
}

static void MainThread(void)
{
	HWND window = 0;
	while (1)
	{
		window = FindWindowA("SDL_app", "Counter-Strike 2");

		if (window != 0)
		{
			break;
		}

		Sleep(100);
	}
	globals::device_list = get_input_devices();

	QWORD sdl = 0;
	while (!(sdl = (QWORD)GetModuleHandleA("SDL3.dll"))) Sleep(100);

	// sdl + 0xE5B40;
	QWORD sdl_rawinput = FindPattern(sdl, (PBYTE)"\x48\x89\x4C\x24\x08\x53\x55\x56\x41\x56\x48\x83\xEC\x68\x83\xBA", (PBYTE)"xxxxxxxxxxxxxxxx");
	if (sdl_rawinput == 0)
	{
		LOG("plugin is outdated\n");
		return;
	}

	MH_Initialize();
	MH_CreateHook((LPVOID)sdl_rawinput, &WIN_HandleRawMouseInput, (LPVOID*)&oWIN_HandleRawMouseInput);
	MH_EnableHook((LPVOID)sdl_rawinput);

	globals::game_window_proc = (WNDPROC)SetWindowLongPtrW(window, (-4), (LONG_PTR)WindowProc);

	LOG("plugin is installed\n");
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
		// LOG("%ws\n", NotificationData->Loaded.BaseDllName->Buffer);
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

std::vector<DEVICE_INFO> get_input_devices(void)
{
	std::vector<DEVICE_INFO> devices;


	//
	// get number of devices
	//
	UINT device_count = 0;
	GetRawInputDeviceList(0, &device_count, sizeof(RAWINPUTDEVICELIST));


	//
	// allocate space for device list
	//
	RAWINPUTDEVICELIST *device_list = (RAWINPUTDEVICELIST *)malloc(sizeof(RAWINPUTDEVICELIST) * device_count);


	//
	// get list of input devices
	//
	GetRawInputDeviceList(device_list, &device_count, sizeof(RAWINPUTDEVICELIST));


	for (UINT i = 0; i < device_count; i++)
	{
		//
		// skip non mouse devices ; we can adjust this in future
		//
		if (device_list[i].dwType != RIM_TYPEMOUSE)
		{
			continue;
		}


		//
		// add new device to our dynamic list
		//
		DEVICE_INFO info{};
		info.handle = device_list[i].hDevice;
		devices.push_back(info);
	}


	//
	// free resources
	//
	free(device_list);


	return devices;
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

