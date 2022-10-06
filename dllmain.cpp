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
#include "engine_dll/engine.h"
#include <stdio.h>

BOOL DllOnLoad(void)
{
	AllocConsole();
	freopen("CONOUT$", "w", stdout);

	if (!engine::InstallHooks())
	{
		return 0;
	}

	/*
	if (!client::InstallHooks())
	{
		return 0;
	}
	*/

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

