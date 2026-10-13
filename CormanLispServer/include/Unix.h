//		-------------------------------
//		Copyright (c) Corman Technologies Inc.
//		See LICENSE.txt for license information.
//		-------------------------------
//
//		File:		Unix.h
//		Contents:	The names from the Windows headers which the kernel's
//					sources use, for a system which is not Windows.
//

#ifndef UNIX_H
#define UNIX_H

#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <wchar.h>
#include <limits.h>
#include <errno.h>
#include <alloca.h>
#include <pthread.h>

typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef unsigned long DWORD;
typedef int BOOL;
typedef long LONG;
typedef unsigned long ULONG;
typedef unsigned int UINT;
typedef void* LPVOID;
typedef void* HANDLE;
typedef void* HINSTANCE;
typedef void* HMODULE;
typedef void* HWND;
typedef char TCHAR;

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#define MAX_PATH 260
#define _MAX_PATH MAX_PATH

//
//	The C library's functions which take the size of their destination.
//	These return 0 when they have done what was asked.
//
#define _TRUNCATE ((size_t)-1)
#define _alloca alloca

inline int strncpy_s(char* destination, size_t size, const char* source, size_t count)
{
	size_t length = strlen(source);
	if (length > count)
		length = count;
	if (size == 0)
		return ERANGE;
	if (length >= size)
	{
		if (count != _TRUNCATE)
		{
			destination[0] = 0;
			return ERANGE;
		}
		length = size - 1;
	}
	memcpy(destination, source, length);
	destination[length] = 0;
	return 0;
}

inline int strcpy_s(char* destination, size_t size, const char* source)
{
	if (size == 0)
		return ERANGE;
	if (strlen(source) >= size)
	{
		destination[0] = 0;
		return ERANGE;
	}
	strcpy(destination, source);
	return 0;
}

inline int strncat_s(char* destination, size_t size, const char* source, size_t count)
{
	size_t used = strnlen(destination, size);
	if (used >= size)
		return EINVAL;
	return strncpy_s(destination + used, size - used, source, count);
}

inline int strcat_s(char* destination, size_t size, const char* source)
{
	return strncat_s(destination, size, source, size);
}

inline int fopen_s(FILE** file, const char* path, const char* mode)
{
	*file = fopen(path, mode);
	return *file ? 0 : errno;
}

#define sprintf_s snprintf

//
//	Counters which several threads change.
//
inline LONG InterlockedIncrement(LONG volatile* value)
{
	return __sync_add_and_fetch(value, 1);
}

inline LONG InterlockedCompareExchange(LONG volatile* destination, LONG exchange, LONG comparand)
{
	return __sync_val_compare_and_swap(destination, comparand, exchange);
}

//
//	Exceptions, as the kernel and the Lisp code which it runs know them.
//	Nothing raises these but the kernel itself.
//
#define EXCEPTION_MAXIMUM_PARAMETERS 15

typedef struct _EXCEPTION_RECORD
{
	DWORD ExceptionCode;
	DWORD ExceptionFlags;
	struct _EXCEPTION_RECORD* ExceptionRecord;
	void* ExceptionAddress;
	DWORD NumberParameters;
	unsigned long ExceptionInformation[EXCEPTION_MAXIMUM_PARAMETERS];
} EXCEPTION_RECORD;

typedef struct _CONTEXT
{
	DWORD ContextFlags;
	DWORD SegGs;
	DWORD SegFs;
	DWORD SegEs;
	DWORD SegDs;
	DWORD Edi;
	DWORD Esi;
	DWORD Ebx;
	DWORD Edx;
	DWORD Ecx;
	DWORD Eax;
	DWORD Ebp;
	DWORD Eip;
	DWORD SegCs;
	DWORD EFlags;
	DWORD Esp;
	DWORD SegSs;
} CONTEXT;

typedef struct _EXCEPTION_POINTERS
{
	EXCEPTION_RECORD* ExceptionRecord;
	CONTEXT* ContextRecord;
} EXCEPTION_POINTERS, *LPEXCEPTION_POINTERS;

typedef enum _EXCEPTION_DISPOSITION
{
	ExceptionContinueExecution,
	ExceptionContinueSearch,
	ExceptionNestedException,
	ExceptionCollidedUnwind
} EXCEPTION_DISPOSITION;

typedef EXCEPTION_DISPOSITION (__cdecl *PEXCEPTION_ROUTINE)(EXCEPTION_RECORD* record,
	void* registration, CONTEXT* context, void* dispatcherContext);

typedef struct _EXCEPTION_REGISTRATION_RECORD
{
	struct _EXCEPTION_REGISTRATION_RECORD* Next;
	PEXCEPTION_ROUTINE Handler;
} EXCEPTION_REGISTRATION_RECORD;

#define CONTAINING_RECORD(address, type, field) \
	((type*)((char*)(address) - offsetof(type, field)))

// what a filter returns
#define EXCEPTION_EXECUTE_HANDLER		1
#define EXCEPTION_CONTINUE_SEARCH		0
#define EXCEPTION_CONTINUE_EXECUTION	(-1)

// ExceptionFlags
#define EXCEPTION_NONCONTINUABLE		0x1
#define EXCEPTION_UNWINDING				0x2
#define EXCEPTION_EXIT_UNWIND			0x4

// ExceptionCode
#define EXCEPTION_GUARD_PAGE				0x80000001
#define EXCEPTION_DATATYPE_MISALIGNMENT		0x80000002
#define EXCEPTION_BREAKPOINT				0x80000003
#define EXCEPTION_SINGLE_STEP				0x80000004
#define EXCEPTION_ACCESS_VIOLATION			0xC0000005
#define EXCEPTION_IN_PAGE_ERROR				0xC0000006
#define EXCEPTION_INVALID_HANDLE			0xC0000008
#define EXCEPTION_ILLEGAL_INSTRUCTION		0xC000001D
#define EXCEPTION_NONCONTINUABLE_EXCEPTION	0xC0000025
#define EXCEPTION_INVALID_DISPOSITION		0xC0000026
#define EXCEPTION_ARRAY_BOUNDS_EXCEEDED		0xC000008C
#define EXCEPTION_FLT_DENORMAL_OPERAND		0xC000008D
#define EXCEPTION_FLT_DIVIDE_BY_ZERO		0xC000008E
#define EXCEPTION_FLT_INEXACT_RESULT		0xC000008F
#define EXCEPTION_FLT_INVALID_OPERATION		0xC0000090
#define EXCEPTION_FLT_OVERFLOW				0xC0000091
#define EXCEPTION_FLT_STACK_CHECK			0xC0000092
#define EXCEPTION_FLT_UNDERFLOW				0xC0000093
#define EXCEPTION_INT_DIVIDE_BY_ZERO		0xC0000094
#define EXCEPTION_INT_OVERFLOW				0xC0000095
#define EXCEPTION_PRIV_INSTRUCTION			0xC0000096
#define EXCEPTION_STACK_OVERFLOW			0xC00000FD
#define CONTROL_C_EXIT						0xC000013A

#endif // UNIX_H
