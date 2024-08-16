/*
 * ergo720                Copyright (c) 2022
 * PatrickvL              Copyright (c) 2016
 */

#include "ke.hpp"
#include "ki.hpp"
#include "hal.hpp"
#include "rtl.hpp"


KDPC KeSmbusDpcObject;

// Source: Cxbx-Reloaded
EXPORTNUM(107) VOID XBOXAPI KeInitializeDpc
(
	PKDPC Dpc,
	PKDEFERRED_ROUTINE DeferredRoutine,
	PVOID DeferredContext
)
{
	Dpc->Type = DpcObject;
	Dpc->Inserted = FALSE;
	Dpc->DeferredRoutine = DeferredRoutine;
	Dpc->DeferredContext = DeferredContext;
}

EXPORTNUM(119) BOOLEAN XBOXAPI KeInsertQueueDpc
(
	PKDPC Dpc,
	PVOID SystemArgument1,
	PVOID SystemArgument2
)
{
	KIRQL OldIrql = KfRaiseIrql(HIGH_LEVEL);

	BOOLEAN Inserted = Dpc->Inserted;
	if (!Inserted) {
		Dpc->Inserted = TRUE;
		Dpc->SystemArgument1 = SystemArgument1;
		Dpc->SystemArgument2 = SystemArgument2;
		InsertTailList(&KiPcr.PrcbData.DpcListHead, &Dpc->DpcListEntry);

		if ((KiPcr.PrcbData.DpcRoutineActive == FALSE) && (KiPcr.PrcbData.DpcInterruptRequested == FALSE)) {
			KiPcr.PrcbData.DpcInterruptRequested = TRUE;
			HalRequestSoftwareInterrupt(DISPATCH_LEVEL);
		}
	}

	KfLowerIrql(OldIrql);

	return Inserted == FALSE;
}

// Source: Cxbx-Reloaded
VOID XBOXAPI KiExecuteDpcQueue()
{
	// On entry, interrupts must be disabled

	while (!IsListEmpty(&KiPcr.PrcbData.DpcListHead)) {
		// Extract the head entry and retrieve the containing KDPC pointer for it:
		PKDPC Dpc = CONTAINING_RECORD(RemoveHeadList(&KiPcr.PrcbData.DpcListHead), KDPC, DpcListEntry);
		// Mark it as no longer linked into the DpcQueue
		Dpc->Inserted = FALSE;
		// Set DpcRoutineActive to support KeIsExecutingDpc:
		KiPcr.PrcbData.DpcRoutineActive = TRUE;
		enable();

		// Call the Deferred Procedure:
		Dpc->DeferredRoutine(Dpc, Dpc->DeferredContext, Dpc->SystemArgument1, Dpc->SystemArgument2);

		disable();
		KiPcr.PrcbData.DpcRoutineActive = FALSE;
	}

	KiPcr.PrcbData.DpcInterruptRequested = FALSE;
}

VOID XBOXAPI KeSmbusDpcRoutine(PKDPC Dpc, PVOID DeferredContext, PVOID SystemArgument1, PVOID SystemArgument2)
{
	RIP_UNIMPLEMENTED();
}
