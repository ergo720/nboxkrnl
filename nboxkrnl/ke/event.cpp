/*
 * ergo720                Copyright (c) 2023
 */

#include "ke.hpp"
#include "..\rtl\rtl.hpp"


EXPORTNUM(108) VOID XBOXAPI KeInitializeEvent
(
	PKEVENT Event,
	EVENT_TYPE Type,
	BOOLEAN SignalState
)
{
	Event->Header.Type = Type;
	Event->Header.Size = sizeof(KEVENT) / sizeof(LONG);
	Event->Header.SignalState = SignalState;
	InitializeListHead(&(Event->Header.WaitListHead));
}

EXPORTNUM(145) LONG XBOXAPI KeSetEvent
(
	PKEVENT Event,
	KPRIORITY Increment,
	BOOLEAN	Wait
)
{
	RIP_UNIMPLEMENTED();

	return 0;
}
