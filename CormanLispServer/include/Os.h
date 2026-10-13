//		-------------------------------
//		Copyright (c) Corman Technologies Inc.
//		See LICENSE.txt for license information.
//		-------------------------------
//
//		File:		Os.h
//		Contents:	What the kernel asks of the operating system.
//

#ifndef OS_H
#define OS_H

#include <windows.h>

//
//	Memory, by the page. Committed memory can be read, written and
//	executed, and starts out as zeros.
//

//	Sets a range of addresses aside without making it usable.
//	Returns 0 if there is no such range to be had.
extern void* osReserveMemory(unsigned long size);

//	Makes part of a reserved range usable.
extern bool osCommitMemory(void* address, unsigned long size);

//	Gives the memory of part of a reserved range back. The addresses
//	stay reserved.
extern bool osDecommitMemory(void* address, unsigned long size);

//	Reserves and commits in one step. Returns 0 if out of memory.
extern void* osAllocateMemory(unsigned long size);

//	Releases what osAllocateMemory() returned.
extern void osFreeMemory(void* address);

//	Makes committed pages read-only, or writable again. A write to a
//	read-only page raises an access violation.
extern bool osProtectMemory(void* address, unsigned long size, bool writable);

//	To be called after machine code has been written or moved, before it
//	is run.
extern bool osFlushInstructionCache();

//
//	A slot which holds a value for each thread. The value is 0 in a
//	thread which has not set it.
//
typedef unsigned long OsThreadSlot;

extern OsThreadSlot osAllocateThreadSlot();
extern void osFreeThreadSlot(OsThreadSlot slot);
extern void* osThreadSlotValue(OsThreadSlot slot);
extern void osSetThreadSlotValue(OsThreadSlot slot, void* value);

//
//	A lock which the thread that holds it may enter again. Lisp code is
//	handed the address of an OsLock as a foreign pointer, and the image
//	file records how many bytes one takes up.
//
typedef CRITICAL_SECTION OsLock;

extern void osInitializeLock(OsLock* lock);
extern void osDeleteLock(OsLock* lock);

//	These two are called from inline assembly as well, which pushes the
//	argument and expects the function to remove it.
extern void __stdcall osEnterLock(OsLock* lock);
extern void __stdcall osLeaveLock(OsLock* lock);

//
//	Events
//
//	An event is set or it is not, and a thread can wait until it is. One
//	which resets itself is no longer set once a wait for it has returned,
//	so that each osSetEvent() lets one waiting thread go on.
//
typedef void* OsEvent;
const unsigned long OsForever = 0xffffffff;

//	The new event is not set.
extern OsEvent osCreateEvent(bool resetsItself);
extern void osCloseEvent(OsEvent event);
extern void osSetEvent(OsEvent event);
extern void osResetEvent(OsEvent event);

//	Returns false if the event was still not set after that long.
extern bool osWaitEvent(OsEvent event, unsigned long milliseconds);

//
//	Threads
//
typedef void* OsThread;
typedef unsigned (__stdcall* OsThreadProc)(void* argument);

//	The new thread does not run until osResumeThread() is called for it.
//	Returns 0 on failure. Whoever is done with the OsThread closes it;
//	that does not end the thread.
extern OsThread osCreateThread(OsThreadProc proc, void* argument, unsigned long* id);
extern void osCloseThread(OsThread thread);

//	Ends the calling thread. Does not return.
extern void osExitThread(unsigned code);

//	To be called by a thread which is going to run Lisp, before it does,
//	and again when it is done. If the first returns false the thread
//	cannot run Lisp, and has nothing to stop.
extern bool osStartThreadServices();
extern void osStopThreadServices();

extern unsigned long osCurrentThreadId();

//	An OsThread for the calling thread, to be closed like any other.
extern OsThread osOpenCurrentThread();

//	The address just past the calling thread's stack: the stack grows down
//	from there.
extern void* osStackTop();

//	Stops another thread wherever it happens to be, and lets it go on.
//	A thread suspended twice has to be resumed twice.
extern bool osSuspendThread(OsThread thread);
extern void osResumeThread(OsThread thread);

//	The registers of a thread which is suspended. Setting them changes
//	where and with what the thread goes on when it is resumed.
struct OsRegisters
{
	unsigned long eax;
	unsigned long ebx;
	unsigned long ecx;
	unsigned long edx;
	unsigned long esi;
	unsigned long edi;
	unsigned long ebp;
	unsigned long esp;
	unsigned long eip;
	unsigned long eflags;
};

extern bool osGetThreadRegisters(OsThread thread, OsRegisters* registers);
extern bool osSetThreadRegisters(OsThread thread, const OsRegisters* registers);

//
//	Time
//

//	A counter which only goes forward, and how much it advances in a second.
extern __int64 osPerformanceCounter();
extern __int64 osPerformanceFrequency();

//	Milliseconds since some moment in the past. Wraps around.
extern unsigned long osMillisecondCount();

extern void osSleep(unsigned long milliseconds);

//
//	The calendar
//
struct OsTime
{
	int year;
	int month;			// 1 to 12
	int dayOfWeek;		// 0 is Sunday
	int day;
	int hour;
	int minute;
	int second;
	int milliseconds;
};

extern void osUniversalTime(OsTime* time);
extern void osLocalTime(OsTime* time);
extern long osSecondsSince1970();

//	How many minutes local time is behind universal time, and whether
//	daylight saving time is in effect. Returns false if that is not known.
extern bool osTimeZone(long* minutesWest, bool* daylight);

//	A file time counts the tenths of a microsecond since the start of 1601,
//	universal time.
extern bool osTimeToFileTime(const OsTime* time, unsigned __int64* fileTime);
extern bool osFileTimeToTime(unsigned __int64 fileTime, OsTime* time);

//
//	Files, read or written from start to end
//
typedef void* OsFile;
#define OsNoFile ((OsFile)-1)

//	Opens an existing file for reading, or creates an empty one for
//	writing in place of whatever had that name. Returns OsNoFile on failure.
extern OsFile osOpenFile(const char* path, bool forWriting);
extern bool osCloseFile(OsFile file);

//	At the end of the file osReadFile() succeeds with nothing read.
extern bool osReadFile(OsFile file, void* buffer, unsigned long size, unsigned long* sizeRead);
extern bool osWriteFile(OsFile file, const void* buffer, unsigned long size, unsigned long* sizeWritten);

//
//	Libraries loaded while running
//
extern void* osLoadLibrary(const char* path);
extern bool osFreeLibrary(void* library);
extern void* osLibrarySymbol(void* library, const char* name);

//	The operating system's code for why the last call to it by this
//	thread failed.
extern unsigned long osLastError();

//	A message for whoever is debugging the kernel.
extern void osDebugMessage(const char* message);

#endif // OS_H
