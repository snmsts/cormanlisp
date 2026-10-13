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
//	Time
//

//	A counter which only goes forward, and how much it advances in a second.
extern __int64 osPerformanceCounter();
extern __int64 osPerformanceFrequency();

//	Milliseconds since some moment in the past. Wraps around.
extern unsigned long osMillisecondCount();

extern void osSleep(unsigned long milliseconds);

#endif // OS_H
