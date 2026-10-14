//		-------------------------------
//		Copyright (c) Corman Technologies Inc.
//		See LICENSE.txt for license information.
//		-------------------------------
//
//		File:		OsWindows.cpp
//		Contents:	What the kernel asks of the operating system, on Windows.
//

#include <ws2tcpip.h>
#include "Stdafx.h"
#include <ole2.h>
#include <sys/timeb.h>
#include <userenv.h>
#include <shlobj.h>

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

bool osCanRead(const void* address, unsigned long size)
{
	return !IsBadReadPtr(address, size);
}

bool osGuardPage(void* address)
{
	DWORD oldProtect = 0;
	return VirtualProtect(address, 1, PAGE_GUARD | PAGE_READWRITE, &oldProtect) != 0;
}

void* osHeapAllocate(unsigned long size)
{
	return HeapAlloc(GetProcessHeap(), 0, size);
}

void osHeapFree(void* memory)
{
	HeapFree(GetProcessHeap(), 0, memory);
}

void* osHeapReallocate(void* memory, unsigned long size)
{
	return HeapReAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, memory, size);
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

OsEvent osCreateEvent(bool resetsItself)
{
	return CreateEvent(NULL, !resetsItself, FALSE, NULL);
}

void osCloseEvent(OsEvent event)
{
	if (event)
		CloseHandle(event);
}

void osSetEvent(OsEvent event)
{
	SetEvent(event);
}

void osResetEvent(OsEvent event)
{
	ResetEvent(event);
}

bool osWaitEvent(OsEvent event, unsigned long milliseconds)
{
	return WaitForSingleObject(event, milliseconds) == WAIT_OBJECT_0;
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

OsFile osOpenFile(const char* path, int access, int mode)
{
	return CreateFile(path,
		((access & OsReadAccess) ? GENERIC_READ : 0) | ((access & OsWriteAccess) ? GENERIC_WRITE : 0),
		FILE_SHARE_READ,
		NULL,
		mode == OsCreateAlways ? CREATE_ALWAYS : mode == OsOpenAlways ? OPEN_ALWAYS : OPEN_EXISTING,
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

unsigned long osFileSize(OsFile file)
{
	return GetFileSize(file, NULL);
}

long osSeekFile(OsFile file, long offset, bool fromEnd)
{
	return (long)SetFilePointer(file, offset, NULL, fromEnd ? FILE_END : FILE_BEGIN);
}

bool osDeleteFile(const char* path)
{
	return DeleteFile(path) != 0;
}

static unsigned __int64 fromFileTime(const FILETIME* time)
{
	return ((unsigned __int64)time->dwHighDateTime << 32) | time->dwLowDateTime;
}

static void toFileTime(unsigned __int64 from, FILETIME* time)
{
	time->dwLowDateTime = (DWORD)from;
	time->dwHighDateTime = (DWORD)(from >> 32);
}

bool osFileTimes(OsFile file, unsigned __int64* created,
	unsigned __int64* accessed, unsigned __int64* written)
{
	FILETIME c, a, w;
	if (!GetFileTime(file, &c, &a, &w))
		return false;
	*created = fromFileTime(&c);
	*accessed = fromFileTime(&a);
	*written = fromFileTime(&w);
	return true;
}

bool osSetFileTimes(OsFile file, const unsigned __int64* created,
	const unsigned __int64* accessed, const unsigned __int64* written)
{
	FILETIME c, a, w;
	toFileTime(*created, &c);
	toFileTime(*accessed, &a);
	toFileTime(*written, &w);
	return SetFileTime(file, &c, &a, &w) != 0;
}

unsigned long osFileAttributes(const char* path)
{
	return GetFileAttributesA(path);
}

bool osSetFileAttributes(const char* path, unsigned long attributes)
{
	return SetFileAttributesA(path, attributes) != 0;
}

unsigned long osFullPath(const char* path, char* fullPath, unsigned long size)
{
	DWORD length = GetFullPathNameA(path, size, fullPath, NULL);
	return length < size ? length : 0;
}

bool osCurrentDirectory(char* path, unsigned long size)
{
	DWORD length = GetCurrentDirectoryA(size, path);
	return length > 0 && length < size;
}

bool osCreateDirectory(const char* path)
{
	return CreateDirectoryA(path, NULL) != 0;
}

bool osMoveFile(const char* from, const char* to)
{
	return MoveFileA(from, to) != 0;
}

static void fromFindData(const WIN32_FIND_DATAA* from, OsFoundFile* found)
{
	found->isDirectory = (from->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? 1 : 0;
	found->size = from->nFileSizeLow;
	strcpy_s(found->name, sizeof(found->name), from->cFileName);
}

OsFileSearch osFindFirstFile(const char* pattern, OsFoundFile* found)
{
	WIN32_FIND_DATAA data;
	HANDLE search = FindFirstFileA(pattern, &data);
	if (search == INVALID_HANDLE_VALUE)
		return 0;
	fromFindData(&data, found);
	return search;
}

bool osFindNextFile(OsFileSearch search, OsFoundFile* found)
{
	WIN32_FIND_DATAA data;
	if (!FindNextFileA(search, &data))
		return false;
	fromFindData(&data, found);
	return true;
}

void osFindClose(OsFileSearch search)
{
	if (search)
		FindClose(search);
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

const char* osName()
{
	return "WINDOWS";
}

unsigned long osProcessId()
{
	return GetCurrentProcessId();
}

void* osProcessHandle()
{
	return GetCurrentProcess();
}

void osExitProcess(unsigned code)
{
	ExitProcess(code);
}

unsigned long osModulePath(void* module, char* path, unsigned long size)
{
	return GetModuleFileName((HMODULE)module, path, size);
}

bool osSetCurrentDirectory(const char* path)
{
	return SetCurrentDirectoryA(path) != 0;
}

void osMarkKernelLoaded()
{
	// a named mutex, which another program can look for
	CreateMutex(NULL, FALSE, "CormanLispServer");
}

bool osUserName(char* name, unsigned long size)
{
	DWORD length = size;
	return GetUserNameA(name, &length) != 0;
}

static bool endWithSeparator(char* path, unsigned long size)
{
	size_t length = strlen(path);
	if (length == 0 || path[length - 1] != '\\')
	{
		if (length + 2 > size)
			return false;
		path[length] = '\\';
		path[length + 1] = 0;
	}
	return true;
}

bool osUserDirectory(char* path, unsigned long size)
{
	HANDLE token = NULL;
	DWORD length = size;
	bool found = false;
	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token) || token == NULL)
		return false;
	found = GetUserProfileDirectoryA(token, path, &length) != 0;
	CloseHandle(token);
	return found && endWithSeparator(path, size);
}

bool osUserDocumentsDirectory(char* path, unsigned long size)
{
	HANDLE token = NULL;
	char found[MAX_PATH] = { 0 };
	bool ok = false;
	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token) || token == NULL)
		return false;
	ok = SHGetFolderPathA(NULL, CSIDL_PERSONAL, token, SHGFP_TYPE_CURRENT, found) == S_OK;
	CloseHandle(token);
	if (!ok || strlen(found) + 1 > size)
		return false;
	strcpy_s(path, size, found);
	return endWithSeparator(path, size);
}

unsigned long osLastError()
{
	return GetLastError();
}

void osDebugMessage(const char* message)
{
	OutputDebugString(message);
}

void osShowMessage(void* window, const char* text, const char* title, bool inFront)
{
	MessageBox((HWND)window, text, title, inFront ? MB_OK|MB_SETFOREGROUND : MB_OK);
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

static void toSystemTime(const OsTime* time, SYSTEMTIME* to)
{
	to->wYear = (short)time->year;
	to->wMonth = (short)time->month;
	to->wDayOfWeek = (short)time->dayOfWeek;
	to->wDay = (short)time->day;
	to->wHour = (short)time->hour;
	to->wMinute = (short)time->minute;
	to->wSecond = (short)time->second;
	to->wMilliseconds = (short)time->milliseconds;
}

static bool toFileTime(const SYSTEMTIME* from, unsigned __int64* fileTime)
{
	FILETIME to = {0};
	if (!SystemTimeToFileTime(from, &to))
		return false;
	*fileTime = ((unsigned __int64)to.dwHighDateTime << 32) | to.dwLowDateTime;
	return true;
}

bool osTimeZone(const OsTime* universal, long* secondsWest, bool* daylight,
	char* name, unsigned long nameSize)
{
	TIME_ZONE_INFORMATION zone;
	SYSTEMTIME there = {0};
	SYSTEMTIME here = {0};
	unsigned __int64 thereTime = 0;
	unsigned __int64 hereTime = 0;
	toSystemTime(universal, &there);

	// by the rules of that year, which need not be those of this year
	if (!GetTimeZoneInformationForYear(there.wYear, NULL, &zone)
			|| !SystemTimeToTzSpecificLocalTime(&zone, &there, &here)
			|| !toFileTime(&there, &thereTime)
			|| !toFileTime(&here, &hereTime))
		return false;
	*secondsWest = (long)((__int64)(thereTime - hereTime) / 10000000);

	// a zone which never has daylight saving time has no date for it
	*daylight = zone.DaylightDate.wMonth != 0
		&& *secondsWest != (zone.Bias + zone.StandardBias) * 60;
	if (name && nameSize > 0)
	{
		if (!WideCharToMultiByte(CP_ACP, 0,
				*daylight ? zone.DaylightName : zone.StandardName, -1,
				name, (int)nameSize, NULL, NULL))
			name[0] = 0;
	}
	return true;
}

bool osTimeToFileTime(const OsTime* time, unsigned __int64* fileTime)
{
	SYSTEMTIME from = {0};
	toSystemTime(time, &from);
	return toFileTime(&from, fileTime);
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

bool osCopyFile(const char* from, const char* to, bool replace)
{
	return CopyFile(from, to, replace ? FALSE : TRUE) != 0;
}

void* osMapFile(const char* path, unsigned long* size)
{
	HANDLE file = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL, NULL);
	HANDLE mapping = NULL;
	void* address = 0;
	if (file == INVALID_HANDLE_VALUE)
		return 0;
	*size = GetFileSize(file, NULL);
	if (*size != 0xffffffff)
		mapping = CreateFileMapping(file, NULL, PAGE_READONLY, 0, 0, NULL);
	CloseHandle(file);
	if (!mapping)
		return 0;
	address = MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0);
	CloseHandle(mapping);
	return address;
}

bool osUnmapFile(void* address)
{
	return UnmapViewOfFile(address) != 0;
}

bool osFlushInstructionCache()
{
	return FlushInstructionCache(GetCurrentProcess(), NULL, 0) != 0;
}

bool osAllowExecution(void* address, unsigned long size)
{
	DWORD oldProtect = 0;
	return VirtualProtect(address, size, PAGE_EXECUTE_READWRITE, &oldProtect) != 0;
}

OsThread osCreateThread(OsThreadProc proc, void* argument, unsigned long* id)
{
	unsigned int threadID = 0;
	OsThread thread = (OsThread)_beginthreadex(
			0,		// security
			0,		// stack size (0 = use default)
			proc,
			argument,
			CREATE_SUSPENDED,
			&threadID);
	*id = threadID;
	return thread;
}

void osCloseThread(OsThread thread)
{
	CloseHandle(thread);
}

void osExitThread(unsigned code)
{
	_endthreadex(code);
}

bool osStartThreadServices()
{
	WSADATA wsd;
	CoInitialize(0);
	if (WSAStartup(MAKEWORD(2, 2), &wsd) != 0)
	{
		CoUninitialize();
		return false;
	}
	return true;
}

void osStopThreadServices()
{
	WSACleanup();
	CoUninitialize();
}

unsigned long osCurrentThreadId()
{
	return GetCurrentThreadId();
}

OsThread osOpenCurrentThread()
{
	HANDLE thread = 0;
	DuplicateHandle(
		GetCurrentProcess(),
		GetCurrentThread(),
		GetCurrentProcess(),
		&thread,
		0,
		TRUE,
		DUPLICATE_SAME_ACCESS);
	return thread;
}

void* osStackTop()
{
	long dummy = 0;
	MEMORY_BASIC_INFORMATION info;
	VirtualQuery(&dummy, &info, sizeof(MEMORY_BASIC_INFORMATION));
	return (char*)info.BaseAddress + info.RegionSize;
}

bool osSuspendThread(OsThread thread)
{
	return SuspendThread(thread) != (DWORD)-1;
}

void osResumeThread(OsThread thread)
{
	ResumeThread(thread);
}

bool osGetThreadRegisters(OsThread thread, OsRegisters* registers)
{
	CONTEXT context;
	context.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
	if (!GetThreadContext(thread, &context))
		return false;
	registers->eax = context.Eax;
	registers->ebx = context.Ebx;
	registers->ecx = context.Ecx;
	registers->edx = context.Edx;
	registers->esi = context.Esi;
	registers->edi = context.Edi;
	registers->ebp = context.Ebp;
	registers->esp = context.Esp;
	registers->eip = context.Eip;
	registers->eflags = context.EFlags;
	return true;
}

bool osSetThreadRegisters(OsThread thread, const OsRegisters* registers)
{
	// the rest of what goes with these registers stays as it is
	CONTEXT context;
	context.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
	if (!GetThreadContext(thread, &context))
		return false;
	context.Eax = registers->eax;
	context.Ebx = registers->ebx;
	context.Ecx = registers->ecx;
	context.Edx = registers->edx;
	context.Esi = registers->esi;
	context.Edi = registers->edi;
	context.Ebp = registers->ebp;
	context.Esp = registers->esp;
	context.Eip = registers->eip;
	context.EFlags = registers->eflags;
	context.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
	return SetThreadContext(thread, &context) != 0;
}
