/*
 * ergo720                Copyright (c) 2022
 * cxbxr devs
 */

#include "..\ki\ki.hpp"
#include "..\rtl\rtl.hpp"
#include "..\kernel.hpp"


EXPORTNUM(156) volatile DWORD KeTickCount = 0;

VOID XBOXAPI KeInitializeTimer(PKTIMER Timer)
{
	KeInitializeTimerEx(Timer, NotificationTimer);
}

EXPORTNUM(113) VOID XBOXAPI KeInitializeTimerEx
(
	PKTIMER Timer,
	TIMER_TYPE Type
)
{
	Timer->Header.Type = Type + TimerNotificationObject;
	Timer->Header.Size = sizeof(KTIMER) / sizeof(ULONG);
	Timer->Header.Inserted = FALSE;
	Timer->Header.SignalState = 0;
	InitializeListHead(&(Timer->Header.WaitListHead));
	Timer->TimerListEntry.Flink = nullptr;
	Timer->TimerListEntry.Blink = nullptr;
	Timer->DueTime.QuadPart = 0;
	Timer->Period = 0;
}

VOID XBOXAPI KiTimerExpiration(PKDPC Dpc, PVOID DeferredContext, PVOID SystemArgument1, PVOID SystemArgument2)
{
	// TODO
	RIP_UNIMPLEMENTED();
}
