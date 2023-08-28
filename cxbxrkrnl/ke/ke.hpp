/*
 * ergo720                Copyright (c) 2022
 */

#pragma once

#include "..\types.hpp"

#define XBOX_KEY_LENGTH 16

#define THREAD_QUANTUM 60
#define NORMAL_BASE_PRIORITY 8
#define HIGH_PRIORITY 31

#define HIGH_LEVEL 31
#define DISPATCH_LEVEL 2
#define APC_LEVEL 1
#define PASSIVE_LEVEL 0

#define IDT_INT_VECTOR_BASE 0x30

#define KeGetPcr() (&KiPcr)


using KIRQL = UCHAR;
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

EXPORTNUM(107) DLLEXPORT VOID XBOXAPI KeInitializeDpc
(
    PKDPC Dpc,
    PKDEFERRED_ROUTINE DeferredRoutine,
    PVOID DeferredContext
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

EXPORTNUM(129) DLLEXPORT KIRQL XBOXAPI KeRaiseIrqlToDpcLevel();

EXPORTNUM(156) DLLEXPORT extern volatile DWORD KeTickCount;

EXPORTNUM(161) DLLEXPORT VOID FASTCALL KfLowerIrql
(
    KIRQL NewIrql
);

EXPORTNUM(321) DLLEXPORT extern XBOX_KEY_DATA XboxEEPROMKey;

#ifdef __cplusplus
}
#endif

extern XBOX_KEY_DATA XboxCERTKey;

VOID XBOXAPI KiSuspendNop(PKAPC Apc, PKNORMAL_ROUTINE *NormalRoutine, PVOID *NormalContext, PVOID *SystemArgument1, PVOID *SystemArgument2);
VOID XBOXAPI KiSuspendThread(PVOID NormalContext, PVOID SystemArgument1, PVOID SystemArgument);
VOID KeInitializeThread(PKTHREAD Thread, PVOID KernelStack, ULONG KernelStackSize, ULONG TlsDataSize, PKSYSTEM_ROUTINE SystemRoutine, PKSTART_ROUTINE StartRoutine,
    PVOID StartContext, PKPROCESS Process);

VOID XBOXAPI KeInitializeTimer(PKTIMER Timer);
