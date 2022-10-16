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
static UINT (WINAPI *NtUserGetRawInputData)(
	_In_ HRAWINPUT hRawInput,
	_In_ UINT uiCommand,
	_Out_writes_bytes_to_opt_(*pcbSize, return) LPVOID pData,
	_Inout_ PUINT pcbSize,
	_In_ UINT cbSizeHeader);


HANDLE mouse_device = 0;

UINT
WINAPI
GetRawInputDataHook(
    _In_ HRAWINPUT hRawInput,
    _In_ UINT uiCommand,
    _Out_writes_bytes_to_opt_(*pcbSize, return) LPVOID pData,
    _Inout_ PUINT pcbSize,
    _In_ UINT cbSizeHeader)
{
	UINT ret = NtUserGetRawInputData(hRawInput, uiCommand, pData, pcbSize, cbSizeHeader);
	if (!ret)
	{
		return 0;
	}

	RAWINPUT *data = (RAWINPUT*)pData;

	//
	// block mouse_event
	//
	if (data->header.hDevice != mouse_device)
	{
		//
		// todo: initialize legit mouse_device better way
		//
		if (mouse_device == 0)
		{
			mouse_device = data->header.hDevice;
		}
		else
		{
			if (pcbSize)
			{
				memset(pData, 0, *pcbSize);
			}
			return 0;
		}
	}

	//
	// block MouseServiceCallbackMeme
	//
	if (data->data.hid.dwCount > 1)
	{
		//
		// todo: disconnect player from server
		//
		if (pcbSize)
		{
			memset(pData, 0, *pcbSize);
		}

		//
		// add false positive counter here before kicking the player
		// because when game is frozen, this is not accurate
		//
		printf("[CSGO-AC] MouseClassServiceCallbackMeme: disconnecting player from the server...\n");

		return 0;
	}

	//
	// blocks EC viewangle method
	//
	// client::initialize();

	return ret;
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
	*(PVOID*)&NtUserGetRawInputData = (PVOID)GetProcAddress(GetModuleHandleA("win32u.dll"), "NtUserGetRawInputData");
	if (!utils::hook(
		(PVOID)GetRawInputData,
		GetRawInputDataHook
	))
	{
		*(int*)(0)=0;
	}
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

