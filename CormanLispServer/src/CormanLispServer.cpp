//		-------------------------------
//		Copyright (c) Corman Technologies Inc.
//		See LICENSE.txt for license information.
//		-------------------------------
//
//		File:		CormanLispServer.cpp
//		Contents:	Server for Corman Lisp
//		History:	8/5/97  RGC  Created.
//					4/10/16 Artem Boldarev
//							global UserInfo object.
//

#include "stdafx.h"
#include "ExceptionFrame.h"
#include <process.h>
#include <string.h>

#include "CormanLispServer.h"
#include "Host.h"
#include "lisp.h"

const UserInfo*          CurrentUserInfo    = 0;


TextOutputFuncType TextOutputFuncPtr = 0;
HINSTANCE gAppInstance = 0;
HWND      gAppMainWindow = 0;

OSVERSIONINFO gOsVersionInfo;
TCHAR g_szFileName[MAX_PATH];
char CormanLispServerDirectory[MAX_PATH+1];

TerminalInput TerminalInputBuf(0x8000);

static void RunLispThread(HANDLE* thread);

int DLL_Loaded = false;		// we may get run when statically linked

void processAttach(HINSTANCE hInstance)
{
	BOOL ret = 0;
	char* result = 0;
	QV_Index = osAllocateThreadSlot();
	Thread_Index = osAllocateThreadSlot();
	osSetThreadSlotValue(QV_Index, 0);
	osSetThreadSlotValue(Thread_Index, 0);
	osThreadSlotValue(QV_Index);
	osThreadSlotValue(Thread_Index);
	GetModuleFileName(hInstance, g_szFileName, MAX_PATH);

	// create a named mutex that is used to determine if the
	// Corman Lisp kernel is loaded
	CreateMutex(NULL, FALSE, "CormanLispServer");

	// setup the DLL directory
	strcpy_s(CormanLispServerDirectory, sizeof(CormanLispServerDirectory), g_szFileName);
	result = strrchr(CormanLispServerDirectory, '\\');
	if (result)
		*result = 0;
	else
		CormanLispServerDirectory[0] = 0;
}

extern "C" int __declspec( dllexport ) __stdcall
DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID /*lpReserved*/)
{
	char* result = 0;
	BOOL ret = 0;
	switch (dwReason)
	{
		case DLL_PROCESS_ATTACH:
			DLL_Loaded = true;		
			processAttach(hInstance);
			break;
		case DLL_PROCESS_DETACH:
			osFreeThreadSlot(QV_Index);
			osFreeThreadSlot(Thread_Index);
			break;
		case DLL_THREAD_ATTACH:
			osSetThreadSlotValue(QV_Index, 0);
			osSetThreadSlotValue(Thread_Index, 0);
			osThreadSlotValue(QV_Index);
			osThreadSlotValue(Thread_Index);
			break;
		case DLL_THREAD_DETACH:
			// need to free QV, ThreadRecord
			osSetThreadSlotValue(QV_Index, 0);
			osSetThreadSlotValue(Thread_Index, 0);
			break;
	}
	return 1;   // ok
}

extern "C" long Initialize(const wchar_t* imageName, int clientType, HINSTANCE appInstance,
			   HANDLE* thread, HWND mainWindow, TextOutputFuncType TextOutputFunc)
{
    static int initFlag = 0;
    static CriticalSection initSect;

    initSect.Enter();
    if (initFlag == 0)      // only want to initialize once
    {
        initFlag++;
	    const wchar_t* ch = 0;
	    int i = 0;
	    InitializationEvent.Reset();

	    if (!DLL_Loaded)	
		    processAttach(appInstance);
	    TextOutputFuncPtr = TextOutputFunc;
	    gAppInstance = appInstance;
	    gAppMainWindow = mainWindow;
	    if (imageName)
	    {
		    for (ch = imageName; *ch && i < LispImageNameMax - 1; ch++, i++)
			    LispImageName[i] = (unsigned char)(*ch & 0xff);		// just copy low byte
	    }
	    LispImageName[i] = 0;

	    // initialize lisp system
	    osSetThreadSlotValue(QV_Index, QV);	// store global QV as the QV for this thread
	    osThreadSlotValue(QV_Index);

	    initLisp();
	    osSetThreadSlotValue(QV_Index, 0);
	    CormanLispClientType = clientType;
	    RunLispThread(thread);		// start 'er up
    }
    initSect.Leave();

	return 1;		// success
}

void InitializeCormanLisp(const CormanLispHost* host, const UserInfo *user_info)
{
	CurrentUserInfo = user_info;
	hostAttach(host);
	if (!DLL_Loaded)
		processAttach((HINSTANCE)hostAppInstance());

	// initialize lisp system
	osSetThreadSlotValue(QV_Index, QV);	// store global QV as the QV for this thread
	osThreadSlotValue(QV_Index);

	initLisp();
	osSetThreadSlotValue(QV_Index, 0);
}

void RunCormanLisp(HANDLE* thread)
{
	RunLispThread(thread);
}

void
ProcessLispSource(const char* text, long numChars)
{
	long i = 0;
	long added = 0;
	while (numChars > 0)
	{
		added = TerminalInputBuf.addChars((char*)text + i, numChars);
		i += added;
		numChars -= added;
	}
}

//#define NTONLY 1
//#define WIN98ONLY 1

#ifdef NTONLY
__declspec(naked)
LispObj*
ThreadQV()
{
	__asm
	{
		push	ebp
		mov		ebp, esp
		mov		eax, fs:0018h			;; eax = TIB
		mov		edx, dword ptr QV_Index
		mov		eax, dword ptr [eax + edx*4 + 0e10h]
		pop		ebp
		ret
	}
}
#elif WIN98ONLY
__declspec(naked)
LispObj*
ThreadQV()
{
	__asm
	{
		push	ebp
		mov		ebp, esp
		mov		eax, fs:0018h			;; eax = TIB
		mov		eax, [eax + 2Ch]		;; eax = TLS array
		mov		edx, dword ptr QV_Index
		mov		eax, [eax + edx*4]
		pop		ebp
		ret
	}
}
#else
LispObj*
ThreadQV()
{
	return (LispObj*)osThreadSlotValue(QV_Index);
}
#endif

extern int lispmain();

// This is the thread procedure for the primary lisp thread
UINT LispMainProc(LPVOID /*pParam*/)
{
	LispObj dummy = 0;
	LispObj fp = 0;
	ThreadRecord* th = 0;
	DWORD currThreadID = 0;
	currThreadID = osCurrentThreadId();

	__asm	mov  dummy, ebp

	// find our thread record
	th = ThreadList.getList();
	while (th)
	{
		if (th->threadID == currThreadID)
			break;
		th = th->next;
	}

	th->stackStart = (unsigned long*)dummy;
	osSetThreadSlotValue(Thread_Index, th);
	osSetThreadSlotValue(QV_Index, th->QV_rec);
	osThreadSlotValue(QV_Index);	// force NT to allocate the block
	if (!osStartThreadServices())
		osExitThread(0);

	pushDynamicBinding(COMPILER_RUNTIME, NIL);

	// bind ccl:*current-thread-id*, ccl:*current-thread-handle*
	pushDynamicBinding(CURRENT_THREAD_ID, createUnsignedLispInteger(th->threadID));
	fp = foreignNode();
	UVECTOR(fp)[FOREIGN_PTR] = (LispObj)th->thread;
	pushDynamicBinding(CURRENT_THREAD_HANDLE, fp);
	fp = 0;

	lispmain();

	osStopThreadServices();
	osExitThread(0);
	return 0;
}

//	RunLispThread()
//	Starts up a new lisp thread.
static void
RunLispThread(HANDLE* thread)
{
	ThreadRecord* th = new ThreadRecord;
	auto sz = sizeof(*th);
	th->QV_rec = createNewQV();
	NumLispThreads++;
	ThreadList.insert(th);
	th->type = ThreadRecord::PrimaryThread;
	th->started = 1;

	// create the thread
	th->thread = osCreateThread((OsThreadProc)LispMainProc, 0, &th->threadID);
	if (thread)
		*thread = th->thread;

	if (!th->thread)
	{
	//	AfxMessageBox("Could not start main lisp thread.");
		return;
	}
	osResumeThread(th->thread);
}

extern int lispSecondary(LispObj func);

//
// Returns the address of the top of the current thread's stack--
// since the stack grows down, this is the *highest* word of the stack
// memory region.
//
unsigned long* getStackStart()
{
	return (unsigned long*)((char*)osStackTop() - 4);
}

// This is the thread procedure for all non-primary lisp threads
// i.e. any lisp thread which is explicitly created from lisp code.
//
UINT
SecondaryThreadProc(LPVOID func)
{
	ThreadRecord* thisThread = 0;
	DWORD currThreadID = osCurrentThreadId();
	HANDLE currThreadHandle = 0;
	LispObj fp = 0;
	LispObj dummy = 0;
	LispObj f = (LispObj)func;

	// find our thread record
	thisThread = ThreadList.getList();
	while (thisThread)
	{
		if (thisThread->threadID == currThreadID)
			break;
		thisThread = thisThread->next;
	}
	if (thisThread->threadID != currThreadID)
	{
//		AfxMessageBox("Can't find our thread ID in the active thread list");
		return 0;
	}

	__asm	mov  dummy, ebp

	thisThread->stackStart = (unsigned long*)dummy;
	osSetThreadSlotValue(Thread_Index, thisThread);
	osSetThreadSlotValue(QV_Index, thisThread->QV_rec);
	osThreadSlotValue(QV_Index);	// force NT to allocate the block
	thisThread->event.Set();

	pushDynamicBinding(COMPILER_RUNTIME, NIL);

	// bind ccl:*current-thread-id*, ccl:*current-thread-handle*
	pushDynamicBinding(CURRENT_THREAD_ID, createUnsignedLispInteger(currThreadID));
	fp = foreignNode();
	UVECTOR(fp)[FOREIGN_PTR] = (LispObj)thisThread->thread;
	pushDynamicBinding(CURRENT_THREAD_HANDLE, fp);

	lispSecondary(f);

	popDynamicBinding(CURRENT_THREAD_ID);
	popDynamicBinding(CURRENT_THREAD_HANDLE);

	// remove the thread record
	ThreadList.remove(thisThread);
 	NumLispThreads--;

	osExitThread(0);
	osCloseThread(currThreadHandle);

	return 0;
}

//	RunSecondaryThread()
//	Starts up a new lisp thread.
LispObj
RunSecondaryThread(LispObj func)
{
	ThreadRecord* th = new ThreadRecord;
	th->QV_rec = createNewQV();
	NumLispThreads++;
	ThreadList.insert(th);

	// make sure garbage collection cannot happen while
	// the thread is being started. This is because the passed
	// function argument may not get updated properly.
	//
	GCCriticalSection.Enter();

    th->type = ThreadRecord::SecondaryThread;
    th->started = 1;

    // create the thread
    th->thread = osCreateThread((OsThreadProc)SecondaryThreadProc, (void*)func, &th->threadID);

    if (!th->thread)
    {
	    return 0;
    }

    osResumeThread(th->thread);
    th->event.Wait();
    th->event.Reset();

	GCCriticalSection.Leave();	// don't enable GC until the thread
	return createUnsignedLispInteger(th->threadID);
}

//
// Call this function to allow a non-lisp thread to become a lisp
// thread. That means it may call lisp functions (defined via defun-callback)
// directly, and the garbage collector will check its stack for references
// into the lisp heap.
//
// After being blessed, it will exist in the LispThreadQueue, and it
// will be considered to be executing foreign code.
//
// This thread will remain in the set of lisp threads until the process
// terminates, unless it explicitly removes itself by calling UnblessThread().
//
extern "C" __declspec(dllexport) void BlessThread()
{
	// If Lisp is initializing, wait for initialization to finish.
	InitializationEvent.Wait();

	GCCriticalSection.Enter();	// make sure garbage collection is not running
//    __try
//    {
	    if (osThreadSlotValue(QV_Index) == 0)
	    {
		    ThreadRecord* th = new ThreadRecord;
		    LispObj fp = 0;
		    HANDLE threadHandle = 0;
		    BOOL ret = FALSE;

		    th->QV_rec = createNewQV();
		    th->stackStart = getStackStart();
		    th->threadID = osCurrentThreadId();
		    threadHandle = osOpenCurrentThread();

		    th->thread = threadHandle;

		    //th->thread = OpenThread(THREAD_ALL_ACCESS, FALSE, th->threadID);
		    th->type = ThreadRecord::BlessedThread;
		    th->started = true;
			th->image_loads_count = getImageLoadsCount();

		    NumLispThreads++;
		    ThreadList.insert(th);
		    osSetThreadSlotValue(Thread_Index, th);
		    osSetThreadSlotValue(QV_Index, th->QV_rec);
		    osThreadSlotValue(QV_Index);	// force NT to allocate the block
		    pushDynamicBinding(COMPILER_RUNTIME, NIL);

		    // bind ccl:*current-thread-id*, ccl:*current-thread-handle*
		    pushDynamicBinding(CURRENT_THREAD_ID, createUnsignedLispInteger(osCurrentThreadId()));
		    fp = foreignNode();
		    UVECTOR(fp)[FOREIGN_PTR] = (LispObj)th->thread;
		    pushDynamicBinding(CURRENT_THREAD_HANDLE, fp);

			// set marker for foreign code transition
			th->QV_rec[STACK_MARKER_INDEX_Index] = 4;	// start at 1st transition (foreign code)
		    th->QV_rec[STACK_MARKERS_Index] = (LispObj)(th->stackStart);
		    th->QV_rec[STACK_MARKERS_Index + 1] = 0;

	    }
//    }
//    __finally
//    {
	    GCCriticalSection.Leave();
//    }
}

// 
// The reverse operation of BlessThread.
// Removes the thread from the lisp thread queue, and it may no
// longer be used to call directly into lisp function.
// It will know longer be checked by the garbage collector for 
// references. It generally shouldn't be necessary to call this function
// unless an application is creating and destroying threads. It should
// be called prior to any "blessed" thread being terminated by the
// application. If it is not called, the open thread handle will prevent
// the thread from being recycled, and cause the garbage collector to
// continue to scan it for references.
//
extern "C" __declspec(dllexport) void UnblessThread()
{
	GCCriticalSection.Enter();	// make sure garbage collection is not running
	ThreadRecord* th = (ThreadRecord*)osThreadSlotValue(Thread_Index);
	osSetThreadSlotValue(Thread_Index, 0);
	osSetThreadSlotValue(QV_Index, 0);
    {
	    if (th)
	    {
			if (th->image_loads_count == getImageLoadsCount())
			{
				popDynamicBinding(CURRENT_THREAD_ID);
				popDynamicBinding(CURRENT_THREAD_HANDLE);
			}
		    osCloseThread(th->thread);

		    // remove the thread record--this also deletes the ThreadRecord
		    ThreadList.remove(th);
		    th = 0;
 		    NumLispThreads--;
	    }
    }
	    GCCriticalSection.Leave();	// make sure garbage collection is not running
}

//
// Returns a direct callback function pointer to be called
// by foreign code (from a blessed thread).
//
extern "C" __declspec(dllexport) 
void* GetCallbackFunctionPointer(wchar_t* functionName, wchar_t* package)
{
	LispObj s = 0;
	LispObj sym = 0;
	LispObj ret = 0;

	// If Lisp is initializing, wait for initialization to finish.
	InitializationEvent.Wait();

	// need to wrap this so page faults get handled during memory
	// allocation
	{
		ExceptionFrame frame(handleStructuredException);
		FOREIGN_TO_LISP()

		s = wstringNode(functionName);
		if (!package)
			sym = s;	// just use string
		else
		{
			// if INTERN is not defined, probably because the lisp image did not
			// load correctly, just return 0 now
			if (!isFunction(symbolFunction(INTERN)))
				return 0;
			sym = LispCall4(Funcall, FUNCALL, INTERN, s, wstringNode(package));
		}
		ret = LispCall3(Funcall, FUNCALL, GET_CALLBACK, sym);

		LISP_RETURN_TO_FOREIGN()
	}

	if (isForeignHeapPtr(ret) || isForeign(ret))
		return (void*)foreignPtr(ret);
	else
		return 0;
}

long GetNumLispThreads()
{
	return NumLispThreads;
}

void ThrowUserException()
{
	LispCall2(Funcall, THROW_SYSTEM_EXCEPTION, EX_USER_ABORT);
}

long user_abort = 0;
static OsRegisters lispContext;

// This function is used by AbortLispThread() to force
// a call to the ThrowUserException() function.
// This simulates a call from the original function,
void __declspec(naked) CallThrowUserExceptionStub()
{
	__asm push eax  ;; push return address
	__asm jmp ThrowUserException;
}

// Aborts the primary thread
void AbortLispThread()
{
	unsigned long addr = 0;
	ThreadRecord* tr = 0;
	HANDLE th = 0;

	// make sure we don't abort a thread while it is performing a garbage collection!
	GCCriticalSection.Enter();
    {
	    tr = ThreadList.getPrimaryThread();
	    if (!tr)
	    {
		    GCCriticalSection.Leave();
		    return;
	    }
	    th = tr->thread;
	    user_abort = 1;
	    osSuspendThread(th);
	    ThreadList.ensureSafeState(tr);
	    osGetThreadRegisters(th, &lispContext);
	    if (!((tr->QV_rec[STACK_MARKER_INDEX_Index] >> 2) & 1))
	    {
		    lispContext.eax = lispContext.eip;
		    lispContext.eip = (long)CallThrowUserExceptionStub;
		    osSetThreadRegisters(th, &lispContext);
	    }
	    else
	    {
		    // cannot abort--executing in foreign code
	    }
	    osResumeThread(th);
    }
	    GCCriticalSection.Leave();
}

static OsRegisters terminateContext;

static void ThrowExitThread(LispObj result)
{
	LispCall4(Funcall, THROW_EXCEPTION, EXIT_THREAD_TAG, result, wrapInteger(1));
}

//	A thread which is being terminated is made to resume here, with the
//	value to return in edi.
void __declspec(naked) TerminateLispThreadException()
{
	__asm push edi
	__asm call ThrowExitThread
}

void TerminateLispThread(LispObj threadID, LispObj ret)
{
	HANDLE h = ThreadList.GetLispThreadHandle(lispIntegerToUnsignedLong(threadID));
	if (h)
	{
		osSuspendThread(h);
		osGetThreadRegisters(h, &terminateContext);
		terminateContext.eip = (long)TerminateLispThreadException;
		terminateContext.edi = (long)ret;
		osSetThreadRegisters(h, &terminateContext);
		osResumeThread(h);
	}
	else
		Error("Not a valid lisp thread ID: ~A", threadID);
}