/*
 * ergo720                Copyright (c) 2022
 * PatrickvL              Copyright (c) 2017
 */

#include "ke.hpp"
#include "..\hal\hal.hpp"
#include "..\rtl\rtl.hpp"


BOOLEAN KiInsertQueueApc
(
	PKAPC Apc,
	KPRIORITY Increment
)
{
	RIP_UNIMPLEMENTED();
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
