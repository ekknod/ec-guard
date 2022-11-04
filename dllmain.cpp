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
WNDPROC game_window_proc = 0;
DWORD   invalid_cnt      = 0;

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
					invalid_cnt++;

					printf("[CSGO-AC] invalid mouse input detected %d\n", invalid_cnt);

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
	if (!client::initialize())
	{
		printf("[-] join server before installing the plugin\n");
		return 0;
	}
	game_window_proc = (WNDPROC)SetWindowLongPtrW(FindWindowA("Valve001", 0), GWL_WNDPROC, (LONG)WindowProc);
#endif
	
	if (!engine::initialize())
	{
		return 0;
	}

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

