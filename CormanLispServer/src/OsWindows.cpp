//		-------------------------------
//		Copyright (c) Corman Technologies Inc.
//		See LICENSE.txt for license information.
//		-------------------------------
//
//		File:		OsWindows.cpp
//		Contents:	What the kernel asks of the operating system, on Windows.
//

#include "stdafx.h"

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
