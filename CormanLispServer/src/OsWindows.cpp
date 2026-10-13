//		-------------------------------
//		Copyright (c) Corman Technologies Inc.
//		See LICENSE.txt for license information.
//		-------------------------------
//
//		File:		OsWindows.cpp
//		Contents:	What the kernel asks of the operating system, on Windows.
//

#include "stdafx.h"
#include <sys/timeb.h>

#include "Os.h"

void* osReserveMemory(unsigned long size)
{
	return VirtualAlloc(NULL, size, MEM_RESERVE, PAGE_EXECUTE_READWRITE);
}

bool osCommitMemory(void* address, unsigned long size)
{
	return VirtualAlloc(address, size, MEM_COMMIT, PAGE_EXECUTE_READWRITE) != 0;
}

bool osDecommitMemory(void* address, unsigned long size)
{
	return VirtualFree(address, size, MEM_DECOMMIT) != 0;
}

void* osAllocateMemory(unsigned long size)
{
	return VirtualAlloc(NULL, size, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
}

void osFreeMemory(void* address)
{
	if (address)
		VirtualFree(address, 0, MEM_RELEASE);
}

bool osProtectMemory(void* address, unsigned long size, bool writable)
{
	DWORD oldProtect = 0;
	return VirtualProtect(address, size,
		writable ? PAGE_EXECUTE_READWRITE : PAGE_EXECUTE_READ, &oldProtect) != 0;
}

OsThreadSlot osAllocateThreadSlot()
{
	return TlsAlloc();
}

void osFreeThreadSlot(OsThreadSlot slot)
{
	TlsFree(slot);
}

void* osThreadSlotValue(OsThreadSlot slot)
{
	return TlsGetValue(slot);
}

void osSetThreadSlotValue(OsThreadSlot slot, void* value)
{
	TlsSetValue(slot, value);
}

void osInitializeLock(OsLock* lock)
{
	InitializeCriticalSection(lock);
}

void osDeleteLock(OsLock* lock)
{
	DeleteCriticalSection(lock);
}

void __stdcall osEnterLock(OsLock* lock)
{
	EnterCriticalSection(lock);
}

void __stdcall osLeaveLock(OsLock* lock)
{
	LeaveCriticalSection(lock);
}

__int64 osPerformanceCounter()
{
	LARGE_INTEGER count;
	count.QuadPart = 0;
	QueryPerformanceCounter(&count);
	return count.QuadPart;
}

__int64 osPerformanceFrequency()
{
	LARGE_INTEGER frequency;
	frequency.QuadPart = 0;
	QueryPerformanceFrequency(&frequency);
	return frequency.QuadPart;
}

unsigned long osMillisecondCount()
{
	return GetTickCount();
}

void osSleep(unsigned long milliseconds)
{
	Sleep(milliseconds);
}

OsFile osOpenFile(const char* path, bool forWriting)
{
	return CreateFile(path,
		forWriting ? GENERIC_WRITE : GENERIC_READ,
		FILE_SHARE_READ,
		NULL,
		forWriting ? CREATE_ALWAYS : OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);
}

bool osCloseFile(OsFile file)
{
	return CloseHandle(file) != 0;
}

bool osReadFile(OsFile file, void* buffer, unsigned long size, unsigned long* sizeRead)
{
	return ReadFile(file, buffer, size, sizeRead, NULL) != 0;
}

bool osWriteFile(OsFile file, const void* buffer, unsigned long size, unsigned long* sizeWritten)
{
	return WriteFile(file, buffer, size, sizeWritten, NULL) != 0;
}

void* osLoadLibrary(const char* path)
{
	return LoadLibrary(path);
}

bool osFreeLibrary(void* library)
{
	return FreeLibrary((HMODULE)library) != 0;
}

void* osLibrarySymbol(void* library, const char* name)
{
	return (void*)GetProcAddress((HMODULE)library, name);
}

unsigned long osLastError()
{
	return GetLastError();
}

void osDebugMessage(const char* message)
{
	OutputDebugString(message);
}

static void fromSystemTime(const SYSTEMTIME* from, OsTime* time)
{
	time->year = from->wYear;
	time->month = from->wMonth;
	time->dayOfWeek = from->wDayOfWeek;
	time->day = from->wDay;
	time->hour = from->wHour;
	time->minute = from->wMinute;
	time->second = from->wSecond;
	time->milliseconds = from->wMilliseconds;
}

void osUniversalTime(OsTime* time)
{
	SYSTEMTIME now;
	GetSystemTime(&now);
	fromSystemTime(&now, time);
}

void osLocalTime(OsTime* time)
{
	SYSTEMTIME now;
	GetLocalTime(&now);
	fromSystemTime(&now, time);
}

long osSecondsSince1970()
{
	_timeb now;
	_ftime_s(&now);
	return (long)now.time;
}

bool osTimeZone(long* minutesWest, bool* daylight)
{
	TIME_ZONE_INFORMATION tzi;
	DWORD result = GetTimeZoneInformation(&tzi);
	if (result == 0xffffffff)
		return false;
	*minutesWest = tzi.Bias;
	*daylight = (result == TIME_ZONE_ID_DAYLIGHT);
	return true;
}

bool osTimeToFileTime(const OsTime* time, unsigned __int64* fileTime)
{
	SYSTEMTIME from = {0};
	FILETIME to = {0};
	from.wYear = (short)time->year;
	from.wMonth = (short)time->month;
	from.wDayOfWeek = (short)time->dayOfWeek;
	from.wDay = (short)time->day;
	from.wHour = (short)time->hour;
	from.wMinute = (short)time->minute;
	from.wSecond = (short)time->second;
	from.wMilliseconds = (short)time->milliseconds;
	if (!SystemTimeToFileTime(&from, &to))
		return false;
	*fileTime = ((unsigned __int64)to.dwHighDateTime << 32) | to.dwLowDateTime;
	return true;
}

bool osFileTimeToTime(unsigned __int64 fileTime, OsTime* time)
{
	FILETIME from = {0};
	SYSTEMTIME to = {0};
	from.dwLowDateTime = (DWORD)fileTime;
	from.dwHighDateTime = (DWORD)(fileTime >> 32);
	if (!FileTimeToSystemTime(&from, &to))
		return false;
	fromSystemTime(&to, time);
	return true;
}

bool osFlushInstructionCache()
{
	return FlushInstructionCache(GetCurrentProcess(), NULL, 0) != 0;
}
