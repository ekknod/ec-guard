#pragma once
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>
#include <Ntsecapi.h>
#include <vector>
#include <chrono>
#include <MinHook.h>

typedef const UNICODE_STRING * PCUNICODE_STRING;
typedef struct _LDR_DLL_LOADED_NOTIFICATION_DATA {
    ULONG Flags;                    //Reserved.
    PCUNICODE_STRING FullDllName;   //The full path name of the DLL module.
    PCUNICODE_STRING BaseDllName;   //The base file name of the DLL module.
    PVOID DllBase;                  //A pointer to the base address for the DLL in memory.
    ULONG SizeOfImage;              //The size of the DLL image, in bytes.
} LDR_DLL_LOADED_NOTIFICATION_DATA, *PLDR_DLL_LOADED_NOTIFICATION_DATA;

typedef union _LDR_DLL_NOTIFICATION_DATA {
    LDR_DLL_LOADED_NOTIFICATION_DATA Loaded;
    LDR_DLL_LOADED_NOTIFICATION_DATA Unloaded;
} LDR_DLL_NOTIFICATION_DATA, *PLDR_DLL_NOTIFICATION_DATA;

typedef const _LDR_DLL_NOTIFICATION_DATA * PCLDR_DLL_NOTIFICATION_DATA;

typedef VOID (CALLBACK * PLDR_DLL_NOTIFICATION_FUNCTION)(
  _In_      ULONG NotificationReason,
  _In_      PCLDR_DLL_NOTIFICATION_DATA NotificationData,
  _In_opt_  PVOID Context
);

typedef struct {
	HANDLE handle;
	UINT64 total_calls;
} DEVICE_INFO ;

#define LDR_DLL_NOTIFICATION_REASON_LOADED 1
#define LDR_DLL_NOTIFICATION_REASON_UNLOADED 2


inline void FontColor(int color=0x07) { SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color); }


#define DEBUG

#define LOG(...) \
FontColor(3); \
printf("[ec-guard.dll] "); \
FontColor(7); \
printf(__VA_ARGS__); \


typedef ULONG_PTR QWORD;

