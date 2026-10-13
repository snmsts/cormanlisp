//		-------------------------------
//		Copyright (c) Corman Technologies Inc.
//		See LICENSE.txt for license information.
//		-------------------------------
//
//		File:		TerminalInput.h
//		Contents:	The text which the host has supplied for Lisp to read
//					as its console input.
//

#ifndef TERMINALINPUT_H
#define TERMINALINPUT_H

#include "threadclasses.h"

//	The host adds text from its thread and Lisp takes it from another.
class TerminalInput
{
public:
	TerminalInput(long length);
	~TerminalInput();
	long numchars();

	long getCharsInBuffer(unsigned char* buf, long num);
	long addChars(const char* buf, long num);

	//	Returns when text has been added, or after that long.
	void waitForChars(unsigned long milliseconds = OsForever);

private:
	long fSize;
	char* fBuf;
	long fIndex;
	CriticalSection fLock;
	Event fCharsAvailable;
	Event fBufEmpty;
};

#endif // TERMINALINPUT_H
