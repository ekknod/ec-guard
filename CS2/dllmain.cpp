#define _CRT_SECURE_NO_WARNINGS
#include "utils.h"
#include <stdio.h>

#ifndef __linux__

HANDLE  mouse_device     = 0;
WNDPROC game_window_proc = 0;
DWORD   invalid_cnt      = 0;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	//
	// mouse_event detection
	//
	{
		if (uMsg == WM_INPUT)
		{
			RAWINPUT data{};
			UINT size = sizeof(RAWINPUT);
			GetRawInputData((HRAWINPUT)lParam, RID_INPUT, &data, &size, sizeof(RAWINPUTHEADER));

			if (data.header.dwType == RIM_TYPEMOUSE)
			{
				if (data.header.hDevice != mouse_device)
				{
					//
					// todo: initialize legit mouse_device better way
					//
					if (mouse_device == 0)
					{
						mouse_device = data.header.hDevice;
					}
					else
					{
						invalid_cnt++;

						printf("[CSGO-AC.dll] invalid mouse input detected %d\n", invalid_cnt);

						uMsg = WM_NULL;
					}
				}
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
					printf("[CSGO-AC.dll] invalid mouse input detected %d\n", invalid_cnt);
					uMsg = WM_NULL;
				}
			}
		}
	}
	return CallWindowProc(game_window_proc, hwnd, uMsg, wParam, lParam );
}

#endif

BOOL DllOnLoad(void)
{
#ifndef __linux__
	AllocConsole();
	freopen("CONOUT$", "w", stdout);
#endif
#ifndef __linux__
	game_window_proc = (WNDPROC)SetWindowLongPtrW(FindWindowA("SDL_app", "Counter-Strike 2"), (-4), (LONG_PTR)WindowProc);
#endif
	printf("[CS2-AC.dll] plugin is installed\n");
	return 1;
}

#ifdef __linux__
int __attribute__((constructor)) DllMain(void)
{
	if (DllOnLoad())
		return 0;
	return -1;
}

void __attribute__((destructor)) DllUnload(void)
{
}

#else

BOOL WINAPI DllMain(HMODULE hModule, DWORD dwReason, LPVOID Reserved)
{
	BOOL ret = 0;
	if (dwReason == DLL_PROCESS_ATTACH)
	{
		ret = DllOnLoad();
	}
	return ret;
}
#endif

