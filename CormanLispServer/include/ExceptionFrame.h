//		-------------------------------
//		Copyright (c) Corman Technologies Inc.
//		See LICENSE.txt for license information.
//		-------------------------------
//
//		File:		ExceptionFrame.h
//		Contents:	Operating system exceptions and non-local exits in the
//					kernel, without the compiler's __try, __except and
//					throw.
//

#ifndef EXCEPTIONFRAME_H
#define EXCEPTIONFRAME_H

#include "Stdafx.h"

//
//	A place in a function to come back to, abandoning whatever has been
//	called from there since. The frames in between may be those of
//	compiled Lisp code, which no compiler's unwinder knows how to walk,
//	so they are not unwound: the registers are set back, and that is all.
//
struct JumpContext
{
	// EnterJumpContext() and ResumeJumpContext() know this layout
	unsigned long m_eip;
	unsigned long m_esp;
	unsigned long m_ebp;
	unsigned long m_ebx;
	unsigned long m_esi;
	unsigned long m_edi;
};

//	Returns 1, and returns 0 from the same call when the context is resumed.
extern int __cdecl EnterJumpContext(JumpContext* context);

//	Does not return.
extern void __cdecl ResumeJumpContext(JumpContext* context);

//
//	An ExceptionFilter looks at an exception and returns one of
//	EXCEPTION_CONTINUE_EXECUTION, EXCEPTION_CONTINUE_SEARCH and
//	EXCEPTION_EXECUTE_HANDLER, as the expression of __except does.
//
typedef long (*ExceptionFilter)(long exception, LPEXCEPTION_POINTERS info);

//
//	While an ExceptionFrame exists, its filter is asked about the
//	exceptions raised by the thread which created it. It has to be a local
//	variable: the operating system only accepts handlers which are
//	registered on the stack.
//
//		ExceptionFrame frame(filter);
//		if (EXCEPTION_FRAME_ENTER(frame))
//		{
//			...					// the guarded code
//		}
//		else
//		{
//			...					// the filter returned EXCEPTION_EXECUTE_HANDLER
//		}
//
//	When the filter returns EXCEPTION_EXECUTE_HANDLER the guarded code is
//	abandoned and EXCEPTION_FRAME_ENTER() returns a second time, with
//	false. Whatever the abandoned code had to release is left to the
//	handler. Code whose filter never returns EXCEPTION_EXECUTE_HANDLER
//	does not need EXCEPTION_FRAME_ENTER().
//
struct ExceptionFrame
{
	ExceptionFrame(ExceptionFilter filter);
	~ExceptionFrame();

	EXCEPTION_REGISTRATION_RECORD m_record;
	ExceptionFilter m_filter;
	JumpContext m_context;
	bool m_registered;

	void abandoned();

	// stops asking the filter, before the frame goes out of scope
	void leave();
};

#define EXCEPTION_FRAME_ENTER(frame) \
	(EnterJumpContext(&(frame).m_context) ? true : ((frame).abandoned(), false))

//
//	The kernel's own errors, raised with KernelThrow(), end up at the
//	innermost KernelCatch of the thread:
//
//		KernelCatch catcher;
//		if (KERNEL_CATCH_ENTER(catcher))
//		{
//			...					// may call KernelThrow()
//		}
//		else
//		{
//			...					// catcher.m_value is what was thrown
//		}
//
//	The catcher stops catching before the second branch is entered. Like
//	an ExceptionFrame, a KernelCatch has to be a local variable.
//
struct KernelCatch
{
	KernelCatch();
	~KernelCatch();

	KernelCatch* m_next;
	EXCEPTION_REGISTRATION_RECORD* m_handlers;
	unsigned long m_value;			// a LispObj
	JumpContext m_context;
	bool m_registered;

	// stops catching, before the catcher goes out of scope
	void leave();
};

#define KERNEL_CATCH_ENTER(catcher) \
	(EnterJumpContext(&(catcher).m_context) ? true : ((catcher).leave(), false))

//	Does not return.
extern void KernelThrow(unsigned long value);

#endif // EXCEPTIONFRAME_H
