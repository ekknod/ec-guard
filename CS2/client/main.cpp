#include <windows.h>
#include <stdio.h>
#include <TlHelp32.h>
#include <string>
#include <filesystem>

#define DEBUG
#define LOG(...) printf("[ec-guard.exe] " __VA_ARGS__)
#define TARGET_GAME "cs2.exe"
#define TARGET_DLL  "ec-guard.dll"






typedef enum
{
	NotRunning = 0,
	RunningWithoutAC = 1,
	Running = 2
} GameState ;

typedef struct
{
	DWORD       pid;
	std::string path;
} PROCESS_INFO ;

BOOL        load_library(HANDLE process, std::string dll_path);
BOOL        get_process_info(PCSTR process_name, PROCESS_INFO *info);
DWORD       get_process_id(PCSTR process_name);
GameState   get_game_state(PCSTR process_name, PCSTR dll_name);
BOOL        terminate_process(PCSTR process_name);
std::string get_process_cmd(HANDLE process_handle, std::string path);

int main(void)
{
	char buffer[260]{};
	GetCurrentDirectoryA(260, buffer);
	std::string dll_path = buffer + std::string("\\") + std::string(TARGET_DLL);
	if (!std::filesystem::exists(dll_path))
	{
		LOG("Anti-Cheat file is missing: %s\n", dll_path.c_str());
		return 0;
	}

	GameState state = get_game_state(TARGET_GAME, TARGET_DLL);

	if (state == GameState::Running)
	{
		LOG("is already running\n");
		return getchar();
	}

	else if (state == GameState::RunningWithoutAC)
	{
		LOG("please close the game before starting Anti-Cheat\n");
		while (get_process_id(TARGET_GAME))
			Sleep(100);
	}

	LOG("Anti-Cheat is started\n");

	LOG("Waiting for the game...\n");

	PROCESS_INFO info{};

	while (!get_process_info(TARGET_GAME, &info))
	{
		Sleep(100);
	}

	HANDLE process_handle = OpenProcess(PROCESS_ALL_ACCESS, 0, info.pid);
	
	//
	// get command line
	//
	std::string command_line = get_process_cmd(process_handle, info.path);


	while (!TerminateProcess(process_handle, EXIT_SUCCESS))
		break;
	CloseHandle(process_handle);


	PROCESS_INFORMATION pi  = {};
	STARTUPINFOA        si  = {};

	si.cb = sizeof(STARTUPINFO);
	si.dwFlags = STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_NORMAL;

	if (!CreateProcessA(
			0,
			(LPSTR)command_line.c_str(),
			0,
			0,
			0,
			CREATE_SUSPENDED,
			0,
			0,
			&si,
			&pi
		))
	{
		LOG("unknown error 404\n");
		return getchar();
	}


	BOOL status = 0;
	if (!load_library(pi.hProcess, dll_path))
	{
		TerminateProcess(pi.hProcess, 0);
	}
	else
	{
		ResumeThread(pi.hThread);
		status = 1;
	}

	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);

	if (status)
		LOG("Anti-Cheat is successfully launched\n");
	else
		LOG("Anti-Cheat failed to launch\n");

	return 0;
}

#pragma comment(lib, "ntdll.lib")

extern "C" __kernel_entry NTSTATUS NtQueryInformationProcess(
	HANDLE           ProcessHandle,
	ULONG            ProcessInformationClass,
	PVOID            ProcessInformation,
	ULONG            ProcessInformationLength,
	PULONG           ReturnLength
);

ULONG_PTR get_peb(HANDLE process)
{
	ULONG_PTR peb[6]{};

	if (NtQueryInformationProcess(process, 0, &peb, 48, 0) != 0)
	{
		return 0;
	}

	return peb[1];
}

ULONG_PTR get_wow64_process(HANDLE process)
{
	ULONG_PTR wow64_process = 0;

	if (process == 0)
		return wow64_process;

	if (NtQueryInformationProcess(process, 26, &wow64_process, 8, 0) != 0)
	{
		return 0;
	}

	return wow64_process;
}

inline void wcs2str(short *buffer, ULONG_PTR length)
{
	for (ULONG_PTR i = 0; i < length; i++)
	{
		((char*)buffer)[i] = (char)buffer[i];
	}
}

std::string get_process_cmd(HANDLE process_handle, std::string path)
{
	ULONG_PTR peb = get_wow64_process(process_handle);

	ULONG_PTR off_0 = 0, off_1 = 0, rsize = 0;

	if (peb == 0)
	{
		off_0 = 0x20;
		off_1 = 0x70;
		rsize = 8;
		peb   = get_peb(process_handle);
	}
	else
	{
		off_0 = 0x10;
		off_1 = 0x40;
		rsize = 4;
	}

	if (peb == 0)
	{
		return path + " -steam -insecure";
	}

	ULONG_PTR a0 = 0;
	ReadProcessMemory(process_handle, (LPCVOID)(peb + off_0), &a0, rsize, 0);

	a0 = a0 + off_1;

	USHORT len = 0;
	ReadProcessMemory(process_handle, (LPCVOID)(a0  + 0x02), &len, sizeof(USHORT), 0);
	ReadProcessMemory(process_handle, (LPCVOID)(a0  + rsize), &a0, rsize, 0);

	char parameters[512]{};
	ReadProcessMemory(process_handle, (LPCVOID)a0, parameters, len, 0);

	wcs2str((short*)parameters, len);

	return std::string(parameters);
}

BOOL load_library(HANDLE process, std::string dll_path)
{
	BOOL status = 0;
	HANDLE thread_handle = 0;



	PVOID dll_name_address = VirtualAllocEx(process, 0, 0x1000, MEM_COMMIT, PAGE_READWRITE);
		
	if (dll_name_address == 0)
		return 0;

	if (!WriteProcessMemory(process, dll_name_address, dll_path.c_str(), dll_path.size(), 0))
	{
		goto E0;
	}

	thread_handle = CreateRemoteThread(process, NULL, 0, (LPTHREAD_START_ROUTINE)LoadLibraryA, (LPVOID)dll_name_address, 0, NULL);
	if (thread_handle == 0)
	{
		goto E0;
	}

	if (WaitForSingleObject(thread_handle, INFINITE) == WAIT_FAILED)
	{
		goto E2;
	}
	status = 1;
E2:
	CloseHandle(thread_handle);
E0:
	VirtualFreeEx(process, dll_name_address, MAX_PATH, MEM_RELEASE);
	
	return status;
}

DWORD get_process_id(PCSTR process_name)
{
	DWORD pid = 0;
	HANDLE snp = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	PROCESSENTRY32 entry{};
	entry.dwSize = sizeof(PROCESSENTRY32);

	while (Process32Next(snp, &entry))
	{
		CHAR  uc_name[260]{};
		for (int i = 0; i < 260; i++)
		{
			uc_name[i] = (char)entry.szExeFile[i];
		}
		if (!_strcmpi(uc_name, process_name))
		{
			pid = entry.th32ProcessID;
			break;
		}
	}
	CloseHandle(snp);

	return pid;
}

ULONG_PTR get_process_dll(DWORD process_id, PCSTR dll_name)
{
	ULONG_PTR dll = 0;
	HANDLE snp = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, process_id);

	if (snp == 0)
		return 0;

	MODULEENTRY32 entry{};
	entry.dwSize = sizeof(MODULEENTRY32);

	while (Module32Next(snp, &entry))
	{
		CHAR uc_name[256]{};
		for (int i = 0; i < 256; i++)
		{
			uc_name[i] = (char)entry.szModule[i];
		}

		if (!_strcmpi(uc_name, dll_name))
		{
			dll = (ULONG_PTR)entry.hModule;
			break;
		}
	}

	CloseHandle(snp);
	return dll;
}

BOOL get_process_info(PCSTR process_name, PROCESS_INFO *info)
{
	BOOL status = 0;

	DWORD pid = get_process_id(process_name);
	if (pid == 0)
		return 0;

	HANDLE snp = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
	if (snp == 0)
		return 0;

	MODULEENTRY32 entry{};
	entry.dwSize = sizeof(MODULEENTRY32);
	if (Module32First(snp, &entry))
	{
		CHAR uc_name[260]{};
		for (int i = 0; i < 260; i++)
		{
			uc_name[i] = (char)entry.szExePath[i];
		}

		info->pid  = pid;
		info->path = std::string(uc_name);

		status = 1;
	}
	CloseHandle(snp);

	return status;
}

GameState get_game_state(PCSTR process_name, PCSTR dll_name)
{
	DWORD process_id = get_process_id(process_name);
	if (process_id == 0)
		return GameState::NotRunning;

	if (get_process_dll(process_id, dll_name) == 0)
		return GameState::RunningWithoutAC;

	return GameState::Running;
}

BOOL terminate_process(PCSTR process_name)
{
	DWORD process_id      = get_process_id(process_name);
	HANDLE process_handle = OpenProcess(PROCESS_ALL_ACCESS, 0, process_id);

	if (process_handle == 0)
		return 0;

	BOOL status = TerminateProcess(process_handle, EXIT_SUCCESS);

	CloseHandle(process_handle);

	return status;
}

