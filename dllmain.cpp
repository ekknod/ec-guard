/*
 * CSGO-AC project is proof of concept how modern anti-cheats ingame plugin DLL's should work.
 * this would break most External cheats / make it really difficult to find the .data outside of the game.
 * because anti-cheat.dll plugin can be virtualized / data kept encrypted always before it's needed again.
 * 
 * Currently it only has own implementation for ClientState ViewAngles.
 * it could do more, for example. store player data (positions/etc..) at own .DLL.
 * (5EWIN does this for example BoneMatrix/lifeState/bDormant)
 *
 * 
 * credits: 5EWIN/GamersClub Anti-Cheat, they do something like this already.
 */

#define _CRT_SECURE_NO_WARNINGS
#include "client_dll/client.h"
#include "engine_dll/engine.h"
#include "utils.h"
#include <stdio.h>

#ifndef __linux__

HANDLE  mouse_device     = 0;
WNDPROC game_window_proc = NULL;
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	//
	// mouse_event x,y blocking
	// 
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
					return 0;
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
	
	if (!engine::initialize())
	{
		return 0;
	}

#ifndef __linux__
	game_window_proc = (WNDPROC)SetWindowLongPtr(FindWindowA("Valve001", 0), GWL_WNDPROC, (LONG)(LONG_PTR)WindowProc);
#endif

	printf("[CSGO-AC] plugin is installed\n");
	
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

