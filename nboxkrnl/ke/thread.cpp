/*
 * ergo720                Copyright (c) 2022
 * RadWolfie              Copyright (c) 2022
 */

#include "ke.hpp"
#include "..\ki\ki.hpp"
#include "..\kernel.hpp"
#include "..\mm\mm.hpp"
#include "..\hal\halp.hpp"
#include "..\rtl\rtl.hpp"
#include <string.h>


VOID XBOXAPI KiSuspendNop(PKAPC Apc, PKNORMAL_ROUTINE *NormalRoutine, PVOID *NormalContext, PVOID *SystemArgument1, PVOID *SystemArgument2) {}

VOID XBOXAPI KiSuspendThread(PVOID NormalContext, PVOID SystemArgument1, PVOID SystemArgument)
{
	// TODO
	RIP_UNIMPLEMENTED();
}

VOID XBOXAPI KiThreadStartup()
{
	// TODO
	RIP_UNIMPLEMENTED();
}

VOID KiInitializeContextThread(PKTHREAD Thread, ULONG TlsDataSize, PKSYSTEM_ROUTINE SystemRoutine, PKSTART_ROUTINE StartRoutine, PVOID StartContext)
{
	ULONG_PTR StackAddress = reinterpret_cast<ULONG_PTR>(Thread->StackBase);

	StackAddress -= sizeof(FX_SAVE_AREA);
	PFX_SAVE_AREA FxSaveArea = reinterpret_cast<PFX_SAVE_AREA>(StackAddress);
	memset(FxSaveArea, 0, sizeof(FX_SAVE_AREA));

	FxSaveArea->FloatSave.ControlWord = 0x27F;
	FxSaveArea->FloatSave.MXCsr = 0x1F80;

	Thread->NpxState = NPX_STATE_NOT_LOADED;

	TlsDataSize = ROUND_UP(TlsDataSize, 4);
	StackAddress -= TlsDataSize;
	if (TlsDataSize) {
		Thread->TlsData = reinterpret_cast<PVOID>(StackAddress);
		memset(Thread->TlsData, 0, TlsDataSize);
	}
	else {
		Thread->TlsData = nullptr;
	}

	StackAddress -= sizeof(KSTART_FRAME);
	PKSTART_FRAME StartFrame = reinterpret_cast<PKSTART_FRAME>(StackAddress);
	StackAddress -= sizeof(KSWITCHFRAME);
	PKSWITCHFRAME CtxSwitchFrame = reinterpret_cast<PKSWITCHFRAME>(StackAddress);

	StartFrame->StartContext = StartContext;
	StartFrame->StartRoutine = StartRoutine;
	StartFrame->SystemRoutine = SystemRoutine;

	CtxSwitchFrame->RetAddr = KiThreadStartup;
	CtxSwitchFrame->Eflags = 0x200; // interrupt enable flag
	CtxSwitchFrame->ExceptionList = reinterpret_cast<PVOID>(EXCEPTION_CHAIN_END);

	Thread->KernelStack = reinterpret_cast<PVOID>(CtxSwitchFrame);
}

VOID KeInitializeThread(PKTHREAD Thread, PVOID KernelStack, ULONG KernelStackSize, ULONG TlsDataSize, PKSYSTEM_ROUTINE SystemRoutine, PKSTART_ROUTINE StartRoutine,
	PVOID StartContext, PKPROCESS Process)
{
	Thread->Header.Type = ThreadObject;
	Thread->Header.Size = sizeof(KTHREAD) / sizeof(LONG);
	Thread->Header.SignalState = 0;
	InitializeListHead(&Thread->Header.WaitListHead);

	InitializeListHead(&Thread->MutantListHead);

	InitializeListHead(&Thread->ApcState.ApcListHead[KernelMode]);
	InitializeListHead(&Thread->ApcState.ApcListHead[UserMode]);
	Thread->KernelApcDisable = FALSE;
	Thread->ApcState.Process = Process;
	Thread->ApcState.ApcQueueable = TRUE;

	KeInitializeApc(
		&Thread->SuspendApc,
		Thread,
		KiSuspendNop,
		nullptr,
		KiSuspendThread,
		KernelMode,
		nullptr);

	KeInitializeSemaphore(&Thread->SuspendSemaphore, 0, 2);

	KeInitializeTimer(&Thread->Timer);
	PKWAIT_BLOCK TimerWaitBlock = &Thread->TimerWaitBlock;
	TimerWaitBlock->Object = &Thread->Timer;
	TimerWaitBlock->WaitKey = static_cast<CSHORT>(STATUS_TIMEOUT);
	TimerWaitBlock->WaitType = WaitAny;
	TimerWaitBlock->Thread = Thread;
	TimerWaitBlock->NextWaitBlock = nullptr;
	TimerWaitBlock->WaitListEntry.Flink = &Thread->Timer.Header.WaitListHead;
	TimerWaitBlock->WaitListEntry.Blink = &Thread->Timer.Header.WaitListHead;

	Thread->StackBase = KernelStack;
	Thread->StackLimit = reinterpret_cast<PVOID>(reinterpret_cast<LONG_PTR>(KernelStack) - KernelStackSize);

	KiInitializeContextThread(Thread, TlsDataSize, SystemRoutine, StartRoutine, StartContext);

	Thread->DisableBoost = Process->DisableBoost;
	Thread->Quantum = Process->ThreadQuantum;
	Thread->Priority = Process->BasePriority;
	Thread->BasePriority = Process->BasePriority;
	Thread->State = Initialized;

	KIRQL OldIrql = KeRaiseIrqlToDpcLevel();

	InsertTailList(&Process->ThreadListHead, &Thread->ThreadListEntry);
	Process->StackCount++;

	KfLowerIrql(OldIrql);
}

EXPORTNUM(140) ULONG XBOXAPI KeResumeThread
(
	PKTHREAD Thread
)
{
	RIP_UNIMPLEMENTED();

	return 1;
}

EXPORTNUM(152) ULONG XBOXAPI KeSuspendThread
(
	PKTHREAD Thread
)
{
	RIP_UNIMPLEMENTED();

	return 0;
}

EXPORTNUM(155) BOOLEAN XBOXAPI KeTestAlertThread
(
	KPROCESSOR_MODE AlertMode
)
{
	// TODO
	RIP_UNIMPLEMENTED();

	return FALSE;
}
