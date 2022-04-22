/*
 * ergo720                Copyright (c) 2022
 */

#pragma once

#include "..\types.h"


using KIRQL = UCHAR;
using KPROCESSOR_MODE = CCHAR;

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
using PKTREAD = KTHREAD *;

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

EXPORTNUM(95) VOID XBOXAPI KeBugCheck
(
    ULONG BugCheckCode
);

EXPORTNUM(96) VOID XBOXAPI KeBugCheckEx
(
    ULONG BugCheckCode,
    ULONG_PTR BugCheckParameter1,
    ULONG_PTR BugCheckParameter2,
    ULONG_PTR BugCheckParameter3,
    ULONG_PTR BugCheckParameter4
);

EXPORTNUM(107) VOID XBOXAPI KeInitializeDpc
(
    PKDPC Dpc,
    PKDEFERRED_ROUTINE DeferredRoutine,
    PVOID DeferredContext
);

EXPORTNUM(156) extern volatile DWORD KeTickCount;

#ifdef __cplusplus
}
#endif
