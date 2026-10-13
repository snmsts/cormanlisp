/*		-------------------------------
 *		Copyright (c) Corman Technologies Inc.
 *		See LICENSE.txt for license information.
 *		-------------------------------
 *
 *		File:		CormanLispHost.h
 *		Contents:	Running the Corman Lisp kernel from a C program.
 */

#ifndef CORMANLISPHOST_H
#define CORMANLISPHOST_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 *	What the kernel asks of the program which hosts it. The kernel calls
 *	these from its own threads. A function which the host has no use for
 *	may be left 0. Each one is passed the context member.
 *
 *	The functions which return a long return 0 when they have done what
 *	was asked.
 */
typedef struct CormanLispHost
{
	void* context;

	/* what Lisp writes to its console; the text is not terminated */
	void (*outputText)(void* context, const char* text, long numChars);

	/* Lisp cannot go on; the text says why */
	void (*shutdown)(void* context, const char* text, long numChars);

	/* a line of status, for a host which shows one */
	void (*setMessage)(void* context, const char* text);
	void (*getMessage)(void* context, char* text, long maxMessageLength);
	void (*setDefaultMessage)(void* context);

	/* for a host which is an editor */
	long (*openEditWindow)(void* context, const char* file, void** window);
	long (*openURL)(void* context, const char* file, void** window);
	long (*addMenu)(void* context, const char* menuName);
	long (*addMenuItem)(void* context, const char* menuName, const char* menuItem);
	void (*replaceSelection)(void* context, const char* text, long numChars);

	/* the handles which the windowing system knows the host by */
	void* (*appInstance)(void* context);
	void* (*appMainWindow)(void* context);
} CormanLispHost;

/* the kinds of host, for clientType */
#define CORMANLISP_WIN_APP_CLIENT	0
#define CORMANLISP_CONSOLE_CLIENT	1
#define CORMANLISP_IDE_CLIENT		2

/*
 *	Sizes in bytes for the heaps, to be given before the kernel is
 *	initialized. A size of 0 leaves the default as it is.
 */
void CormanLispSetHeapSizes(int heapReserve, int heapInitialSize,
		int ephemeralHeap1Size, int ephemeralHeap2Size);

/*
 *	Loads the image, or prepares to build one if imageName is empty. The
 *	host structure is copied. Returns 0 on failure.
 */
long CormanLispInitialize(const CormanLispHost* host, const char* imageName, int clientType);

/*
 *	Starts the first Lisp thread, which reads and evaluates what
 *	CormanLispProcessSource() supplies. If thread is not 0 it receives
 *	the operating system's handle of that thread.
 */
void CormanLispRun(void** thread);

/* Adds to the text which the first Lisp thread reads as its console input. */
void CormanLispProcessSource(const char* text, long numChars);

long CormanLispNumThreads(void);

/* Interrupts what the first Lisp thread is doing, as Control-C does. */
void CormanLispAbortThread(void);

#ifdef __cplusplus
}
#endif

#endif /* CORMANLISPHOST_H */
