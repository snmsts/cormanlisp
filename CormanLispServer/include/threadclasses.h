//		-------------------------------
//		Copyright (c) Corman Technologies Inc.
//		See LICENSE.txt for license information.
//		-------------------------------
//
//		File:		ThreadClasses.h
//		Contents:	Thread synchronization classes for Corman Lisp.
//		History:	8/5/97  RGC Created.
//

#ifndef THREADCLASSES_H
#define THREADCLASSES_H

#include <assert.h>

#include "Os.h"

class Event
{
public:
	Event(bool resetsItself = true) { m_event = osCreateEvent(resetsItself); }
	~Event()		{ osCloseEvent(m_event); }
	void Set()		{ osSetEvent(m_event); }
	void Reset()	{ osResetEvent(m_event); }
	bool Wait(unsigned long milliseconds = OsForever)
					{ return osWaitEvent(m_event, milliseconds); }
public:
	OsEvent m_event;
};

class CriticalSection
{
public:
	CriticalSection() { osInitializeLock(&m_sect); }
	~CriticalSection(){	osDeleteLock(&m_sect); }
	void Enter()	  { osEnterLock(&m_sect); }
	void Leave()	  { osLeaveLock(&m_sect); }
public:
	OsLock m_sect;
};

#endif // THREADCLASSES_H
