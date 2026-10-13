//		-------------------------------
//		Copyright (c) Corman Technologies Inc.
//		See LICENSE.txt for license information.
//		-------------------------------
//
//		File:		ExceptionFrame.cpp
//		Contents:	Operating system exception handling for a block of
//					code, without the compiler's __try and __except.
//

#include "stdafx.h"
#include <stddef.h>

#include "ExceptionFrame.h"

//	The head of the thread's list of exception handlers, fs:[0].
static EXCEPTION_REGISTRATION_RECORD*& exceptionList()
{
	return ((NT_TIB*)NtCurrentTeb())->ExceptionList;
}

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
		// resume as if EnterExceptionFrame() had just returned 0
		context->Eip = frame->m_eip;
		context->Esp = frame->m_esp;
		context->Ebp = frame->m_ebp;
		context->Ebx = frame->m_ebx;
		context->Esi = frame->m_esi;
		context->Edi = frame->m_edi;
		context->Eax = 0;
		return ExceptionContinueExecution;
	}
	return ExceptionContinueSearch;
}

ExceptionFrame::ExceptionFrame(ExceptionFilter filter)
{
	m_filter = filter;
	m_eip = 0;
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

//	EnterExceptionFrame() below addresses the members by number.
static char frameLayout[
	offsetof(ExceptionFrame, m_eip) == 12 && offsetof(ExceptionFrame, m_esp) == 16 &&
	offsetof(ExceptionFrame, m_ebp) == 20 && offsetof(ExceptionFrame, m_ebx) == 24 &&
	offsetof(ExceptionFrame, m_esi) == 28 && offsetof(ExceptionFrame, m_edi) == 32 ? 1 : -1];

__declspec(naked) int __cdecl EnterExceptionFrame(ExceptionFrame* frame)
{
	__asm
	{
		mov		eax, [esp + 4]		;; eax = frame
		mov		edx, [esp]
		mov		[eax + 12], edx		;; the address to return to
		lea		edx, [esp + 4]
		mov		[eax + 16], edx		;; esp after the return
		mov		[eax + 20], ebp
		mov		[eax + 24], ebx
		mov		[eax + 28], esi
		mov		[eax + 32], edi
		mov		eax, 1
		ret
	}
}
