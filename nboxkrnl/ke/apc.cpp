/*
 * ergo720                Copyright (c) 2022
 * PatrickvL              Copyright (c) 2017
 */

#include "ke.hpp"
#include "..\hal\hal.hpp"


EXPORTNUM(101) VOID XBOXAPI KeEnterCriticalRegion()
{
	(*(volatile ULONG *)&KeGetCurrentThread()->KernelApcDisable) -= 1;
}

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

EXPORTNUM(122) VOID XBOXAPI KeLeaveCriticalRegion()
{
	PKTHREAD Thread = KeGetCurrentThread();
	if ((((*(volatile ULONG *)&Thread->KernelApcDisable) += 1) == 0) &&
		!((*(volatile PLIST_ENTRY *)&Thread->ApcState.ApcListHead[KernelMode].Flink) != &Thread->ApcState.ApcListHead[KernelMode])) {
		Thread->ApcState.KernelApcPending = TRUE;
		HalRequestSoftwareInterrupt(APC_LEVEL);
	}
}
