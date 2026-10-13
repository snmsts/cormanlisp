//		-------------------------------
//		Copyright (c) Corman Technologies Inc.
//		See LICENSE.txt for license information.
//		-------------------------------
//
//		File:		ExceptionFrame.h
//		Contents:	Operating system exception handling for a block of
//					code, without the compiler's __try and __except.
//

#ifndef EXCEPTIONFRAME_H
#define EXCEPTIONFRAME_H

#include <windows.h>

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
//	false. Nothing is unwound on the way: whatever the abandoned code had
//	to release is left to the handler. Code whose filter never returns
//	EXCEPTION_EXECUTE_HANDLER does not need EXCEPTION_FRAME_ENTER().
//
struct ExceptionFrame
{
	ExceptionFrame(ExceptionFilter filter);
	~ExceptionFrame();

	// EnterExceptionFrame() and the handler know this layout
	EXCEPTION_REGISTRATION_RECORD m_record;
	ExceptionFilter m_filter;
	unsigned long m_eip;
	unsigned long m_esp;
	unsigned long m_ebp;
	unsigned long m_ebx;
	unsigned long m_esi;
	unsigned long m_edi;
	bool m_registered;

	void abandoned();

	// stops asking the filter, before the frame goes out of scope
	void leave();
};

//	Returns 1, and returns 0 from the same call if the code which follows
//	is abandoned.
extern int __cdecl EnterExceptionFrame(ExceptionFrame* frame);

#define EXCEPTION_FRAME_ENTER(frame) \
	(EnterExceptionFrame(&(frame)) ? true : ((frame).abandoned(), false))

#endif // EXCEPTIONFRAME_H
