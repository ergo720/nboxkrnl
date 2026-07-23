/*
 * ergo720                Copyright (c) 2022
 * PatrickvL              Copyright (c) 2016
 */

#include "ki.hpp"
#include "rtl.hpp"
#include "hal.hpp"
#include "dbg.hpp"
#include "..\kernel.hpp"
#include "assert.h"


VOID XBOXAPI KeInitializeTimer(PKTIMER Timer)
{
	KeInitializeTimerEx(Timer, NotificationTimer);
}

VOID FASTCALL KiCheckExpiredTimers(DWORD OldKeTickCount)
{
	// NOTE1: this is only called by HalpClockIsr at CLOCK_LEVEL with interrupts enabled
	// NOTE2: we cannot just check a single table index because HalpClockIsr is not guaranteed to be called every ms

	ULONG EndKeTickCount = KeTickCount;
	if ((KeTickCount - OldKeTickCount) > TIMER_TABLE_SIZE) {
		EndKeTickCount = OldKeTickCount + TIMER_TABLE_SIZE;
	}

	for (ULONG StartKeTickCount = OldKeTickCount; StartKeTickCount < EndKeTickCount; ++StartKeTickCount) {
		ULONG Index = StartKeTickCount & (TIMER_TABLE_SIZE - 1);
		if (IsListEmpty(&KiTimerTableListHead[Index]) == FALSE) {
			PLIST_ENTRY NextEntry = KiTimerTableListHead[Index].Flink;
			PKTIMER Timer = CONTAINING_RECORD(NextEntry, KTIMER, TimerListEntry);
			if (((ULONG)KeInterruptTime.HighTime < Timer->DueTime.HighPart) ||
				(((ULONG)KeInterruptTime.HighTime == Timer->DueTime.HighPart) &&
				(KeInterruptTime.LowTime < Timer->DueTime.LowPart))) {
				continue;
			}

			// Don't call KeInsertQueueDpc here because that raises the IRQL
			disable();
			if (!KiTimerExpireDpc.Inserted) {
				KiTimerExpireDpc.Inserted = TRUE;
				KiTimerExpireDpc.SystemArgument1 = (PVOID)(ULONG_PTR)OldKeTickCount;
				KiTimerExpireDpc.SystemArgument2 = (PVOID)(ULONG_PTR)EndKeTickCount;
				InsertTailList(&KiPcr.PrcbData.DpcListHead, &KiTimerExpireDpc.DpcListEntry);

				if ((KiPcr.PrcbData.DpcRoutineActive == FALSE) && (KiPcr.PrcbData.DpcInterruptRequested == FALSE)) {
					KiPcr.PrcbData.DpcInterruptRequested = TRUE;
					HalRequestSoftwareInterrupt(DISPATCH_LEVEL);
				}
			}
			enable();
			break;
		}
	}
}

VOID KiRemoveTimer(PKTIMER Timer)
{
	Timer->Header.Inserted = FALSE;
	RemoveEntryList(&Timer->TimerListEntry);
}

// Source: Cxbx-Reloaded
ULONG KiComputeTimerTableIndex(ULONGLONG DueTime)
{
	return (DueTime / CLOCK_TIME_INCREMENT) & (TIMER_TABLE_SIZE - 1);
}

static BOOLEAN KiInsertTimerInTimerTable(LARGE_INTEGER RelativeTime, LARGE_INTEGER CurrentTime, PKTIMER Timer)
{
	// NOTE: RelativeTime must be negative, it's the 100ns time increments before the timer is due
	assert(RelativeTime.QuadPart < 0);

	Timer->DueTime.QuadPart = CurrentTime.QuadPart - RelativeTime.QuadPart;
	PLIST_ENTRY ListHead = &KiTimerTableListHead[KiComputeTimerTableIndex(Timer->DueTime.QuadPart)];
	PLIST_ENTRY NextEntry = ListHead->Blink;
	while (NextEntry != ListHead) {
		PKTIMER NextTimer = CONTAINING_RECORD(NextEntry, KTIMER, TimerListEntry);
		if (((Timer->DueTime.HighPart == NextTimer->DueTime.HighPart) &&
			(Timer->DueTime.LowPart >= NextTimer->DueTime.LowPart)) ||
			(Timer->DueTime.HighPart > NextTimer->DueTime.HighPart)) {

			InsertHeadList(NextEntry, &Timer->TimerListEntry);
			return TRUE;
		}

		NextEntry = NextEntry->Blink;
	}

	InsertHeadList(ListHead, &Timer->TimerListEntry);
	CurrentTime.QuadPart = KeQueryInterruptTime();
	if (((Timer->DueTime.HighPart == (ULONG)CurrentTime.HighPart) &&
		(Timer->DueTime.LowPart <= CurrentTime.LowPart)) ||
		(Timer->DueTime.HighPart < (ULONG)CurrentTime.HighPart)) {

		KiRemoveTimer(Timer);
		Timer->Header.SignalState = TRUE;
	}

	return Timer->Header.Inserted;
}

PLARGE_INTEGER KiRecalculateTimerDueTime(PLARGE_INTEGER OriginalTime, PLARGE_INTEGER DueTime, PLARGE_INTEGER NewTime)
{
	if (OriginalTime->QuadPart >= 0) {
		return OriginalTime;
	}

	NewTime->QuadPart = KeQueryInterruptTime();
	NewTime->QuadPart -= DueTime->QuadPart;
	return NewTime;
}

BOOLEAN KiInsertTimer(PKTIMER Timer, LARGE_INTEGER DueTime)
{
	LARGE_INTEGER RelativeTime = DueTime;
	Timer->Header.Inserted = TRUE;
	Timer->Header.Absolute = FALSE;
	if (Timer->Period == 0) {
		Timer->Header.SignalState = FALSE;
	}

	// If DueTime is positive or zero, then it's an absolute timer
	if (DueTime.HighPart >= 0) {
		LARGE_INTEGER SystemTime, TimeDifference;
		KeQuerySystemTime(&SystemTime);
		TimeDifference.QuadPart = SystemTime.QuadPart - DueTime.QuadPart;

		// If TimeDifference is positive or zero, then SystemTime >= DueTime so the timer already expired
		if (TimeDifference.HighPart >= 0) {
			Timer->Header.SignalState = TRUE;
			Timer->Header.Inserted = FALSE;
			return FALSE;
		}

		RelativeTime = TimeDifference;
		Timer->Header.Absolute = TRUE;
	}

	LARGE_INTEGER CurrentTime;
	CurrentTime.QuadPart = KeQueryInterruptTime();
	return KiInsertTimerInTimerTable(RelativeTime, CurrentTime, Timer);
}

BOOLEAN KiReinsertTimer(PKTIMER Timer, ULARGE_INTEGER DueTime)
{
	Timer->Header.Inserted = TRUE;
	if (Timer->Period == 0) {
		Timer->Header.SignalState = FALSE;
	}

	LARGE_INTEGER CurrentTime, TimeDifference;
	CurrentTime.QuadPart = KeQueryInterruptTime();
	TimeDifference.QuadPart = CurrentTime.QuadPart - DueTime.QuadPart;

	// If TimeDifference is positive or zero, then SystemTime >= DueTime so the timer already expired
	if (TimeDifference.HighPart >= 0) {
		Timer->Header.SignalState = TRUE;
		Timer->Header.Inserted = FALSE;
		return FALSE;
	}

	return KiInsertTimerInTimerTable(TimeDifference, CurrentTime, Timer);
}

// Source: Cxbx-Reloaded
VOID KiTimerListExpire(PLIST_ENTRY ExpiredListHead, KIRQL OldIrql)
{
	ULARGE_INTEGER SystemTime;
	LARGE_INTEGER Interval;
	ULONG DpcCalls;
	PKTIMER Timer;
	PKDPC TimerDpc;
	ULONG Period;
	DPC_QUEUE_ENTRY DpcEntry[MAX_TIMER_DPCS];

	/* Query system */
	KeQuerySystemTime((PLARGE_INTEGER)&SystemTime);

	/* Loop expired list */
ProcessDpcs:
	DpcCalls = 0;
	while (ExpiredListHead->Flink != ExpiredListHead) {
		/* Get the current timer */
		Timer = CONTAINING_RECORD(ExpiredListHead->Flink, KTIMER, TimerListEntry);

		/* Remove it */
		RemoveEntryList(&Timer->TimerListEntry);

		/* Not inserted */
		Timer->Header.Inserted = FALSE;

		/* Signal it */
		Timer->Header.SignalState = 1;

		/* Get the DPC and period */
		TimerDpc = Timer->Dpc;
		Period = Timer->Period;

		/* Check if there's any waiters */
		if (!IsListEmpty(&Timer->Header.WaitListHead)) {
			KiWaitTest(Timer, PRIORITY_BOOST_TIMER);
		}

		/* Check if we have a period */
		if (Period) {
			/* Calculate the interval and insert the timer */
			Interval.QuadPart = Period * -10000LL;
			while (!KiInsertTimer(Timer, Interval));
		}

		/* Check if we have a DPC */
		if (TimerDpc) {
			/* Setup the DPC Entry */
			DpcEntry[DpcCalls].Dpc = TimerDpc;
			DpcEntry[DpcCalls].Routine = TimerDpc->DeferredRoutine;
			DpcEntry[DpcCalls].Context = TimerDpc->DeferredContext;
			DpcCalls++;
			if (DpcCalls == MAX_TIMER_DPCS) {
				break;
			}
		}
	}

	/* Check if we still have DPC entries */
	if (DpcCalls) {
		/* Release the dispatcher while doing DPCs */
		KiUnlockDispatcherDatabase(DISPATCH_LEVEL);

		/* Start looping all DPC Entries */
		for (ULONG i = 0; i < DpcCalls; ++i) {
			/* Call the DPC */
			DpcEntry[i].Routine(
				DpcEntry[i].Dpc,
				DpcEntry[i].Context,
				PVOID(SystemTime.u.LowPart),
				PVOID(SystemTime.u.HighPart)
			);
		}

		if (DpcCalls == MAX_TIMER_DPCS) {
			assert(KeGetCurrentIrql() == DISPATCH_LEVEL);
			goto ProcessDpcs;
		}

		/* Lower IRQL */
		KfLowerIrql(OldIrql);
	}
	else {
		/* Unlock the dispatcher */
		KiUnlockDispatcherDatabase(OldIrql);
	}
}

// Source: Cxbx-Reloaded
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

EXPORTNUM(149) BOOLEAN XBOXAPI KeSetTimer
(
	PKTIMER Timer,
	LARGE_INTEGER DueTime,
	PKDPC Dpc
)
{
	return KeSetTimerEx(Timer, DueTime, 0, Dpc);
}

EXPORTNUM(150) BOOLEAN XBOXAPI KeSetTimerEx
(
	PKTIMER Timer,
	LARGE_INTEGER DueTime,
	LONG Period,
	PKDPC Dpc
)
{
	KIRQL OldIrql = KeRaiseIrqlToDpcLevel();

	UCHAR Inserted = Timer->Header.Inserted;
	if (Inserted) {
		KiRemoveTimer(Timer);
	}

	Timer->Header.SignalState = FALSE;
	Timer->Dpc = Dpc;
	Timer->Period = Period;
	if (KiInsertTimer(Timer, DueTime) == FALSE) {
		if (IsListEmpty(&Timer->Header.WaitListHead) == FALSE) {
			RIP_API_MSG("Unwaiting threads is not supported");
		}

		if (Dpc) {
			LARGE_INTEGER SystemTime;
			KeQuerySystemTime(&SystemTime);
			KeInsertQueueDpc(Timer->Dpc, (PVOID)(ULONG_PTR)SystemTime.LowPart, (PVOID)(ULONG_PTR)SystemTime.HighPart);
		}

		if (Period) {
			// NOTE: Period is expressed in ms
			LARGE_INTEGER PeriodicTime;
			PeriodicTime.QuadPart = (LONGLONG)Period * (LONGLONG)CLOCK_TIME_INCREMENT * -1LL;
			while (!KiInsertTimer(Timer, PeriodicTime)) {}
		}
	}

	KfLowerIrql(OldIrql);

	return Inserted;
}

#if _DEBUG
static VOID KiCheckTimerTable (ULARGE_INTEGER CurrentTime)
{
	KIRQL OldIrql = KfRaiseIrql(HIGH_LEVEL);

	for (DWORD Index = 0; Index < TIMER_TABLE_SIZE; ++Index) {
		PLIST_ENTRY ListHead = &KiTimerTableListHead[Index];
		PLIST_ENTRY NextEntry = ListHead->Flink;
		while (NextEntry != ListHead) {
			PKTIMER Timer = CONTAINING_RECORD(NextEntry, KTIMER, TimerListEntry);
			NextEntry = NextEntry->Flink;
			if (Timer->DueTime.QuadPart <= CurrentTime.QuadPart) {
				DbgPrint("Found expired timer -> Timer=0x%p, Period=%d, Dpc=0x%p, DueTime=0x%016llX, CurrentTime=0x%016llX",
					Timer, Timer->Period, Timer->Dpc, Timer->DueTime.QuadPart, CurrentTime.QuadPart);
				DbgBreakPoint();
			}
		}
	}

	KfLowerIrql(OldIrql);
}
#endif

VOID XBOXAPI KiTimerExpiration(PKDPC Dpc, PVOID DeferredContext, PVOID SystemArgument1, PVOID SystemArgument2)
{
	assert(KeGetCurrentIrql() == DISPATCH_LEVEL);

	ULARGE_INTEGER InterruptTime{ .QuadPart = KeQueryInterruptTime() };
	DWORD OldKeTickCount = (ULONG_PTR)(SystemArgument1);
	DWORD EndKeTickCount = (ULONG_PTR)(SystemArgument2);
	LIST_ENTRY TempList;
	InitializeListHead(&TempList);

	for (DWORD i = OldKeTickCount; i < EndKeTickCount; ++i) {
		DWORD Index = i & (TIMER_TABLE_SIZE - 1);
		PLIST_ENTRY ListHead = &KiTimerTableListHead[Index];
		PLIST_ENTRY NextEntry = ListHead->Flink;

		while (ListHead != NextEntry) {
			PKTIMER Timer = CONTAINING_RECORD(NextEntry, KTIMER, TimerListEntry);
			if (Timer->DueTime.QuadPart <= InterruptTime.QuadPart) {
				// This timer expired, so remove it from KiTimerTableListHead and add it to the expired timer list
				RemoveEntryList(&Timer->TimerListEntry);
				InsertTailList(&TempList, &Timer->TimerListEntry);
				NextEntry = ListHead->Flink;
			}
			else {
				// Timers are in ascending order of due time in KiTimerTableListHead, so the remaining timers cannot have expired
				break;
			}
		}
	}

	/* Verify the timer table, on a debug kernel only */
#if _DEBUG
	KiCheckTimerTable(InterruptTime);
#endif

	KiTimerListExpire(&TempList, DISPATCH_LEVEL);
}
