//		-------------------------------
//		Copyright (c) Corman Technologies Inc.
//		See LICENSE.txt for license information.
//		-------------------------------
//
//		File:		CormanLispServer.h
//		Contents:	Server interface for Corman Lisp
//		History:	8/5/97  RGC  Created.
//					4/10/16 Artem Boldarev
//					4/10/16 Artem Boldarev
//							global UserInfo object.
//

#ifndef CORMANLISPSERVER_H
#define CORMANLISPSERVER_H

#include "CormanLispHost.h"
#include "CharBuf.h"
#include "UserInfo.h"

class CharBuf;

void InitializeCormanLisp(const CormanLispHost* host, const UserInfo *user_info);
void RunCormanLisp(HANDLE* thread);
void ProcessLispSource(const char* text, long numChars);
long GetNumLispThreads();
void ThrowUserException();
void AbortLispThread();
extern "C" __declspec(dllexport) void BlessThread();
extern "C" __declspec(dllexport) void UnblessThread();
extern "C" __declspec(dllexport) void* GetCallbackFunctionPointer(wchar_t* functionName, wchar_t* packageName);
long handleStructuredException(long exception, LPEXCEPTION_POINTERS info);
EXCEPTION_DISPOSITION __cdecl Heap_Fault_Handler(struct _EXCEPTION_RECORD* ExceptionRecord,
    void* EstablisherFrame, struct _CONTEXT* ContextRecord, void* DispatcherContext);
long getHeapStatistics(long generation, long* allocated, long* used);

extern CharBuf						TerminalInputBuf;
extern const int					LispImageNameMax;
extern char							LispImageName[];
extern int							CormanLispClientType;
extern const UserInfo*              CurrentUserInfo;

typedef void (*TextOutputFuncType)(wchar_t* text, long numChars);
extern TextOutputFuncType			TextOutputFuncPtr;

#endif	// CORMANLISPSERVER_H