/*
 * ergo720                Copyright (c) 2022
 * PatrickvL              Copyright (c) 2017
 */

#include "ke.hpp"
#include "hal.hpp"
#include "rtl.hpp"


static BOOLEAN KiInsertQueueApc(PKAPC Apc, KPRIORITY Increment)
{
	BOOLEAN Inserted = FALSE;
	PKTHREAD Thread = Apc->Thread;

	if (!Apc->Inserted) {
		if (Apc->NormalRoutine) {
			// If there is a NormalRoutine, it's either a user or a normal kernel APC. In both cases, the APC is added to the tail of the list
			InsertTailList(&Thread->ApcState.ApcListHead[Apc->ApcMode], &Apc->ApcListEntry);
		}
		else {
			// This is a special kernel APC. These are added at the head of ApcListHead, before all other normal kernel APCs
			PLIST_ENTRY Entry = Thread->ApcState.ApcListHead[KernelMode].Flink;
			while (Entry != &Thread->ApcState.ApcListHead[KernelMode]) {
				PKAPC CurrApc = CONTAINING_RECORD(Entry, KAPC, ApcListEntry);
				if (CurrApc->NormalRoutine) {
					// We have found the first normal kernel APC in the list, so we add the new APC in front of the found APC
					break;
				}
				Entry = Entry->Flink;
			}
			Entry = Entry->Blink;
			InsertHeadList(Entry, &Apc->ApcListEntry);
		}

		Apc->Inserted = TRUE;

		if (Apc->ApcMode == KernelMode) {
			// If it's a kernel APC, attempt to deliver it right away
			Thread->ApcState.KernelApcPending = TRUE;
			if (Thread->State == Running) {
				// Thread is running, try to preempt it with a APC interrupt
				HalRequestSoftwareInterrupt(APC_LEVEL);
			}
			else if ((Thread->State == Waiting) && (Thread->WaitIrql == PASSIVE_LEVEL) &&
				((Apc->NormalRoutine == nullptr) || ((Thread->KernelApcDisable == 0) && (Thread->ApcState.KernelApcInProgress == FALSE)))) {
				// Thread is waiting (Thread->State == Waiting) and can execute APCs (Thread->WaitIrql == PASSIVE_LEVEL) after the wait. Exit the wait if this is a special
				// kernel APC (Apc->NormalRoutine == nullptr) or it's a normal kernel APC that can execute (Thread->KernelApcDisable == 0) and there isn't another APC
				// in progress (Thread->ApcState.KernelApcInProgress == FALSE)
				KiUnwaitThread(Thread, STATUS_KERNEL_APC, Increment);
			}

		}
		else if ((Thread->State == Waiting) && (Thread->WaitMode == UserMode) && (Thread->Alertable)) {
			// If it's a user APC, only deliver it if the thread is doing an alertable wait in user mode
			Thread->ApcState.UserApcPending = TRUE;
			KiUnwaitThread(Thread, STATUS_USER_APC, Increment);
		}

		Inserted = TRUE;
	}

	return Inserted;
}

VOID XBOXAPI KiExecuteApcQueue()
{
	// This function delivers all normal/special kernel APCs for the current thread. Note that user APCs are never delivered here, because those require the target thread
	// to be waiting. If the current thread is waiting, then it surely cannot execute this code either
	// NOTE: on entry, IRQL must be at APC_LEVEL

	KIRQL OldIrql = KeRaiseIrqlToSynchLevel();
	PKTHREAD Thread = KeGetCurrentThread();
	Thread->ApcState.KernelApcPending = FALSE;

	while (IsListEmpty(&Thread->ApcState.ApcListHead[KernelMode]) == FALSE) {
		PLIST_ENTRY Entry = Thread->ApcState.ApcListHead[KernelMode].Flink;
		PKAPC Apc = CONTAINING_RECORD(Entry, KAPC, ApcListEntry);

		// Create a copy of the APC arguments because the KernelRoutine might delete the APC object
		PKKERNEL_ROUTINE KernelRoutine = Apc->KernelRoutine;
		PKNORMAL_ROUTINE NormalRoutine = Apc->NormalRoutine;
		PVOID NormalContext = Apc->NormalContext;
		PVOID SystemArgument1 = Apc->SystemArgument1;
		PVOID SystemArgument2 = Apc->SystemArgument2;

		if (NormalRoutine == nullptr) {
			// Special kernel APC

			RemoveEntryList(Entry);
			Apc->Inserted = FALSE;
			KfLowerIrql(OldIrql);

			// Special kernel APCs run at APC_LEVEL
			KernelRoutine(Apc, &NormalRoutine, &NormalContext, &SystemArgument1, &SystemArgument2);

			OldIrql = KeRaiseIrqlToSynchLevel();
		}
		else {
			// Normal kernel APC
			if ((Thread->ApcState.KernelApcInProgress == FALSE) && (Thread->KernelApcDisable == 0)) {
				RemoveEntryList(Entry);
				Apc->Inserted = FALSE;
				KfLowerIrql(OldIrql);

				// Special kernel APCs run at APC_LEVEL
				KernelRoutine(Apc, &NormalRoutine, &NormalContext, &SystemArgument1, &SystemArgument2);

				if (NormalRoutine) {
					Thread->ApcState.KernelApcInProgress = TRUE;
					KfLowerIrql(PASSIVE_LEVEL);

					// Normal kernel APCs run at PASSIVE_LEVEL
					NormalRoutine(NormalContext, SystemArgument1, SystemArgument2);

					OldIrql = KfRaiseIrql(APC_LEVEL);
				}

				OldIrql = KeRaiseIrqlToSynchLevel();
				Thread->ApcState.KernelApcInProgress = FALSE;
			}
			else {
				// If we reach here, it means we found a normal kernel APC that we cannot execute, so there's no point in cheking the remaining part of the list
				// because they are all normal kernel APCs too
				break;
			}
		}
	}

	KfLowerIrql(OldIrql);
}

EXPORTNUM(101) VOID XBOXAPI KeEnterCriticalRegion()
{
	(*(volatile ULONG *)&KeGetCurrentThread()->KernelApcDisable) -= 1;
}

// Source: Cxbx-Reloaded
EXPORTNUM(105) VOID XBOXAPI KeInitializeApc
(
	PKAPC Apc,
	PKTHREAD Thread,
	PKKERNEL_ROUTINE KernelRoutine,
	PKRUNDOWN_ROUTINE RundownRoutine,
	PKNORMAL_ROUTINE NormalRoutine,
	KPROCESSOR_MODE ApcMode,
	PVOID NormalContext
)
{
	Apc->Type = ApcObject;
	Apc->ApcMode = ApcMode;
	Apc->Inserted = FALSE;
	Apc->Thread = Thread;
	Apc->KernelRoutine = KernelRoutine;
	Apc->RundownRoutine = RundownRoutine;
	Apc->NormalRoutine = NormalRoutine;
	Apc->NormalContext = NormalContext;
	if (NormalRoutine == nullptr) {
		Apc->ApcMode = KernelMode;
		Apc->NormalContext = nullptr;
	}
}

EXPORTNUM(118) BOOLEAN XBOXAPI KeInsertQueueApc
(
	PKAPC Apc,
	PVOID SystemArgument1,
	PVOID SystemArgument2,
	KPRIORITY Increment
)
{
	KIRQL OldIrql = KeRaiseIrqlToDpcLevel();

	PKTHREAD Thread = Apc->Thread;
	BOOLEAN Inserted = FALSE;
	if (Thread->ApcState.ApcQueueable == TRUE) {
		Apc->SystemArgument1 = SystemArgument1;
		Apc->SystemArgument2 = SystemArgument2;
		Inserted = KiInsertQueueApc(Apc, Increment);
	}

	KiUnlockDispatcherDatabase(OldIrql);

	return Inserted;
}

EXPORTNUM(122) VOID XBOXAPI KeLeaveCriticalRegion()
{
	PKTHREAD Thread = KeGetCurrentThread();
	if ((((*(volatile ULONG *)&Thread->KernelApcDisable) += 1) == 0) &&
		((*(volatile PLIST_ENTRY *)&Thread->ApcState.ApcListHead[KernelMode].Flink) != &Thread->ApcState.ApcListHead[KernelMode])) {
		Thread->ApcState.KernelApcPending = TRUE;
		HalRequestSoftwareInterrupt(APC_LEVEL);
	}
}
