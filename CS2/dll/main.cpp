#include "main.h"

//
// current components:
// - usermode input inject detection
//
// missing components:
// - validating mouse packets to game camera (this would cause harm for internal cheats)
//

std::vector<DEVICE_INFO> get_input_devices(void);

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
			globals::device_list.clear();
			globals::device_list.push_back(primary_dev);
			LOG("primary input device has been now selected\n");
		}
	}


	//
	// validate incoming rawinput device
	//
	if (uMsg == WM_INPUT)
	{
		RAWINPUT data{};
		UINT size = sizeof(RAWINPUT);
		GetRawInputData((HRAWINPUT)lParam, RID_INPUT, &data, &size, sizeof(RAWINPUTHEADER));


		if (data.header.dwType != RIM_TYPEMOUSE)
		{
			return CallWindowProc(globals::game_window_proc, hwnd, uMsg, wParam, lParam );
		}


		BOOLEAN found = 0;
		for (DEVICE_INFO &dev : globals::device_list)
		{
			if (dev.handle == data.header.hDevice)
			{
				found = 1;
				dev.total_calls++;
				break;
			}
		}


		if (found == 0)
		{
			LOG("invalid mouse input detected %d\n", ++globals::invalid_cnt);
			uMsg = WM_NULL;
		}
	}


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
			break;
		}
	}

	if (found == 0)
	{
		LOG("invalid mouse input detected %d\n", ++globals::invalid_cnt);
		memset(rawmouse, 0, sizeof(RAWMOUSE));
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
	globals::device_list      = get_input_devices();
	globals::game_window_proc = (WNDPROC)SetWindowLongPtrW(window, (-4), (LONG_PTR)WindowProc);

	QWORD sdl = 0;
	while (!(sdl = (QWORD)GetModuleHandleA("SDL3.dll"))) Sleep(100);

	//
	// hardcoded, just wanted to write this quick
	//
	QWORD sdl_rawinput = sdl + 0xE5B40;


	MH_Initialize();
	MH_CreateHook((LPVOID)sdl_rawinput, &WIN_HandleRawMouseInput, (LPVOID*)&oWIN_HandleRawMouseInput);
	MH_EnableHook((LPVOID)sdl_rawinput);

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

