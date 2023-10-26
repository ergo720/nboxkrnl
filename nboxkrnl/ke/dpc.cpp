/*
 * ergo720                Copyright (c) 2022
 * PatrickvL              Copyright (c) 2016
 */

#include "ke.hpp"
#include "..\ki\ki.hpp"


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
		__asm sti

		// Call the Deferred Procedure:
		Dpc->DeferredRoutine(Dpc, Dpc->DeferredContext, Dpc->SystemArgument1, Dpc->SystemArgument2);

		__asm cli
		KiPcr.PrcbData.DpcRoutineActive = FALSE;
	}

	KiPcr.PrcbData.DpcInterruptRequested = FALSE;
}
