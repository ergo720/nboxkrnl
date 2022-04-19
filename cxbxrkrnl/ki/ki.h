/*
 * ergo720                Copyright (c) 2022
 */

#pragma once

#include "..\types.h"
#include "..\ke\ke.h"

#define KERNEL_STACK_SIZE 12288
#define KERNEL_BASE 0x80010000

#define EXCEPTION_CHAIN_END reinterpret_cast<struct EXCEPTION_REGISTRATION_RECORD *>(0xFFFFFFFF)

// cr0 flags
#define CR0_TS (1 << 3) // task switched
#define CR0_MP (1 << 1) // monitor coprocessor

#define SIZE_OF_FPU_REGISTERS        128
#define NPX_STATE_NOT_LOADED (CR0_TS | CR0_MP) // x87 fpu, XMM, and MXCSR registers not saved
#define NPX_STATE_LOADED 0                     // x87 fpu, XMM, and MXCSR registers saved

#define TIMER_TABLE_SIZE 32

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
inline uint8_t KiIdleThreadStack[KERNEL_STACK_SIZE];
inline LIST_ENTRY KiWaitInListHead;

inline KTIMER_TABLE_ENTRY KiTimerTableListHead[TIMER_TABLE_SIZE];

inline KDPC KiTimerExpireDpc;

extern KPCR KiPcr;
extern KTSS KiTss;
extern const KGDT KiGdt[5];
extern const KIDT KiIdt[32];
inline constexpr uint16_t KiGdtLimit = sizeof(KiGdt) - 1;
inline constexpr uint16_t KiIdtLimit = sizeof(KiIdt) - 1;
inline constexpr size_t KiFxAreaSize = sizeof(FX_SAVE_AREA);


void InitializeCrt();
void KiInitializeKernel();
void KiInitSystem();

void KiTrap0();
void KiTrap1();
void KiTrap2();
void KiTrap3();
void KiTrap4();
void KiTrap5();
void KiTrap6();
void KiTrap7();
void KiTrap8();
void KiTrap10();
void KiTrap11();
void KiTrap12();
void KiTrap13();
void KiTrap14();
void KiTrap16();
void KiTrap17();
void KiTrap18();
void KiTrap19();
void KiUnexpectedInterrupt();

VOID XBOXAPI KiTimerExpiration
(
    PKDPC Dpc,
    PVOID DeferredContext,
    PVOID SystemArgument1,
    PVOID SystemArgument2
);
