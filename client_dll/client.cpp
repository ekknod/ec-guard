#include "client.h"
#include "../utils.h"

#include <stdio.h>

namespace hooks
{
	static bool __fastcall C_CSPlayer_CreateMove( void *, void *, float flInputSampleTime, void *pCmd );
}

static PVOID get_client_entity(PVOID entity_list, int index)
{
	index = index + 1;
	index = index + 0xFFFFDFFF;
	index = index + index;
	return *(PVOID*)((ULONG_PTR)entity_list + index * 8);
}

static bool __fastcall hooks::C_CSPlayer_CreateMove( void *, void *, float flInputSampleTime, void *pCmd )
{
	//
	// blocks EC method for getting viewangles
	// original code can't be found in CSGO SDK 2013, so i did not write it here.
	//
	return true;
}

BOOL client::initialize(void)
{
	printf("[client::initialize]\n");
	PVOID factory = utils::get_interface_factory("client.dll");
	if (factory == 0)
	{
		return 0;
	}

	PVOID entity_list = utils::get_interface(factory, "VClientEntityList003");
	if (entity_list == 0)
	{
		return 0;
	}

	PVOID entity = get_client_entity(entity_list, 0);
	if (entity == 0)
	{
		return 0;
	}

	PVOID create_move = utils::get_interface_function(entity, 289);
	if (create_move == 0)
	{
		return 0;
	}

	utils::hook(create_move, hooks::C_CSPlayer_CreateMove);

	printf("[client::initialize] complete\n");

	return 1;
}

