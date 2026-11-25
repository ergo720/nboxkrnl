/*
* SPDX-License-Identifier: GPL-2.0+
*
* ergo720                Copyright (c) 2022
* PatrickvL              Copyright (c) 2016
*
* This program is free software; you can redistribute it
* and/or modify it under the terms of the GNU General Public
* License as published by the Free Software Foundation; either
* version 2 of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty
* of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*/

#include "ke.hpp"
#include "ki.hpp"
#include "hal.hpp"


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
