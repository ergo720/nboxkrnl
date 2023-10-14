/*
 * ergo720                Copyright (c) 2022
 */

#pragma once

#include "..\types.hpp"
#include "..\ke\ke.hpp"
#include "..\kernel.hpp"

#define EXCEPTION_CHAIN_END reinterpret_cast<EXCEPTION_REGISTRATION_RECORD *>(0xFFFFFFFF)

// cr0 flags
#define CR0_TS (1 << 3) // task switched
#define CR0_EM (1 << 2) // emulation
#define CR0_MP (1 << 1) // monitor coprocessor

#define SIZE_OF_FPU_REGISTERS        128
#define NPX_STATE_NOT_LOADED (CR0_TS | CR0_MP) // x87 fpu, XMM, and MXCSR registers not saved
#define NPX_STATE_LOADED 0                     // x87 fpu, XMM, and MXCSR registers saved

#define TIMER_TABLE_SIZE 32

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

struct alignas(16) FX_SAVE_AREA {
	FLOATING_SAVE_AREA FloatSave;
	ULONG Align16Byte[3];
};
using PFX_SAVE_AREA = FX_SAVE_AREA *;

// Sanity check: this struct is used to save the npx state with the fxsave instruction, and the Intel docs require that the buffer
// must be 16-bytes aligned
static_assert(alignof(FX_SAVE_AREA) == 16);

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

struct KTIMER_TABLE_ENTRY {
	LIST_ENTRY Entry;
	ULARGE_INTEGER Time;
};
using PKTIMER_TABLE_ENTRY = KTIMER_TABLE_ENTRY *;


inline KTHREAD KiIdleThread;
inline uint8_t alignas(4) KiIdleThreadStack[KERNEL_STACK_SIZE];

// List of all thread that can be scheduled, one for each priority level
inline LIST_ENTRY KiReadyThreadLists[NUM_OF_THREAD_PRIORITIES];

// Bitmask of KiReadyThreadLists -> bit position is one if there is at least one ready thread at that priority
inline DWORD KiReadyThreadMask = 1;

inline LIST_ENTRY KiWaitInListHead;

inline KTIMER_TABLE_ENTRY KiTimerTableListHead[TIMER_TABLE_SIZE];

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

VOID KiInitializeProcess(PKPROCESS Process, KPRIORITY BasePriority, LONG ThreadQuantum);

VOID XBOXAPI KiTimerExpiration(PKDPC Dpc, PVOID DeferredContext, PVOID SystemArgument1, PVOID SystemArgument2);
