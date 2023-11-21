#define _CRT_SECURE_NO_WARNINGS

#include "engine.h"
#include "../utils.h"
#include <math.h>
#include <stdio.h>

#ifdef __linux__
#define __fastcall
#endif

namespace hooks
{
	DWORD viewangle_tid = 0;

	//
	// viewangles .data location is CSGO-AC.dll
	// it could be encrypted and decrypted in GetViewAngles/SetViewAngles routine if wanted to.
	//
	static vec3 viewangles{0, -89.0f, 0};
	#ifdef __linux__
	static void __fastcall GetViewAngles(void *, vec3 &va);
	static void __fastcall SetViewAngles(void *, vec3 &va);
	#else
	static void __fastcall GetViewAngles(void *, void *, vec3 &va);
	static void __fastcall SetViewAngles(void *, void *, vec3 &va);
	#endif
}

static float AngleNormalize( float angle )
{
	angle = fmodf(angle, 360.0f);
	if (angle > 180) 
	{
    		angle -= 360;
	}
	if (angle < -180)
	{
    		angle += 360;
	}
	return angle;
}

inline vec3 virtualize_me_decrypt_encrypt(vec3 va)
{
	int data_x = *(int*)&va.x ^ 0xECECECEC;
	int data_y = *(int*)&va.y ^ 0xECECECEC;
	int data_z = *(int*)&va.z;
	return vec3{ *(float*)&data_x, *(float*)&data_y, *(float*)&data_z };
}

#ifdef __linux__
static void __fastcall hooks::GetViewAngles(void *, vec3 &va)
#else
static void __fastcall hooks::GetViewAngles(void *, void *, vec3 &va)
#endif
{
	//
	// check if routine is called outside from main thread
	//
	if (viewangle_tid != utils::get_current_thread_id())
	{
		if (viewangle_tid == 0)
		{
			viewangle_tid = utils::get_current_thread_id();
		}
		else
		{
			return;
		}
	}
	va = virtualize_me_decrypt_encrypt(viewangles);
}

#ifdef __linux__
static void __fastcall hooks::SetViewAngles(void *, vec3 &va)
#else
static void __fastcall hooks::SetViewAngles(void *, void *, vec3 &va)
#endif
{
	//
	// check if routine is called outside from main thread
	//
	if (viewangle_tid != utils::get_current_thread_id())
	{
		if (viewangle_tid == 0)
		{
			viewangle_tid = utils::get_current_thread_id();
		}
		else
		{
			return;
		}
	}

	vec3 stack_va;
	stack_va.x = AngleNormalize(va.x);
	stack_va.y = AngleNormalize(va.y);
	stack_va.z = AngleNormalize(va.z);
	viewangles = virtualize_me_decrypt_encrypt(stack_va);
}

BOOL engine::initialize(void)
{
	#ifdef __linux
	PVOID factory = utils::get_interface_factory("./bin/linux64/engine_client.so");
	#else
	PVOID factory = utils::get_interface_factory("engine.dll");
	#endif
	if (factory == 0)
		return 0;

	PVOID IEngineClient = utils::get_interface(factory, "VEngineClient014");
	if (IEngineClient == 0)
		return 0;
	
	utils::hook(
		utils::get_interface_function(IEngineClient, 18), // GetViewAngles,
		(PVOID)hooks::GetViewAngles
	);
	
	utils::hook(
		utils::get_interface_function(IEngineClient, 19), // SetViewAngles,
		(PVOID)hooks::SetViewAngles
	);
	hooks::viewangles = virtualize_me_decrypt_encrypt(hooks::viewangles);



	printf("[engine.dll] hook success\n");

	return 1;
}

