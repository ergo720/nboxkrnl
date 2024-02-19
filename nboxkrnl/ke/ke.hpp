/*
 * ergo720                Copyright (c) 2022
 */

#pragma once

#include "..\types.hpp"

#define XBOX_KEY_LENGTH 16

#define THREAD_QUANTUM 60 // ms that a thread is allowed to run before being preempted, in multiples of CLOCK_QUANTUM_DECREMENT
#define CLOCK_QUANTUM_DECREMENT 3 // subtracts 1 ms after every clock interrupt, in multiples of CLOCK_QUANTUM_DECREMENT
#define WAIT_QUANTUM_DECREMENT 9 // subtracts 3 ms after a wait completes, in multiples of CLOCK_QUANTUM_DECREMENT
#define NORMAL_BASE_PRIORITY 8
#define TIME_CRITICAL_BASE_PRIORITY 14
#define LOW_PRIORITY 0
#define LOW_REALTIME_PRIORITY 16
#define HIGH_PRIORITY 31

#define PRIORITY_BOOST_EVENT 1
#define PRIORITY_BOOST_MUTANT 1
#define PRIORITY_BOOST_SEMAPHORE PRIORITY_BOOST_EVENT
#define PRIORITY_BOOST_IO 0
#define PRIORITY_BOOST_TIMER 0

#define HIGH_LEVEL 31
#define CLOCK_LEVEL 28
#define SYNC_LEVEL CLOCK_LEVEL
#define DISPATCH_LEVEL 2
#define APC_LEVEL 1
#define PASSIVE_LEVEL 0

#define IRQL_OFFSET_FOR_IRQ 4
#define DISPATCH_LENGTH 22

#define IDT_SERVICE_VECTOR_BASE 0x20
#define IDT_INT_VECTOR_BASE 0x30
#define MAX_BUS_INTERRUPT_LEVEL 26

#define SYNCHRONIZATION_OBJECT_TYPE_MASK 7

#define MUTANT_LIMIT 0x80000000

#define MAX_TIMER_DPCS 16

// These macros (or equivalent assembly code) should be used to access the members of KiPcr when the irql is below dispatch level, to make sure that
// the accesses are atomic and thus thread-safe
#define KeGetStackBase(var) \
	__asm { \
		__asm mov eax, [KiPcr].NtTib.StackBase \
		__asm mov var, eax \
	}

#define KeGetStackLimit(var) \
	__asm { \
		__asm mov eax, [KiPcr].NtTib.StackLimit \
		__asm mov var, eax \
	}

#define KeGetExceptionHead(var) \
	__asm { \
		__asm mov eax, [KiPcr].NtTib.ExceptionList \
		__asm mov dword ptr var, eax \
	}

#define KeSetExceptionHead(var) \
	__asm { \
		__asm mov eax, dword ptr var \
		__asm mov [KiPcr].NtTib.ExceptionList, eax \
	}

#define KeResetExceptionHead(var) \
	__asm { \
		__asm mov eax, dword ptr var \
		__asm mov [KiPcr].NtTib.ExceptionList, eax \
	}

#define KeGetDpcStack(var) \
	__asm { \
		__asm mov eax, [KiPcr].PrcbData.DpcStack \
		__asm mov var, eax \
	}

#define KeGetDpcActive(var) \
	__asm { \
		__asm mov eax, [KiPcr].PrcbData.DpcRoutineActive \
		__asm mov var, eax \
	}

#define INITIALIZE_GLOBAL_KEVENT(Event, Type, State) \
    KEVENT Event = {                                 \
        Type,                                        \
        FALSE,                                       \
        sizeof(KEVENT) / sizeof(LONG),               \
        FALSE,                                       \
        State,                                       \
        &Event.Header.WaitListHead,                  \
        &Event.Header.WaitListHead                   \
    }


using KIRQL = UCHAR;
using PKIRQL = KIRQL *;
using KPROCESSOR_MODE = CCHAR;
using XBOX_KEY_DATA = uint8_t[XBOX_KEY_LENGTH];

enum MODE {
	KernelMode,
	UserMode,
	MaximumMode
};

enum KOBJECTS {
	EventNotificationObject = 0,
	EventSynchronizationObject = 1,
	MutantObject = 2,
	ProcessObject = 3,
	QueueObject = 4,
	SemaphoreObject = 5,
	ThreadObject = 6,
	Spare1Object = 7,
	TimerNotificationObject = 8,
	TimerSynchronizationObject = 9,
	Spare2Object = 10,
	Spare3Object = 11,
	Spare4Object = 12,
	Spare5Object = 13,
	Spare6Object = 14,
	Spare7Object = 15,
	Spare8Object = 16,
	Spare9Object = 17,
	ApcObject,
	DpcObject,
	DeviceQueueObject,
	EventPairObject,
	InterruptObject,
	ProfileObject
};

enum TIMER_TYPE {
	NotificationTimer,
	SynchronizationTimer
};

enum WAIT_TYPE {
	WaitAll,
	WaitAny
};

enum KTHREAD_STATE {
	Initialized,
	Ready,
	Running,
	Standby,
	Terminated,
	Waiting,
	Transition
};

enum KINTERRUPT_MODE {
	LevelSensitive,
	Edge
};

enum KWAIT_REASON {
	Executive = 0,
	FreePage = 1,
	PageIn = 2,
	PoolAllocation = 3,
	DelayExecution = 4,
	Suspended = 5,
	UserRequest = 6,
	WrExecutive = 7,
	WrFreePage = 8,
	WrPageIn = 9,
	WrPoolAllocation = 10,
	WrDelayExecution = 11,
	WrSuspended = 12,
	WrUserRequest = 13,
	WrEventPair = 14,
	WrQueue = 15,
	WrLpcReceive = 16,
	WrLpcReply = 17,
	WrVirtualMemory = 18,
	WrPageOut = 19,
	WrRendezvous = 20,
	WrFsCacheIn = 21,
	WrFsCacheOut = 22,
	Spare4 = 23,
	Spare5 = 24,
	Spare6 = 25,
	WrKernel = 26,
	MaximumWaitReason = 27
};

enum EVENT_TYPE {
	NotificationEvent = 0,
	SynchronizationEvent
};

struct KAPC_STATE {
	LIST_ENTRY ApcListHead[MaximumMode];
	struct KPROCESS *Process;
	BOOLEAN KernelApcInProgress;
	BOOLEAN KernelApcPending;
	BOOLEAN UserApcPending;
	BOOLEAN ApcQueueable;
};
using PKAPC_STATE = KAPC_STATE *;

struct KWAIT_BLOCK {
	LIST_ENTRY WaitListEntry;
	struct KTHREAD *Thread;
	PVOID Object;
	struct KWAIT_BLOCK *NextWaitBlock;
	USHORT WaitKey;
	USHORT WaitType;
};
using PKWAIT_BLOCK = KWAIT_BLOCK *;

struct KQUEUE {
	DISPATCHER_HEADER Header;
	LIST_ENTRY EntryListHead;
	ULONG CurrentCount;
	ULONG MaximumCount;
	LIST_ENTRY ThreadListHead;
};
using PKQUEUE = KQUEUE *;

struct KTIMER {
	DISPATCHER_HEADER Header;
	ULARGE_INTEGER DueTime;
	LIST_ENTRY TimerListEntry;
	struct KDPC *Dpc;
	LONG Period;
};
using PKTIMER = KTIMER *;

using PKNORMAL_ROUTINE = VOID(XBOXAPI *)(
	PVOID NormalContext,
	PVOID SystemArgument1,
	PVOID SystemArgument2
	);

using PKKERNEL_ROUTINE = VOID(XBOXAPI *)(
	struct KAPC *Apc,
	PKNORMAL_ROUTINE *NormalRoutine,
	PVOID *NormalContext,
	PVOID *SystemArgument1,
	PVOID *SystemArgument2
	);

using PKRUNDOWN_ROUTINE = VOID(XBOXAPI *)(
	struct KAPC *Apc
	);

using PKSTART_ROUTINE = VOID(XBOXAPI *)(
	VOID *StartContext
	);

using PKSYSTEM_ROUTINE = VOID(XBOXAPI *)(
	PKSTART_ROUTINE StartRoutine,
	VOID *StartContext
	);

struct KAPC {
	CSHORT Type;
	CSHORT Size;
	ULONG Reserved;
	struct KTHREAD *Thread;
	LIST_ENTRY ApcListEntry;
	PKKERNEL_ROUTINE KernelRoutine;
	PKRUNDOWN_ROUTINE RundownRoutine;
	PKNORMAL_ROUTINE NormalRoutine;
	PVOID NormalContext;
	PVOID SystemArgument1;
	PVOID SystemArgument2;
	CCHAR ApcStateIndex;
	KPROCESSOR_MODE ApcMode;
	BOOLEAN Inserted;
};
using PKAPC = KAPC *;

struct KSEMAPHORE {
	DISPATCHER_HEADER Header;
	LONG Limit;
};
using PKSEMAPHORE = KSEMAPHORE *;

struct KEVENT {
	DISPATCHER_HEADER Header;
};
using PKEVENT = KEVENT *;

struct KDEVICE_QUEUE {
	CSHORT Type;
	UCHAR Size;
	BOOLEAN Busy;
	LIST_ENTRY DeviceListHead;
};
using PKDEVICE_QUEUE = KDEVICE_QUEUE *;

struct KMUTANT {
	DISPATCHER_HEADER Header;
	LIST_ENTRY MutantListEntry;
	struct KTHREAD *OwnerThread;
	BOOLEAN Abandoned;
};
using PKMUTANT = KMUTANT *;

struct KSTART_FRAME {
	PKSYSTEM_ROUTINE SystemRoutine;
	PKSTART_ROUTINE StartRoutine;
	PVOID StartContext;
};
using PKSTART_FRAME = KSTART_FRAME *;

struct KSWITCHFRAME {
	PVOID ExceptionList;
	DWORD Eflags;
	PVOID RetAddr;
};
using PKSWITCHFRAME = KSWITCHFRAME *;

struct KTHREAD {
	DISPATCHER_HEADER Header;
	LIST_ENTRY MutantListHead;
	ULONG KernelTime;
	PVOID StackBase;
	PVOID StackLimit;
	PVOID KernelStack;
	PVOID TlsData;
	UCHAR State;
	BOOLEAN Alerted[MaximumMode];
	BOOLEAN Alertable;
	UCHAR NpxState;
	CHAR Saturation;
	SCHAR Priority;
	UCHAR Padding;
	KAPC_STATE ApcState;
	ULONG ContextSwitches;
	LONG_PTR WaitStatus;
	KIRQL WaitIrql;
	KPROCESSOR_MODE WaitMode;
	BOOLEAN WaitNext;
	UCHAR WaitReason;
	PKWAIT_BLOCK WaitBlockList;
	LIST_ENTRY WaitListEntry;
	ULONG WaitTime;
	ULONG KernelApcDisable;
	LONG Quantum;
	SCHAR BasePriority;
	UCHAR DecrementCount;
	SCHAR PriorityDecrement;
	BOOLEAN DisableBoost;
	UCHAR NpxIrql;
	CCHAR SuspendCount;
	BOOLEAN Preempted;
	BOOLEAN HasTerminated;
	PKQUEUE Queue;
	LIST_ENTRY QueueListEntry;
	KTIMER Timer;
	KWAIT_BLOCK TimerWaitBlock;
	KAPC SuspendApc;
	KSEMAPHORE SuspendSemaphore;
	LIST_ENTRY ThreadListEntry;
};
using PKTHREAD = KTHREAD *;

struct ETHREAD {
	KTHREAD Tcb;
	LARGE_INTEGER CreateTime;
	LARGE_INTEGER ExitTime;
	NTSTATUS ExitStatus;
	LIST_ENTRY ReaperLink;
	HANDLE UniqueThread;
	PVOID StartAddress;
	LIST_ENTRY IrpList;
};
using PETHREAD = ETHREAD *;

struct KPROCESS {
	LIST_ENTRY ReadyListHead;
	LIST_ENTRY ThreadListHead;
	ULONG StackCount;
	LONG ThreadQuantum;
	SCHAR BasePriority;
	BOOLEAN DisableBoost;
	BOOLEAN DisableQuantum;
};
using PKPROCESS = KPROCESS *;

using PKDEFERRED_ROUTINE = VOID(XBOXAPI *)(
	struct KDPC *Dpc,
	PVOID DeferredContext,
	PVOID SystemArgument1,
	PVOID SystemArgument2
	);

struct KDPC {
	CSHORT Type;
	BOOLEAN Inserted;
	UCHAR Padding;
	LIST_ENTRY DpcListEntry;
	PKDEFERRED_ROUTINE DeferredRoutine;
	PVOID DeferredContext;
	PVOID SystemArgument1;
	PVOID SystemArgument2;
	PULONG_PTR Lock;
};
using PKDPC = KDPC *;

struct KSYSTEM_TIME {
	ULONG LowTime;
	LONG HighTime;
	LONG High2Time;
};

struct DPC_QUEUE_ENTRY {
	PKDPC Dpc;
	PKDEFERRED_ROUTINE Routine;
	PVOID Context;
};
using PDPC_QUEUE_ENTRY = DPC_QUEUE_ENTRY *;

using PKSERVICE_ROUTINE = BOOLEAN(XBOXAPI *)(
	struct KINTERRUPT *Interrupt,
	PVOID ServiceContext
	);

struct KINTERRUPT {
	PKSERVICE_ROUTINE ServiceRoutine;
	PVOID ServiceContext;
	ULONG BusInterruptLevel;
	ULONG Irql;
	BOOLEAN Connected;
	BOOLEAN ShareVector;
	UCHAR Mode;
	ULONG ServiceCount;
	ULONG DispatchCode[DISPATCH_LENGTH];
};
using PKINTERRUPT = KINTERRUPT *;

#ifdef __cplusplus
extern "C" {
#endif

[[noreturn]] EXPORTNUM(95) DLLEXPORT VOID XBOXAPI KeBugCheck
(
	ULONG BugCheckCode
);

[[noreturn]] EXPORTNUM(96) DLLEXPORT VOID XBOXAPI KeBugCheckEx
(
	ULONG BugCheckCode,
	ULONG_PTR BugCheckParameter1,
	ULONG_PTR BugCheckParameter2,
	ULONG_PTR BugCheckParameter3,
	ULONG_PTR BugCheckParameter4
);

EXPORTNUM(98) DLLEXPORT BOOLEAN XBOXAPI KeConnectInterrupt
(
	PKINTERRUPT  InterruptObject
);

EXPORTNUM(99) DLLEXPORT NTSTATUS XBOXAPI KeDelayExecutionThread
(
	KPROCESSOR_MODE WaitMode,
	BOOLEAN Alertable,
	PLARGE_INTEGER Interval
);

EXPORTNUM(101) DLLEXPORT VOID XBOXAPI KeEnterCriticalRegion();

EXPORTNUM(103) DLLEXPORT KIRQL XBOXAPI KeGetCurrentIrql();

EXPORTNUM(104) DLLEXPORT PKTHREAD XBOXAPI KeGetCurrentThread();

EXPORTNUM(105) DLLEXPORT VOID XBOXAPI KeInitializeApc
(
	PKAPC Apc,
	PKTHREAD Thread,
	PKKERNEL_ROUTINE KernelRoutine,
	PKRUNDOWN_ROUTINE RundownRoutine,
	PKNORMAL_ROUTINE NormalRoutine,
	KPROCESSOR_MODE ApcMode,
	PVOID NormalContext
);

EXPORTNUM(106) DLLEXPORT VOID XBOXAPI KeInitializeDeviceQueue
(
	PKDEVICE_QUEUE DeviceQueue
);

EXPORTNUM(107) DLLEXPORT VOID XBOXAPI KeInitializeDpc
(
	PKDPC Dpc,
	PKDEFERRED_ROUTINE DeferredRoutine,
	PVOID DeferredContext
);

EXPORTNUM(108) DLLEXPORT VOID XBOXAPI KeInitializeEvent
(
	PKEVENT Event,
	EVENT_TYPE Type,
	BOOLEAN SignalState
);

EXPORTNUM(109) DLLEXPORT VOID XBOXAPI KeInitializeInterrupt
(
	PKINTERRUPT Interrupt,
	PKSERVICE_ROUTINE ServiceRoutine,
	PVOID ServiceContext,
	ULONG Vector,
	KIRQL Irql,
	KINTERRUPT_MODE InterruptMode,
	BOOLEAN ShareVector
);

EXPORTNUM(110) DLLEXPORT VOID XBOXAPI KeInitializeMutant
(
	PKMUTANT Mutant,
	BOOLEAN InitialOwner
);

EXPORTNUM(112) DLLEXPORT VOID XBOXAPI KeInitializeSemaphore
(
	PKSEMAPHORE Semaphore,
	LONG Count,
	LONG Limit
);

EXPORTNUM(113) DLLEXPORT VOID XBOXAPI KeInitializeTimerEx
(
	PKTIMER Timer,
	TIMER_TYPE Type
);

EXPORTNUM(118) DLLEXPORT BOOLEAN XBOXAPI KeInsertQueueApc
(
	PKAPC Apc,
	PVOID SystemArgument1,
	PVOID SystemArgument2,
	KPRIORITY Increment
);

EXPORTNUM(119) DLLEXPORT BOOLEAN XBOXAPI KeInsertQueueDpc
(
	PKDPC Dpc,
	PVOID SystemArgument1,
	PVOID SystemArgument2
);

EXPORTNUM(120) DLLEXPORT extern volatile KSYSTEM_TIME KeInterruptTime;

EXPORTNUM(122) DLLEXPORT VOID XBOXAPI KeLeaveCriticalRegion();

EXPORTNUM(125) DLLEXPORT ULONGLONG XBOXAPI KeQueryInterruptTime();

EXPORTNUM(126) DLLEXPORT ULONGLONG XBOXAPI KeQueryPerformanceCounter();

EXPORTNUM(127) DLLEXPORT ULONGLONG XBOXAPI KeQueryPerformanceFrequency();

EXPORTNUM(128) DLLEXPORT VOID XBOXAPI KeQuerySystemTime
(
	PLARGE_INTEGER CurrentTime
);

EXPORTNUM(129) DLLEXPORT KIRQL XBOXAPI KeRaiseIrqlToDpcLevel();

EXPORTNUM(130) DLLEXPORT KIRQL XBOXAPI KeRaiseIrqlToSynchLevel();

EXPORTNUM(131) DLLEXPORT LONG XBOXAPI KeReleaseMutant
(
	PKMUTANT Mutant,
	KPRIORITY Increment,
	BOOLEAN Abandoned,
	BOOLEAN Wait
);

EXPORTNUM(132) DLLEXPORT LONG XBOXAPI KeReleaseSemaphore
(
	PKSEMAPHORE Semaphore,
	KPRIORITY Increment,
	LONG Adjustment,
	BOOLEAN Wait
);

EXPORTNUM(140) DLLEXPORT ULONG XBOXAPI KeResumeThread
(
	PKTHREAD Thread
);

EXPORTNUM(145) DLLEXPORT LONG XBOXAPI KeSetEvent
(
	PKEVENT Event,
	KPRIORITY Increment,
	BOOLEAN	Wait
);

EXPORTNUM(148) DLLEXPORT KPRIORITY XBOXAPI KeSetPriorityThread
(
	PKTHREAD Thread,
	LONG Priority
);

EXPORTNUM(149) DLLEXPORT BOOLEAN XBOXAPI KeSetTimer
(
	PKTIMER Timer,
	LARGE_INTEGER DueTime,
	PKDPC Dpc
);

EXPORTNUM(150) DLLEXPORT BOOLEAN XBOXAPI KeSetTimerEx
(
	PKTIMER Timer,
	LARGE_INTEGER DueTime,
	LONG Period,
	PKDPC Dpc
);

EXPORTNUM(152) DLLEXPORT ULONG XBOXAPI KeSuspendThread
(
	PKTHREAD Thread
);

EXPORTNUM(154) DLLEXPORT extern volatile KSYSTEM_TIME KeSystemTime;

EXPORTNUM(155) DLLEXPORT BOOLEAN XBOXAPI KeTestAlertThread
(
	KPROCESSOR_MODE AlertMode
);

EXPORTNUM(156) DLLEXPORT extern volatile DWORD KeTickCount;

EXPORTNUM(157) DLLEXPORT extern ULONG KeTimeIncrement;

EXPORTNUM(159) DLLEXPORT NTSTATUS XBOXAPI KeWaitForSingleObject
(
	PVOID Object,
	KWAIT_REASON WaitReason,
	KPROCESSOR_MODE WaitMode,
	BOOLEAN Alertable,
	PLARGE_INTEGER Timeout
);

EXPORTNUM(160) DLLEXPORT KIRQL FASTCALL KfRaiseIrql
(
	KIRQL NewIrql
);

EXPORTNUM(161) DLLEXPORT VOID FASTCALL KfLowerIrql
(
	KIRQL NewIrql
);

EXPORTNUM(163) DLLEXPORT VOID FASTCALL KiUnlockDispatcherDatabase
(
	KIRQL NewIrql
);

EXPORTNUM(321) DLLEXPORT extern XBOX_KEY_DATA XboxEEPROMKey;

#ifdef __cplusplus
}
#endif

extern XBOX_KEY_DATA XboxCERTKey;
extern ULONG KernelThunkTable[379];

VOID XBOXAPI KiSuspendNop(PKAPC Apc, PKNORMAL_ROUTINE *NormalRoutine, PVOID *NormalContext, PVOID *SystemArgument1, PVOID *SystemArgument2);
VOID XBOXAPI KiSuspendThread(PVOID NormalContext, PVOID SystemArgument1, PVOID SystemArgument);
VOID KeInitializeThread(PKTHREAD Thread, PVOID KernelStack, ULONG KernelStackSize, ULONG TlsDataSize, PKSYSTEM_ROUTINE SystemRoutine, PKSTART_ROUTINE StartRoutine,
	PVOID StartContext, PKPROCESS Process);

VOID XBOXAPI KeInitializeTimer(PKTIMER Timer);
VOID FASTCALL KiCheckExpiredTimers(DWORD OldKeTickCount);
VOID KeScheduleThread(PKTHREAD Thread);
VOID KiScheduleThread(PKTHREAD Thread);
VOID FASTCALL KeAddThreadToTailOfReadyList(PKTHREAD Thread);

[[noreturn]] VOID CDECL KeBugCheckLogEip(ULONG BugCheckCode);
