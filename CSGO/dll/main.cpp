#include "main.h"

//
// current components:
// - usermode input inject detection
//
// missing components:
// - validating mouse packets to game camera (this would cause harm for internal cheats)
// - .data encryption/decryption (block external/DMA cheats)
//

std::vector<DEVICE_INFO> get_input_devices(void);

namespace globals
{
	std::vector<DEVICE_INFO> device_list;
	WNDPROC game_window_proc = 0;
}

//
// missing component: validating incoming input to game camera
//
static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static DWORD  invalid_cnt     = 0;
	static DWORD  autotrigger_cnt = 0;
	static UINT64 timestamp_left  = 0;


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
			LOG("invalid mouse input detected %d\n", ++invalid_cnt);
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
				LOG("invalid mouse input detected %d\n", ++invalid_cnt);
				uMsg = WM_NULL;
			}
		}
	}
	

	//
	// triggerbot detection
	//
	if (uMsg == WM_LBUTTONDOWN)
	{
		if (timestamp_left)
		{
			UINT64 current_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
				std::chrono::high_resolution_clock::now().time_since_epoch())
				.count();

			UINT64 diff = current_ms - timestamp_left;
			if (diff <= 2)
			{
				LOG("auto trigger detected [ms: %lld] %d\n", diff, autotrigger_cnt++);
			}
			timestamp_left = 0;

		}
	}
	else if (uMsg == WM_LBUTTONUP)
	{
		timestamp_left = std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::high_resolution_clock::now().time_since_epoch())
			.count();
	}
	return CallWindowProc(globals::game_window_proc, hwnd, uMsg, wParam, lParam );
}

static void MainThread(void)
{
	HWND window = 0;
	while (1)
	{
		window = FindWindowA("Valve001", 0);

		if (window != 0)
		{
			break;
		}

		Sleep(100);
	}
	globals::device_list      = get_input_devices();
	globals::game_window_proc = (WNDPROC)SetWindowLongPtrW(window, GWL_WNDPROC, (LONG)WindowProc);
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
		LOG("%ws\n", NotificationData->Loaded.BaseDllName->Buffer);
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

