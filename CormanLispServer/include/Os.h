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

#include "Stdafx.h"

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

//	Whether that many bytes at the address can be read.
extern bool osCanRead(const void* address, unsigned long size);

//	Makes the page at the address raise a stack overflow, once, when it
//	is next touched.
extern bool osGuardPage(void* address);

//	Memory by the byte. Foreign code may be handed it and may free it.
extern void* osHeapAllocate(unsigned long size);
extern void osHeapFree(void* memory);

//	What is added to the block starts out as zeros.
extern void* osHeapReallocate(void* memory, unsigned long size);

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
#ifdef _WIN32
typedef CRITICAL_SECTION OsLock;
#else
typedef pthread_mutex_t OsLock;
#endif

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

//	What local time is at the given universal time: how many seconds it is
//	behind universal time, daylight saving time counted in, whether daylight
//	saving time is in effect, and, if there is room for a name, what the
//	zone is called then. Returns false if that is not known.
extern bool osTimeZone(const OsTime* universal, long* secondsWest, bool* daylight,
	char* name, unsigned long nameSize);

//	A file time counts the tenths of a microsecond since the start of 1601,
//	universal time.
extern bool osTimeToFileTime(const OsTime* time, unsigned __int64* fileTime);
extern bool osFileTimeToTime(unsigned __int64 fileTime, OsTime* time);

//
//	Files, read or written from start to end
//
typedef void* OsFile;
#define OsNoFile ((OsFile)-1)

//	What a file is opened for: either, both, or neither, which only finds
//	out whether it can be opened.
const int OsReadAccess = 1;
const int OsWriteAccess = 2;

//	What opening does about the file being there or not.
const int OsOpenExisting = 0;		// fails if there is no such file
const int OsCreateAlways = 1;		// an empty file in place of any there was
const int OsOpenAlways = 2;			// the file there is, or else a new one

//	Returns OsNoFile on failure. A directory is not a file.
extern OsFile osOpenFile(const char* path, int access, int mode);
extern bool osCloseFile(OsFile file);

//	At the end of the file osReadFile() succeeds with nothing read.
extern bool osReadFile(OsFile file, void* buffer, unsigned long size, unsigned long* sizeRead);
extern bool osWriteFile(OsFile file, const void* buffer, unsigned long size, unsigned long* sizeWritten);

//	Returns (unsigned long)-1 on failure.
extern unsigned long osFileSize(OsFile file);

//	The next read or write is that many bytes from the start of the file,
//	or from its end. Returns where that is from the start, -1 on failure.
extern long osSeekFile(OsFile file, long offset, bool fromEnd);

extern bool osDeleteFile(const char* path);

//
//	When a file was created, last read and last written, as file times
//	(see osTimeToFileTime()). A system which does not keep the time of
//	creation gives the time of the last change instead, and does not set it.
//
extern bool osFileTimes(OsFile file, unsigned __int64* created,
	unsigned __int64* accessed, unsigned __int64* written);
extern bool osSetFileTimes(OsFile file, const unsigned __int64* created,
	const unsigned __int64* accessed, const unsigned __int64* written);

//
//	What a file is, as bits. The bits are the ones Windows has, of which
//	these are known on every system.
//
const unsigned long OsFileReadOnly = 0x01;
const unsigned long OsFileDirectory = 0x10;
const unsigned long OsFileNormal = 0x80;		// none of the others

//	Returns (unsigned long)-1 if there is no such file or directory.
extern unsigned long osFileAttributes(const char* path);
extern bool osSetFileAttributes(const char* path, unsigned long attributes);

//	The path in full, from the root on down, whether or not there is such
//	a file. Returns the length of the full path, 0 on failure.
extern unsigned long osFullPath(const char* path, char* fullPath, unsigned long size);

//
//	Directories
//
extern bool osCurrentDirectory(char* path, unsigned long size);
extern bool osSetCurrentDirectory(const char* path);
extern bool osCreateDirectory(const char* path);

//	Gives a file or a directory another name, which may be in another
//	directory.
extern bool osMoveFile(const char* from, const char* to);

//
//	Looking through a directory for the names which match a pattern. The
//	pattern is a path, in the last part of which * stands for any
//	characters and ? for any one. Lisp reads an OsFoundFile as it is laid
//	out here.
//
struct OsFoundFile
{
	unsigned long isDirectory;
	unsigned long size;
	char name[260];
};
typedef void* OsFileSearch;

//	Returns 0 if nothing matches. Otherwise the first match is in found,
//	and the search is to be closed when it is done with.
extern OsFileSearch osFindFirstFile(const char* pattern, OsFoundFile* found);

//	Returns false when there are no more.
extern bool osFindNextFile(OsFileSearch search, OsFoundFile* found);
extern void osFindClose(OsFileSearch search);

//
//	Libraries loaded while running
//
extern void* osLoadLibrary(const char* path);
extern bool osFreeLibrary(void* library);
extern void* osLibrarySymbol(void* library, const char* name);

//	The operating system's code for why the last call to it by this
//	thread failed.
extern unsigned long osLastError();

//	The name of the operating system, in capitals.
extern const char* osName();

//
//	The process
//
extern unsigned long osProcessId();

//	What the operating system's own functions know the process by.
extern void* osProcessHandle();

//	Ends the process. Does not return.
extern void osExitProcess(unsigned code);

//	The path of the file which a module was loaded from; the module 0 is
//	the program. Returns the length of the path, 0 if it is not known.
extern unsigned long osModulePath(void* module, char* path, unsigned long size);

//	Leaves a mark by which another program can tell that the kernel is
//	loaded in this process.
extern void osMarkKernelLoaded();

//
//	The user. Each of these fills the buffer and returns true, or returns
//	false if it cannot tell. The directories end with a separator.
//
extern bool osUserName(char* name, unsigned long size);
extern bool osUserDirectory(char* path, unsigned long size);
extern bool osUserDocumentsDirectory(char* path, unsigned long size);

//	A message for whoever is debugging the kernel.
extern void osDebugMessage(const char* message);

//	Tells the user something which cannot wait, and returns when they
//	have seen it. The window, if there is one, is the host's.
extern void osShowMessage(void* window, const char* text, const char* title, bool inFront);

#endif // OS_H
