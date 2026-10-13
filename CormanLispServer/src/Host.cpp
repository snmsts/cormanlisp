//		-------------------------------
//		Copyright (c) Corman Technologies Inc.
//		See LICENSE.txt for license information.
//		-------------------------------
//
//		File:		Host.cpp
//		Contents:	The kernel's calls to the program which hosts it, and
//					the functions by which a C program runs the kernel.
//

#include "stdafx.h"
#include <string.h>

#include "Lisp.h"
#include "CormanLispServer.h"
#include "UserInfo.h"
#include "Host.h"

static CormanLispHost Host = { 0 };
static bool HostAttached = false;

void hostAttach(const CormanLispHost* host)
{
	if (host)
	{
		Host = *host;
		HostAttached = true;
	}
}

bool hostAttached()
{
	return HostAttached;
}

void hostOutputText(const char* text, long numChars)
{
	if (Host.outputText)
		Host.outputText(Host.context, text, numChars);
}

void hostShutdown(const char* text, long numChars)
{
	if (Host.shutdown)
		Host.shutdown(Host.context, text, numChars);
}

void hostSetMessage(const char* text)
{
	if (Host.setMessage)
		Host.setMessage(Host.context, text);
}

void hostGetMessage(char* text, long maxMessageLength)
{
	if (Host.getMessage)
		Host.getMessage(Host.context, text, maxMessageLength);
}

void hostSetDefaultMessage()
{
	if (Host.setDefaultMessage)
		Host.setDefaultMessage(Host.context);
}

long hostOpenEditWindow(const char* file, void** window)
{
	if (Host.openEditWindow)
		return Host.openEditWindow(Host.context, file, window);
	return -1;
}

long hostOpenURL(const char* file, void** window)
{
	if (Host.openURL)
		return Host.openURL(Host.context, file, window);
	return -1;
}

long hostAddMenu(const char* menuName)
{
	if (Host.addMenu)
		return Host.addMenu(Host.context, menuName);
	return -1;
}

long hostAddMenuItem(const char* menuName, const char* menuItem)
{
	if (Host.addMenuItem)
		return Host.addMenuItem(Host.context, menuName, menuItem);
	return -1;
}

void hostReplaceSelection(const char* text, long numChars)
{
	if (Host.replaceSelection)
		Host.replaceSelection(Host.context, text, numChars);
}

void* hostAppInstance()
{
	if (Host.appInstance)
		return Host.appInstance(Host.context);
	return 0;
}

void* hostAppMainWindow()
{
	if (Host.appMainWindow)
		return Host.appMainWindow(Host.context);
	return 0;
}

static int clampSize(int size, int smallest, int largest)
{
	if (size < smallest)
		return smallest;
	if (size > largest)
		return largest;
	return size;
}

void CormanLispSetHeapSizes(int heapReserve, int heapInitialSize,
		int ephemeralHeap1Size, int ephemeralHeap2Size)
{
	if (heapReserve != 0)
		LispHeapReserveDefault = clampSize(heapReserve, LispHeapReserveMin, LispHeapReserveMax);
	if (heapInitialSize != 0)
		LispHeapSize = clampSize(heapInitialSize, LispHeapSizeMin, LispHeapSizeMax);
	if (ephemeralHeap1Size != 0)
		EphemeralHeap1Size = clampSize(ephemeralHeap1Size, EphemeralHeap1SizeMin, EphemeralHeap1SizeMax);
	if (ephemeralHeap2Size != 0)
		EphemeralHeap2Size = clampSize(ephemeralHeap2Size, EphemeralHeap2SizeMin, EphemeralHeap2SizeMax);
}

long CormanLispInitialize(const CormanLispHost* host, const char* imageName, int clientType)
{
	static UserInfo* userInfo = 0;

	if (imageName)
		strncpy_s(LispImageName, LispImageNameMax, imageName, LispImageNameMax - 1);
	CormanLispClientType = clientType;
	if (!userInfo)
	{
		UserInfo* info = new UserInfo;
		if (!UserInfo::FillUserInfo(*info))
		{
			delete info;
			return 0;
		}
		userInfo = info;
	}
	InitializeCormanLisp(host, userInfo);
	return 1;
}

void CormanLispRun(void** thread)
{
	RunCormanLisp(thread);
}

void CormanLispProcessSource(const char* text, long numChars)
{
	ProcessLispSource(text, numChars);
}

long CormanLispNumThreads(void)
{
	return GetNumLispThreads();
}

void CormanLispAbortThread(void)
{
	AbortLispThread();
}
