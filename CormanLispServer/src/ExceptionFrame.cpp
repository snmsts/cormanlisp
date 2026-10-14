//		-------------------------------
//		Copyright (c) Corman Technologies Inc.
//		See LICENSE.txt for license information.
//		-------------------------------
//
//		File:		ExceptionFrame.cpp
//		Contents:	Operating system exceptions and non-local exits in the
//					kernel, without the compiler's __try, __except and
//					throw.
//

#include "Stdafx.h"
#include <stddef.h>
#include <stdlib.h>

#include "ExceptionFrame.h"
#include "Os.h"

//	EnterJumpContext() and ResumeJumpContext() address the members by number.
static char contextLayout[
	offsetof(JumpContext, m_eip) == 0 && offsetof(JumpContext, m_esp) == 4 &&
	offsetof(JumpContext, m_ebp) == 8 && offsetof(JumpContext, m_ebx) == 12 &&
	offsetof(JumpContext, m_esi) == 16 && offsetof(JumpContext, m_edi) == 20 ? 1 : -1];

__declspec(naked) int __cdecl EnterJumpContext(JumpContext* context)
{
	__asm
	{
		mov		eax, [esp + 4]		;; eax = context
		mov		edx, [esp]
		mov		[eax], edx			;; the address to return to
		lea		edx, [esp + 4]
		mov		[eax + 4], edx		;; esp after the return
		mov		[eax + 8], ebp
		mov		[eax + 12], ebx
		mov		[eax + 16], esi
		mov		[eax + 20], edi
		mov		eax, 1
		ret
	}
}

__declspec(naked) void __cdecl ResumeJumpContext(JumpContext* context)
{
	__asm
	{
		mov		eax, [esp + 4]		;; eax = context
		mov		edx, [eax]
		mov		ebp, [eax + 8]
		mov		ebx, [eax + 12]
		mov		esi, [eax + 16]
		mov		edi, [eax + 20]
		mov		esp, [eax + 4]
		xor		eax, eax
		jmp		edx
	}
}

#ifdef _WIN32
//	The head of the thread's list of exception handlers, fs:[0].
static EXCEPTION_REGISTRATION_RECORD*& exceptionList()
{
	return ((NT_TIB*)NtCurrentTeb())->ExceptionList;
}
#else
//	Where the operating system keeps no such list, the kernel keeps one.
static __thread EXCEPTION_REGISTRATION_RECORD* ExceptionList = 0;

static EXCEPTION_REGISTRATION_RECORD*& exceptionList()
{
	return ExceptionList;
}

bool DispatchException(EXCEPTION_RECORD* record, CONTEXT* context)
{
	for (EXCEPTION_REGISTRATION_RECORD* r = ExceptionList; r; r = r->Next)
	{
		if (r->Handler(record, r, context, 0) == ExceptionContinueExecution)
			return true;
	}
	return false;
}
#endif

static EXCEPTION_DISPOSITION __cdecl
exceptionFrameHandler(EXCEPTION_RECORD* record, void* registration, CONTEXT* context, void*)
{
	ExceptionFrame* frame = CONTAINING_RECORD(registration, ExceptionFrame, m_record);
	EXCEPTION_POINTERS info = { record, context };
	long action = 0;

	if (record->ExceptionFlags & (EXCEPTION_UNWINDING | EXCEPTION_EXIT_UNWIND))
		return ExceptionContinueSearch;

	action = frame->m_filter(record->ExceptionCode, &info);
	if (action == EXCEPTION_CONTINUE_EXECUTION)
		return ExceptionContinueExecution;
	if (action == EXCEPTION_EXECUTE_HANDLER)
	{
		// resume as if EnterJumpContext() had just returned 0
		context->Eip = frame->m_context.m_eip;
		context->Esp = frame->m_context.m_esp;
		context->Ebp = frame->m_context.m_ebp;
		context->Ebx = frame->m_context.m_ebx;
		context->Esi = frame->m_context.m_esi;
		context->Edi = frame->m_context.m_edi;
		context->Eax = 0;
		return ExceptionContinueExecution;
	}
	return ExceptionContinueSearch;
}

ExceptionFrame::ExceptionFrame(ExceptionFilter filter)
{
	m_filter = filter;
	m_context.m_eip = 0;
	m_record.Handler = (PEXCEPTION_ROUTINE)exceptionFrameHandler;
	m_record.Next = exceptionList();
	exceptionList() = &m_record;
	m_registered = true;
}

ExceptionFrame::~ExceptionFrame()
{
	leave();
}

void ExceptionFrame::leave()
{
	if (m_registered)
	{
		exceptionList() = m_record.Next;
		m_registered = false;
	}
}

//	The handlers which the abandoned code had registered are gone with it.
void ExceptionFrame::abandoned()
{
	exceptionList() = &m_record;
}

//	The innermost KernelCatch of each thread.
static OsThreadSlot KernelCatchIndex = osAllocateThreadSlot();

KernelCatch::KernelCatch()
{
	m_value = 0;
	m_context.m_eip = 0;
	m_handlers = exceptionList();
	m_next = (KernelCatch*)osThreadSlotValue(KernelCatchIndex);
	osSetThreadSlotValue(KernelCatchIndex, this);
	m_registered = true;
}

KernelCatch::~KernelCatch()
{
	leave();
}

void KernelCatch::leave()
{
	if (m_registered)
	{
		osSetThreadSlotValue(KernelCatchIndex, m_next);
		m_registered = false;
	}
}

void KernelThrow(unsigned long value)
{
	KernelCatch* catcher = (KernelCatch*)osThreadSlotValue(KernelCatchIndex);
	if (!catcher)
	{
		osDebugMessage("Error: a kernel error was raised with nothing to catch it!!\n");
		abort();
	}
	catcher->m_value = value;

	// The catchers and the exception handlers set up since are abandoned.
	exceptionList() = catcher->m_handlers;
	ResumeJumpContext(&catcher->m_context);
}
