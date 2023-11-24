#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>
#include <vector>
#include <chrono>


#define LOG(...) printf("[ec-guard.dll] " __VA_ARGS__)


typedef struct {
	HANDLE handle;
	UINT64 total_calls;
} RAWINPUT_DEVICE ;
std::vector<RAWINPUT_DEVICE> get_input_devices(void);

std::vector<RAWINPUT_DEVICE> device_list;
WNDPROC game_window_proc = 0;
DWORD   invalid_cnt      = 0;
DWORD   autotrigger_cnt  = 0;
UINT64  timestamp_mup_ms = 0;


static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	//
	// block all non used devices
	//
	if (device_list.size() > 1)
	{
		RAWINPUT_DEVICE primary_mouse{};
		UINT64          max_calls = 0;

		for (RAWINPUT_DEVICE &dev : device_list)
		{
			if (dev.total_calls > max_calls)
			{
				max_calls     = dev.total_calls;
				primary_mouse = dev;
			}
		}

		if (max_calls > 50)
		{
			device_list.clear();
			device_list.push_back(primary_mouse);
			LOG("primary input device has been now selected\n");
		}
	}


	//
	// mouse input manipulation detection
	//
	{
		if (uMsg == WM_INPUT)
		{
			RAWINPUT data{};
			UINT size = sizeof(RAWINPUT);
			GetRawInputData((HRAWINPUT)lParam, RID_INPUT, &data, &size, sizeof(RAWINPUTHEADER));


			if (data.header.dwType != RIM_TYPEMOUSE)
			{
				return CallWindowProc(game_window_proc, hwnd, uMsg, wParam, lParam );
			}


			BOOLEAN found = 0;
			for (RAWINPUT_DEVICE &dev : device_list)
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
				invalid_cnt++;
				LOG("invalid mouse input detected %d\n", invalid_cnt);
				uMsg = WM_NULL;
			}
		}


		//
		// https://stackoverflow.com/questions/69193249/how-to-distinguish-mouse-and-touchpad-events-using-getcurrentinputmessagesource
		//
		if ((uMsg >= WM_MOUSEFIRST && uMsg <= WM_MOUSELAST) || (uMsg >= WM_KEYFIRST && uMsg <= WM_KEYLAST) || (uMsg >= WM_TOUCH && uMsg <= WM_POINTERWHEEL))
		{
			INPUT_MESSAGE_SOURCE src;
			if (GetCurrentInputMessageSource(&src))
			{
				if (src.originId == IMO_INJECTED)
				{
					invalid_cnt++;
					LOG("invalid mouse input detected %d\n", invalid_cnt);
					uMsg = WM_NULL;
				}
			}
		}
	}
	

	//
	// triggerbot detection
	//
	{
		if (uMsg == WM_LBUTTONDOWN)
		{
			if (timestamp_mup_ms)
			{
				UINT64 current_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
					std::chrono::high_resolution_clock::now().time_since_epoch())
					.count();

				UINT64 diff = current_ms - timestamp_mup_ms;
				if (diff <= 15)
				{
					LOG("auto trigger detected %d\n", autotrigger_cnt);
					autotrigger_cnt++;
				}
				timestamp_mup_ms = 0;

			}
		}
		else if (uMsg == WM_LBUTTONUP)
		{
			timestamp_mup_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
				std::chrono::high_resolution_clock::now().time_since_epoch())
				.count();
		}
	}
	return CallWindowProc(game_window_proc, hwnd, uMsg, wParam, lParam );
}

static void AntiCheatRoutine(void)
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
	device_list      = get_input_devices();
	game_window_proc = (WNDPROC)SetWindowLongPtrW(window, (-4), (LONG_PTR)WindowProc);
}

BOOL DllOnLoad(void)
{
	AllocConsole();
	freopen("CONOUT$", "w", stdout);
	CloseHandle(CreateThread(0, 0, (LPTHREAD_START_ROUTINE)AntiCheatRoutine, 0, 0, 0));
	LOG("plugin is installed\n");
	return 1;
}

BOOL WINAPI DllMain(HMODULE hModule, DWORD dwReason, LPVOID Reserved)
{
	BOOL ret = 0;
	if (dwReason == DLL_PROCESS_ATTACH)
	{
		ret = DllOnLoad();
	}
	return ret;
}

std::vector<RAWINPUT_DEVICE> get_input_devices(void)
{
	std::vector<RAWINPUT_DEVICE> devices;


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
		RAWINPUT_DEVICE info{};
		info.handle = device_list[i].hDevice;
		devices.push_back(info);
	}


	//
	// free resources
	//
	free(device_list);


	return devices;
}

