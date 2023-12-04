/*
 * ergo720                Copyright (c) 2022
 * PatrickvL              Copyright (c) 2017
 */

#include "ke.hpp"
#include "..\kernel.hpp"
#include "..\ex\ex.hpp"


// Source: Cxbx-Reloaded
EXPORTNUM(112) VOID XBOXAPI KeInitializeSemaphore
(
	PKSEMAPHORE Semaphore,
	LONG Count,
	LONG Limit
)
{
	Semaphore->Header.Type = SemaphoreObject;
	Semaphore->Header.Size = sizeof(KSEMAPHORE) / sizeof(LONG);
	Semaphore->Header.SignalState = Count;
	InitializeListHead(&Semaphore->Header.WaitListHead);
	Semaphore->Limit = Limit;
}

// Source: Cxbx-Reloaded
EXPORTNUM(132) LONG XBOXAPI KeReleaseSemaphore
(
	PKSEMAPHORE Semaphore,
	KPRIORITY Increment,
	LONG Adjustment,
	BOOLEAN Wait
)
{
	UCHAR OldIrql = KeRaiseIrqlToDpcLevel();
	LONG OldState = Semaphore->Header.SignalState;
	LONG NewState = Semaphore->Header.SignalState + Adjustment;

	if ((NewState > Semaphore->Limit) || (NewState < OldState)) {
		KiUnlockDispatcherDatabase(OldIrql);
		ExRaiseStatus(STATUS_SEMAPHORE_LIMIT_EXCEEDED); // won't return
	}
	Semaphore->Header.SignalState = NewState;

	if ((OldState == 0) && (IsListEmpty(&Semaphore->Header.WaitListHead) == FALSE)) {
		KiWaitTest(&Semaphore->Header, Increment);
	}

	if (Wait) {
		PKTHREAD Thread = KeGetCurrentThread();
		Thread->WaitNext = TRUE;
		Thread->WaitIrql = OldIrql;
	}
	else {
		KiUnlockDispatcherDatabase(OldIrql);
	}

	return OldState;
}
