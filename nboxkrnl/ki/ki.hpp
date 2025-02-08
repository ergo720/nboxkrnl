/*
 * ergo720                Copyright (c) 2022
 */

#pragma once

#include "..\types.hpp"
#include "ke.hpp"
#include "..\kernel.hpp"

#define EXCEPTION_CHAIN_END2 0xFFFFFFFF
#define EXCEPTION_CHAIN_END reinterpret_cast<EXCEPTION_REGISTRATION_RECORD *>(EXCEPTION_CHAIN_END2)

// cr0 flags
#define CR0_TS (1 << 3) // task switched
#define CR0_EM (1 << 2) // emulation
#define CR0_MP (1 << 1) // monitor coprocessor

#define SIZE_OF_FPU_REGISTERS        128
#define NPX_STATE_NOT_LOADED (CR0_TS | CR0_MP) // x87 fpu, XMM, and MXCSR registers not loaded on fpu
#define NPX_STATE_LOADED 0                     // x87 fpu, XMM, and MXCSR registers loaded on fpu

#define TIMER_TABLE_SIZE 32
#define CLOCK_TIME_INCREMENT 10000 // one clock interrupt every ms -> 1ms == 10000 units of 100ns

#define NUM_OF_THREAD_PRIORITIES 32

using KGDT = uint64_t;
using KIDT = uint64_t;
using KTSS = uint32_t[26];

#pragma pack(1)
struct FLOATING_SAVE_AREA {
	USHORT  ControlWord;
	USHORT  StatusWord;
	USHORT  TagWord;
	USHORT  ErrorOpcode;
	ULONG   ErrorOffset;
	ULONG   ErrorSelector;
	ULONG   DataOffset;
	ULONG   DataSelector;
	ULONG   MXCsr;
	ULONG   Reserved1;
	UCHAR   RegisterArea[SIZE_OF_FPU_REGISTERS];
	UCHAR   XmmRegisterArea[SIZE_OF_FPU_REGISTERS];
	UCHAR   Reserved2[224];
	ULONG   Cr0NpxState;
};
#pragma pack()

struct  FX_SAVE_AREA {
	FLOATING_SAVE_AREA FloatSave;
	ULONG Align16Byte[3];
};
using PFX_SAVE_AREA = FX_SAVE_AREA *;


#define CONTEXT_i386                0x00010000
#define CONTEXT_CONTROL             (CONTEXT_i386 | 0x00000001L)
#define CONTEXT_INTEGER             (CONTEXT_i386 | 0x00000002L)
#define CONTEXT_SEGMENTS            (CONTEXT_i386 | 0x00000004L)
#define CONTEXT_FLOATING_POINT      (CONTEXT_i386 | 0x00000008L)
#define CONTEXT_EXTENDED_REGISTERS  (CONTEXT_i386 | 0x00000020L)

struct CONTEXT {
	DWORD ContextFlags;
	FLOATING_SAVE_AREA FloatSave;
	DWORD Edi;
	DWORD Esi;
	DWORD Ebx;
	DWORD Edx;
	DWORD Ecx;
	DWORD Eax;
	DWORD Ebp;
	DWORD Eip;
	DWORD SegCs;
	DWORD EFlags;
	DWORD Esp;
	DWORD SegSs;
};
using PCONTEXT = CONTEXT *;

struct KPRCB {
	struct KTHREAD *CurrentThread;
	struct KTHREAD *NextThread;
	struct KTHREAD *IdleThread;
	struct KTHREAD *NpxThread;
	ULONG InterruptCount;
	ULONG DpcTime;
	ULONG InterruptTime;
	ULONG DebugDpcTime;
	ULONG KeContextSwitches;
	ULONG DpcInterruptRequested;
	LIST_ENTRY DpcListHead;
	ULONG DpcRoutineActive;
	PVOID DpcStack;
	ULONG QuantumEnd;
	// NOTE: if this is used with a fxsave instruction to save the float state, then this buffer must be 16-bytes aligned.
	// At the moment, we only use the Npx area in the thread's stack
	FX_SAVE_AREA NpxSaveArea;
	VOID *DmEnetFunc;
	VOID *DebugMonitorData;
};
using PKPRCB = KPRCB *;

struct NT_TIB {
	struct EXCEPTION_REGISTRATION_RECORD *ExceptionList;
	PVOID StackBase;
	PVOID StackLimit;
	PVOID SubSystemTib;
	union {
		PVOID FiberData;
		DWORD Version;
	};
	PVOID ArbitraryUserPointer;
	NT_TIB *Self;
};
using PNT_TIB = NT_TIB *;

struct KPCR {
	NT_TIB NtTib;
	KPCR *SelfPcr;
	KPRCB *Prcb;
	KIRQL Irql;
	KPRCB PrcbData;
};
using PKPCR = KPCR *;


inline KTHREAD KiIdleThread;
inline uint8_t alignas(16) KiIdleThreadStack[KERNEL_STACK_SIZE]; // must be 16 byte aligned because it's used by fxsave and fxrstor

// List of all thread that can be scheduled, one for each priority level
inline LIST_ENTRY KiReadyThreadLists[NUM_OF_THREAD_PRIORITIES];

// Bitmask of KiReadyThreadLists -> bit position is one if there is at least one ready thread at that priority
inline DWORD KiReadyThreadMask = 0;

inline LIST_ENTRY KiWaitInListHead;

inline LIST_ENTRY KiTimerTableListHead[TIMER_TABLE_SIZE];

inline KDPC KiTimerExpireDpc;

extern KPCR KiPcr;
extern KTSS KiTss;
extern const KGDT KiGdt[5];
extern KIDT KiIdt[64];
inline constexpr uint16_t KiGdtLimit = sizeof(KiGdt) - 1;
inline constexpr uint16_t KiIdtLimit = sizeof(KiIdt) - 1;
inline constexpr size_t KiFxAreaSize = sizeof(FX_SAVE_AREA);
extern KPROCESS KiUniqueProcess;
extern KPROCESS KiIdleProcess;


VOID InitializeCrt();
[[noreturn]] VOID KiInitializeKernel();
VOID KiInitSystem();
[[noreturn]] VOID KiIdleLoopThread();
DWORD KiSwapThreadContext();
VOID XBOXAPI KiExecuteApcQueue();
VOID XBOXAPI KiExecuteDpcQueue();
PKTHREAD XBOXAPI KiQuantumEnd();
VOID KiAdjustQuantumThread();
NTSTATUS XBOXAPI KiSwapThread();

VOID KiInitializeProcess(PKPROCESS Process, KPRIORITY BasePriority, LONG ThreadQuantum);

VOID XBOXAPI KiTimerExpiration(PKDPC Dpc, PVOID DeferredContext, PVOID SystemArgument1, PVOID SystemArgument2);
BOOLEAN KiInsertTimer(PKTIMER Timer, LARGE_INTEGER DueTime);
BOOLEAN KiReinsertTimer(PKTIMER Timer, ULARGE_INTEGER DueTime);
VOID KiRemoveTimer(PKTIMER Timer);
ULONG KiComputeTimerTableIndex(ULONGLONG DueTime);
PLARGE_INTEGER KiRecalculateTimerDueTime(PLARGE_INTEGER OriginalTime, PLARGE_INTEGER DueTime, PLARGE_INTEGER NewTime);
VOID KiTimerListExpire(PLIST_ENTRY ExpiredListHead, KIRQL OldIrql);
VOID KiTimerListExpire(PLIST_ENTRY ExpiredListHead, KIRQL OldIrql);

VOID KiWaitTest(PVOID Object, KPRIORITY Increment);
VOID KiUnwaitThread(PKTHREAD Thread, LONG_PTR WaitStatus, KPRIORITY Increment);
