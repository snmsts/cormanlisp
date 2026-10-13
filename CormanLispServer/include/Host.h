//		-------------------------------
//		Copyright (c) Corman Technologies Inc.
//		See LICENSE.txt for license information.
//		-------------------------------
//
//		File:		Host.h
//		Contents:	The kernel's calls to the program which hosts it.
//

#ifndef HOST_H
#define HOST_H

#include "CormanLispHost.h"

//	The host given to CormanLispInitialize() is the one which gets called.
//	Without one, or without the function in question, these do nothing.
extern void hostAttach(const CormanLispHost* host);
extern bool hostAttached();

extern void hostOutputText(const char* text, long numChars);
extern void hostShutdown(const char* text, long numChars);
extern void hostSetMessage(const char* text);
extern void hostGetMessage(char* text, long maxMessageLength);
extern void hostSetDefaultMessage();

//	These return 0 when the host has done what was asked.
extern long hostOpenEditWindow(const char* file, void** window);
extern long hostOpenURL(const char* file, void** window);
extern long hostAddMenu(const char* menuName);
extern long hostAddMenuItem(const char* menuName, const char* menuItem);

extern void hostReplaceSelection(const char* text, long numChars);
extern void* hostAppInstance();
extern void* hostAppMainWindow();

#endif // HOST_H
