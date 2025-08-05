/*
 * ergo720                Copyright (c) 2023
 */

#include "ps.hpp"
#include "psp.hpp"
#include "..\kernel.hpp"
#include "rtl.hpp"
#include "mm.hpp"
#include "nt.hpp"
#include "xe.hpp"
#include <string.h>


EXPORTNUM(259) OBJECT_TYPE PsThreadObjectType =
{
	ExAllocatePoolWithTag,
	ExFreePool,
	nullptr,
	nullptr,
	nullptr,
	(PVOID)offsetof(KTHREAD, Header),
	'erhT'
};

BOOLEAN PsInitSystem()
{
	KeInitializeDpc(&PspTerminationDpc, PspTerminationRoutine, nullptr);

	HANDLE Handle;
	NTSTATUS Status = PsCreateSystemThread(&Handle, nullptr, XbeStartupThread, nullptr, FALSE);
	
	if (!NT_SUCCESS(Status)) {
		return FALSE;
	}

	NtClose(Handle);

	return TRUE;
}

EXPORTNUM(254) NTSTATUS XBOXAPI PsCreateSystemThread
(
	PHANDLE ThreadHandle,
	PHANDLE ThreadId,
	PKSTART_ROUTINE StartRoutine,
	PVOID StartContext,
	BOOLEAN DebuggerThread
)
{
	return PsCreateSystemThreadEx(ThreadHandle, 0, KERNEL_STACK_SIZE, 0, ThreadId, StartRoutine, StartContext, FALSE, DebuggerThread, PspSystemThreadStartup);
}

EXPORTNUM(255) NTSTATUS XBOXAPI PsCreateSystemThreadEx
(
	PHANDLE ThreadHandle,
	SIZE_T ThreadExtensionSize,
	SIZE_T KernelStackSize,
	SIZE_T TlsDataSize,
	PHANDLE ThreadId,
	PKSTART_ROUTINE StartRoutine,
	PVOID StartContext,
	BOOLEAN CreateSuspended,
	BOOLEAN DebuggerThread,
	PKSYSTEM_ROUTINE SystemRoutine
)
{
	PETHREAD eThread;
	NTSTATUS Status = ObCreateObject(&PsThreadObjectType, nullptr, sizeof(ETHREAD) + ThreadExtensionSize, (PVOID *)&eThread);

	if (!NT_SUCCESS(Status)) {
		return Status;
	}

	memset(eThread, 0, sizeof(ETHREAD) + ThreadExtensionSize);

	InitializeListHead(&eThread->TimerList);
	InitializeListHead(&eThread->IrpList);

	KernelStackSize = ROUND_UP_4K(KernelStackSize);
	if (KernelStackSize < KERNEL_STACK_SIZE) {
		KernelStackSize = KERNEL_STACK_SIZE;
	}

	PVOID KernelStack = MmCreateKernelStack(KernelStackSize, DebuggerThread);
	if (KernelStack == nullptr) {
		ObfDereferenceObject(eThread);
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	eThread->StartAddress = StartRoutine;
	KeInitializeThread(&eThread->Tcb, KernelStack, KernelStackSize, TlsDataSize, SystemRoutine, StartRoutine, StartContext, &KiUniqueProcess);

	if (CreateSuspended) {
		KeSuspendThread(&eThread->Tcb);
	}

	// After this point, failures will be dealt with by PsTerminateSystemThread

	// Increment the ref count of the thread once more, so that we can safely use eThread if this thread terminates before PsCreateSystemThreadEx has finished
	ObfReferenceObject(eThread);

	// Increment the ref count of the thread once more. This is to guard against the case the XBE closes the thread handle manually
	// before this thread terminates with PsTerminateSystemThread
	ObfReferenceObject(eThread);

	Status = ObInsertObject(eThread, nullptr, 0, &eThread->UniqueThread);

	if (NT_SUCCESS(Status)) {
		PspCallThreadNotificationRoutines(eThread, TRUE);
		Status = ObOpenObjectByPointer(eThread, &PsThreadObjectType, ThreadHandle);
	}

	if (!NT_SUCCESS(Status)) {
		eThread->Tcb.HasTerminated = TRUE;
		if (CreateSuspended) {
			KeResumeThread(&eThread->Tcb);
		}
	}
	else {
		if (ThreadId) {
			*ThreadId = eThread->UniqueThread;
		}
	}

	KeQuerySystemTime(&eThread->CreateTime);
	KeScheduleThread(&eThread->Tcb);

	ObfDereferenceObject(eThread);

	return Status;
}

EXPORTNUM(258) VOID XBOXAPI PsTerminateSystemThread
(
	NTSTATUS ExitStatus
)
{
	PKTHREAD kThread = KeGetCurrentThread();
	kThread->HasTerminated = TRUE;

	KfLowerIrql(PASSIVE_LEVEL);

	PETHREAD eThread = (PETHREAD)kThread;
	if (eThread->UniqueThread) {
		PspCallThreadNotificationRoutines(eThread, FALSE);
	}

	if (kThread->Priority < LOW_REALTIME_PRIORITY) {
		KeSetPriorityThread(kThread, LOW_REALTIME_PRIORITY);
	}

	// Cancel I/O
	KIRQL OldIrql = KeRaiseIrqlToDpcLevel(); // synchronize with IofCompleteRequest

	PLIST_ENTRY Entry = eThread->IrpList.Flink;
	while (Entry != &eThread->IrpList) {
		PIRP CurrIrp = CONTAINING_RECORD(Entry, IRP, ThreadListEntry);
		CurrIrp->Cancel = TRUE;
		Entry = Entry->Flink;
	}

	LARGE_INTEGER Timeout{ .QuadPart = -100 * 10000 }; // 100 ms timeout
	ULONG Counter = 0;

	while (!IsListEmpty(&eThread->IrpList)) {
		KfLowerIrql(OldIrql);
		KeDelayExecutionThread(KernelMode, FALSE, &Timeout); // give this thread a chance to complete its packets

		++Counter;
		if (Counter >= 3000) { // 3000 * 100 = 5 minutes wait at maximum
			// Hopefully, this will never happen, as it means that the IRPs were not completed after 5 minutes. Handle this somehow
			RIP_API_MSG("Failed to cancel pending I/O packets after 5 minutes");
		}

		OldIrql = KfRaiseIrql(APC_LEVEL); // synchronize with IopCompleteRequest (it's an APC routine)
	}
	
	if (KeGetCurrentIrql() == DISPATCH_LEVEL) { // happens when there were no pending IRPs to this thread
		KfLowerIrql(OldIrql);
	}

	OldIrql = KeRaiseIrqlToDpcLevel();

	if (!IsListEmpty(&eThread->TimerList)) {
		RIP_API_MSG("Canceling timers not implemented");
	}

	KfLowerIrql(OldIrql);

	// Cancel mutants associated to this thread
	OldIrql = KeRaiseIrqlToDpcLevel();

	Entry = kThread->MutantListHead.Flink;
	while (Entry != &kThread->MutantListHead) {
		PKMUTANT CurrMutant = CONTAINING_RECORD(Entry, KMUTANT, MutantListEntry);

		RemoveEntryList(&CurrMutant->MutantListEntry);
		CurrMutant->Header.SignalState = 1;
		CurrMutant->Abandoned = TRUE;
		CurrMutant->OwnerThread = nullptr;
		if (!IsListEmpty(&CurrMutant->Header.WaitListHead)) {
			KiWaitTest(CurrMutant, PRIORITY_BOOST_MUTANT);
		}

		Entry = Entry->Flink;
	}

	KfLowerIrql(OldIrql);

	KeQuerySystemTime(&eThread->ExitTime);
	eThread->ExitStatus = ExitStatus;

	if (eThread->UniqueThread) {
		NtClose(eThread->UniqueThread);
		eThread->UniqueThread = NULL_HANDLE;
	}

	// Disable APC queueing to this thread by KeInsertQueueApc
	KeEnterCriticalRegion();
	OldIrql = KeRaiseIrqlToDpcLevel();
	kThread->ApcState.ApcQueueable = FALSE;
	if (kThread->SuspendCount) {
		RIP_API_MSG("Resuming a thread while terminating is not implemented");
	}
	KfLowerIrql(OldIrql);
	KeLeaveCriticalRegion();

	// Flush user mode APCs
	OldIrql = KeRaiseIrqlToDpcLevel();

	Entry = kThread->ApcState.ApcListHead[UserMode].Flink;
	while (Entry != &kThread->ApcState.ApcListHead[UserMode]) {
		PKAPC CurrApc = CONTAINING_RECORD(Entry, KAPC, ApcListEntry);

		CurrApc->Inserted = FALSE;

		KfLowerIrql(OldIrql);
		if (CurrApc->RundownRoutine) {
			CurrApc->RundownRoutine(CurrApc);
		}
		else {
			ExFreePool(CurrApc);
		}
		OldIrql = KeRaiseIrqlToDpcLevel();

		Entry = Entry->Flink;
	}

	KfLowerIrql(OldIrql);

	// Because we have set ApcQueueable to FALSE and lowered the IRQL to PASSIVE_LEVEL (which triggers the delivering of APCs), there shouldn't
	// be any pending kernel APC at this point
	if (!IsListEmpty(&kThread->ApcState.ApcListHead[KernelMode])) {
		KeBugCheckEx(KERNEL_APC_PENDING_DURING_EXIT, kThread->KernelApcDisable, 0, 0, 0);
	}

	KeRaiseIrqlToDpcLevel();

	if (kThread->Queue) {
		RIP_API_MSG("Flushing thread queue not implemented");
	}

	kThread->Header.SignalState = 1;
	if (!IsListEmpty(&kThread->Header.WaitListHead)) {
		KiWaitTest(kThread, 0);
	}

	RemoveEntryList(&kThread->ThreadListEntry);

	kThread->State = Terminated;
	kThread->ApcState.Process->StackCount -= 1;

	if (KiPcr.PrcbData.NpxThread == kThread) {
		KiPcr.PrcbData.NpxThread = nullptr;
	}

	InsertTailList(&PspTerminationListHead, &eThread->ReaperLink);
	KeInsertQueueDpc(&PspTerminationDpc, nullptr, nullptr);

	KiSwapThread(); // won't return

	KeBugCheckLogEip(NORETURN_FUNCTION_RETURNED);
}
