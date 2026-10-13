//		-------------------------------
//		Copyright (c) Corman Technologies Inc.
//		See LICENSE.txt for license information.
//		-------------------------------
//
//		File:		TerminalInput.cpp
//		Contents:	The text which the host has supplied for Lisp to read
//					as its console input.
//		History:	8/5/97  RGC Created.
//
#include "Stdafx.h"
#include <string.h>

#include "TerminalInput.h"

TerminalInput::TerminalInput(long length)
{
	fSize = length;
	fBuf = new char[fSize];
	fIndex = 0;
}

TerminalInput::~TerminalInput()
{
	delete [] fBuf;
}

long
TerminalInput::numchars()
{
	return fIndex;
}

long
TerminalInput::getCharsInBuffer(unsigned char* buf, long num)
{
	long taken = 0;
	fLock.Enter();
	if (fIndex > num)
	{
		memcpy(buf, fBuf, num);
		memmove(fBuf, fBuf + num, fIndex - num);
		fIndex -= num;
		taken = num;
	}
	else
	{
		memcpy(buf, fBuf, fIndex);
		taken = fIndex;
		fIndex = 0;
		fBufEmpty.Set();
	}
	fLock.Leave();
	return taken;
}

long
TerminalInput::addChars(const char* buf, long num)
{
	long i;
	fLock.Enter();
	long spaceAvailable = (fSize - 1) - fIndex;
	if (spaceAvailable < num)
		num = spaceAvailable;
	for (i = 0; i < num; i++)
		fBuf[fIndex++] = buf[i];
	if (i > 0)
	{
		fCharsAvailable.Set();
		fLock.Leave();
	}
	else	// if buffer is full, wait until it is empty
	{
		fBufEmpty.Reset();
		fLock.Leave();
		fBufEmpty.Wait(100);
		fBufEmpty.Reset();
	}
	return i;
}

void
TerminalInput::waitForChars(unsigned long milliseconds)
{
	fCharsAvailable.Wait(milliseconds);
}
