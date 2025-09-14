/*
 * ergo720                Copyright (c) 2022
 * RadWolfie              Copyright (c) 2022
 */

#include "ke.hpp"
#include "ki.hpp"
#include "..\kernel.hpp"
#include "mm.hpp"
#include "halp.hpp"
#include "rtl.hpp"
#include "ps.hpp"
#include "hal.hpp"
#include <string.h>
#include <assert.h>


VOID XBOXAPI KiSuspendNop(PKAPC Apc, PKNORMAL_ROUTINE *NormalRoutine, PVOID *NormalContext, PVOID *SystemArgument1, PVOID *SystemArgument2) {}

VOID XBOXAPI KiSuspendThread(PVOID NormalContext, PVOID SystemArgument1, PVOID SystemArgument)
{
	// TODO
	RIP_UNIMPLEMENTED();
}

[[noreturn]] static VOID __declspec(naked) XBOXAPI KiThreadStartup()
{
	// This function is called from KiSwapThreadContext when a new thread is run for the first time ever
	// On entry, the esp points to a PKSTART_FRAME

	// clang-format off
	__asm {
		mov cl, PASSIVE_LEVEL
		call KfLowerIrql
		mov ecx, dword ptr [KiPcr]KPCR.PrcbData.CurrentThread
		movzx ecx, byte ptr [ecx]KTHREAD.HasTerminated // make sure that PsCreateSystemThreadEx actually succeeded
		test ecx, ecx
		jz thread_start
		push 0xC0000017L // STATUS_NO_MEMORY
		call PsTerminateSystemThread
	thread_start:
		pop ecx
		call ecx // SystemRoutine should never return
		push NORETURN_FUNCTION_RETURNED
		call KeBugCheckLogEip // won't return
	}
	// clang-format on
}

// Source: Cxbx-Reloaded
VOID KiInitializeContextThread(PKTHREAD Thread, ULONG TlsDataSize, PKSYSTEM_ROUTINE SystemRoutine, PKSTART_ROUTINE StartRoutine, PVOID StartContext)
{
	ULONG_PTR StackAddress = reinterpret_cast<ULONG_PTR>(Thread->StackBase);

	StackAddress -= sizeof(FX_SAVE_AREA);
	PFX_SAVE_AREA FxSaveArea = reinterpret_cast<PFX_SAVE_AREA>(StackAddress);
	memset(FxSaveArea, 0, sizeof(FX_SAVE_AREA));

	FxSaveArea->FloatSave.ControlWord = 0x27F;
	FxSaveArea->FloatSave.MXCsr = 0x1F80;
	FxSaveArea->FloatSave.Cr0NpxState = NPX_STATE_NOT_LOADED;
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

// Source: Cxbx-Reloaded
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

template<bool AddToTail>
static VOID FASTCALL KiAddThreadToReadyList(PKTHREAD Thread)
{
	assert(KeGetCurrentIrql() == DISPATCH_LEVEL);

	Thread->State = Ready;
	if constexpr (AddToTail) {
		InsertTailList(&KiReadyThreadLists[Thread->Priority], &Thread->WaitListEntry);
	}
	else {
		InsertHeadList(&KiReadyThreadLists[Thread->Priority], &Thread->WaitListEntry);
	}
	KiReadyThreadMask |= (1 << Thread->Priority);
}

VOID FASTCALL KeAddThreadToTailOfReadyList(PKTHREAD Thread)
{
	KiAddThreadToReadyList<true>(Thread);
}

VOID KiScheduleThread(PKTHREAD Thread)
{
	if (KiPcr.PrcbData.NextThread) {
		// If another thread was selected and this one has higher priority, preempt it
		if (Thread->Priority > KiPcr.PrcbData.NextThread->Priority) {
			KiAddThreadToReadyList<false>(KiPcr.PrcbData.NextThread);
			KiPcr.PrcbData.NextThread = Thread;
			Thread->State = Standby;
			return;
		}
	}
	else {
		// If the current thread has lower priority than this one, preempt it
		if (Thread->Priority > KiPcr.PrcbData.CurrentThread->Priority) {
			KiPcr.PrcbData.NextThread = Thread;
			Thread->State = Standby;
			return;
		}
	}

	// Add thread to the tail of the ready list (round robin scheduling)
	KiAddThreadToReadyList<true>(Thread);
}

VOID KeScheduleThread(PKTHREAD Thread)
{
	KIRQL OldIrql = KeRaiseIrqlToDpcLevel();
	KiScheduleThread(Thread);
	KiUnlockDispatcherDatabase(OldIrql);
}

DWORD __declspec(naked) KiSwapThreadContext()
{
	// This function uses a custom calling convention, so never call it from C++ code
	// esi -> KPCR.PrcbData.CurrentThread before the switch
	// edi -> KPCR.PrcbData.NextThread before the switch
	// ebx -> IRQL of the previous thread. If zero, check for kernel APCs, otherwise don't

	// When the final ret instruction is executed, it will either point to the caller of this function of the next thread when it was last switched out,
	// or it will be the address of KiThreadStartup when the thread is executed for the first time ever

	// clang-format off
	__asm {
		mov byte ptr [edi]KTHREAD.State, Running
		// Construct a KSWITCHFRAME on the stack (note that RetAddr was pushed by the call instruction to this function)
		or bl, bl // set zf in the saved eflags so that it will reflect the IRQL of the previous thread
		pushfd // save eflags
		push dword ptr [KiPcr]KPCR.NtTib.ExceptionList // save per-thread exception list
		cli
		mov dword ptr [esi]KTHREAD.KernelStack, esp // save esp
		// The floating state is saved in a lazy manner, because it's expensive to save it at every context switch. Instead of actually saving it here, we
		// only set flags in cr0 so that an attempt to use any fpu/mmx/sse instructions will cause a "no math coprocessor" exception, which is then handled
		// by the kernel in KiTrapNM and it's there where the floating context is restored
		mov ecx, dword ptr [edi]KTHREAD.StackBase
		sub ecx, SIZE FX_SAVE_AREA
		mov dword ptr [KiPcr]KPCR.NtTib.StackBase, ecx
		mov eax, dword ptr [edi]KTHREAD.StackLimit
		mov dword ptr [KiPcr]KPCR.NtTib.StackLimit, eax
		mov eax, cr0
		and eax, ~(CR0_MP | CR0_EM | CR0_TS)
		or eax, dword ptr [ecx]FLOATING_SAVE_AREA.Cr0NpxState
		mov cr0, eax
		mov esp, dword ptr [edi]KTHREAD.KernelStack // switch to the stack of the new thread -> it points to a KSWITCHFRAME
		sti
		inc dword ptr [edi]KTHREAD.ContextSwitches // per-thread number of context switches
		inc [KiPcr]KPCR.PrcbData.KeContextSwitches // total number of context switches
		pop dword ptr [KiPcr]KPCR.NtTib.ExceptionList // restore exception list
		cmp byte ptr [edi]KTHREAD.ApcState.KernelApcPending, 0
		jnz kernel_apc
		popfd // restore eflags
		xor eax, eax
		ret // load eip for the new thread
	kernel_apc:
		popfd // restore eflags and read saved previous IRQL from zf
		jz deliver_apc // if zero, then previous IRQL was PASSIVE so deliver an APC
		mov cl, APC_LEVEL
		call HalRequestSoftwareInterrupt
		xor eax, eax
		ret // load eip for the new thread
	deliver_apc:
		mov eax, 1
		ret // load eip for the new thread
	}
	// clang-format on
}

static PKTHREAD XBOXAPI KiFindAndRemoveHighestPriorityThread(KPRIORITY LowPriority)
{
	assert(KeGetCurrentIrql() == DISPATCH_LEVEL);

	KPRIORITY HighestPriority;
	if (bit_scan_reverse((ULONG *)&HighestPriority, KiReadyThreadMask) == 0) {
		return nullptr;
	}

	if (HighestPriority < LowPriority) {
		return nullptr;
	}

	PLIST_ENTRY ListHead = &KiReadyThreadLists[HighestPriority];
	assert(ListHead->Flink);

	PKTHREAD Thread = CONTAINING_RECORD(ListHead->Flink, KTHREAD, WaitListEntry);
	RemoveEntryList(&Thread->WaitListEntry);
	if (IsListEmpty(&KiReadyThreadLists[HighestPriority])) {
		KiReadyThreadMask &= ~(1 << HighestPriority);
	}

	return Thread;
}

static VOID KiSetPriorityThread(PKTHREAD Thread, KPRIORITY NewPriority)
{
	assert(KeGetCurrentIrql() == DISPATCH_LEVEL);

	KPRIORITY OldPriority = Thread->Priority;

	if (OldPriority != NewPriority) {
		Thread->Priority = NewPriority;

		switch (Thread->State)
		{
		case Ready:
			RemoveEntryList(&Thread->WaitListEntry);
			if (IsListEmpty(&KiReadyThreadLists[OldPriority])) {
				KiReadyThreadMask &= ~(1 << OldPriority);
			}

			if (NewPriority < OldPriority) {
				// Add the thread to the tail of the ready list (round robin scheduling)
				KiAddThreadToReadyList<true>(Thread);
			}
			else {
				// If NewPriority is higher than OldPriority, check to see if the thread can preempt other threads
				KiScheduleThread(Thread);
			}
			break;

		case Standby:
			if (NewPriority < OldPriority) {
				// Attempt to find another thread to run instead of the one we were about to execute
				PKTHREAD NewThread = KiFindAndRemoveHighestPriorityThread(NewPriority);
				if (NewThread) {
					NewThread->State = Standby;
					KiPcr.PrcbData.NextThread = NewThread;
					KiScheduleThread(Thread);
				}
			}
			break;

		case Running:
			if (KiPcr.PrcbData.NextThread == nullptr) {
				if (NewPriority < OldPriority) {
					// Attempt to preempt the currently running thread (NOTE: the dispatch request should be done by the caller)
					PKTHREAD NewThread = KiFindAndRemoveHighestPriorityThread(NewPriority);
					if (NewThread) {
						NewThread->State = Standby;
						KiPcr.PrcbData.NextThread = NewThread;
					}
				}
			}
			break;

		case Initialized:
			// The thread is being created by PsCreateSystemThreadEx. The function will schedule it at the end of the initialization, so nothing to do here
			break;

		case Terminated:
			// The thread is being killed by PsTerminateSystemThread, so there's no point to re-schedule it here
			break;

		case Waiting:
			// The thread is blocked in a wait (for example, in KeWaitForSingleObject). When the wait ends, a re-schedule will happen, so nothing to do here
			break;
		}
	}
}

PKTHREAD XBOXAPI KiQuantumEnd()
{
	assert(KeGetCurrentIrql() == DISPATCH_LEVEL);

	PKTHREAD Thread = KeGetCurrentThread();

	if (Thread->Quantum <= 0) {
		Thread->Quantum = Thread->ApcState.Process->ThreadQuantum;
		KPRIORITY NewPriority, Priority = Thread->Priority;

		if (Priority < LOW_REALTIME_PRIORITY) {
			NewPriority = Priority - 1;
			if (NewPriority < Thread->BasePriority) {
				NewPriority = Priority;
			}
		}
		else {
			NewPriority = Priority;
		}

		if (Priority != NewPriority) {
			KiSetPriorityThread(Thread, NewPriority);
		}
		else {
			if (KiPcr.PrcbData.NextThread == nullptr) {
				// Attempt to find another thread to run since the current one has just ended its quantum
				PKTHREAD NewThread = KiFindAndRemoveHighestPriorityThread(Priority);
				if (NewThread) {
					NewThread->State = Standby;
					KiPcr.PrcbData.NextThread = NewThread;
				}
			}
		}
	}

	return KiPcr.PrcbData.NextThread;
}

VOID KiAdjustQuantumThread()
{
	assert(KeGetCurrentIrql() == DISPATCH_LEVEL);

	PKTHREAD Thread = KeGetCurrentThread();

	if ((Thread->Priority < LOW_REALTIME_PRIORITY) && (Thread->BasePriority < TIME_CRITICAL_BASE_PRIORITY)) {
		Thread->Quantum -= WAIT_QUANTUM_DECREMENT;
		KiQuantumEnd();
	}
}

NTSTATUS __declspec(naked) XBOXAPI KiSwapThread()
{
	// On entry, IRQL must be at DISPATCH_LEVEL

	// clang-format off
	__asm {
		mov eax, dword ptr [KiPcr]KPCR.PrcbData.NextThread
		test eax, eax
		jnz thread_switch // check to see if a new thread was selected by the scheduler
		push LOW_PRIORITY
		call KiFindAndRemoveHighestPriorityThread
		jnz thread_switch
		lea eax, KiIdleThread
	thread_switch:
		// Save non-volatile registers
		push esi
		push edi
		push ebx
		push ebp
		mov edi, eax
		mov esi, dword ptr [KiPcr]KPCR.PrcbData.CurrentThread
		mov dword ptr [KiPcr]KPCR.PrcbData.CurrentThread, edi
		mov dword ptr [KiPcr]KPCR.PrcbData.NextThread, 0
		movzx ebx, byte ptr [esi]KTHREAD.WaitIrql
		call KiSwapThreadContext // when this returns, it means this thread was switched back again
		test eax, eax
		mov cl, byte ptr [edi]KTHREAD.WaitIrql
		mov ebx, dword ptr [edi]KTHREAD.WaitStatus
		jnz deliver_apc
	restore_regs:
		call KfLowerIrql
		mov eax, ebx
		// Restore non-volatile registers
		pop ebp
		pop ebx
		pop edi
		pop esi
		jmp end_func
	deliver_apc:
		mov cl, APC_LEVEL
		call KfLowerIrql
		call KiExecuteApcQueue
		xor ecx, ecx // if KiSwapThreadContext signals an APC, then WaitIrql of the previous thread must have been zero
		jmp restore_regs
	end_func:
		ret
	}
	// clang-format on
}

EXPORTNUM(104) PKTHREAD XBOXAPI KeGetCurrentThread()
{
	// clang-format off
	__asm mov eax, dword ptr [KiPcr]KPCR.PrcbData.CurrentThread
	// clang-format on
}

EXPORTNUM(140) ULONG XBOXAPI KeResumeThread
(
	PKTHREAD Thread
)
{
	RIP_UNIMPLEMENTED();

	return 1;
}

EXPORTNUM(148) KPRIORITY XBOXAPI KeSetPriorityThread
(
	PKTHREAD Thread,
	LONG Priority
)
{
	KIRQL OldIrql = KeRaiseIrqlToDpcLevel();
	KPRIORITY OldPriority = Thread->Priority;
	Thread->Quantum = Thread->ApcState.Process->ThreadQuantum;
	KiSetPriorityThread(Thread, Priority);
	KiUnlockDispatcherDatabase(OldIrql);

	return OldPriority;
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
