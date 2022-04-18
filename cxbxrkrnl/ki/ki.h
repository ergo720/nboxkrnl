/*
 * ergo720                Copyright (c) 2022
 */

#pragma once

#include "..\types.h"

#define KERNEL_STACK_SIZE 12288
#define KERNEL_BASE 0x10000

#define SIZE_OF_FPU_REGISTERS        128

using KGDT = uint64_t;
using KTSS = uint32_t[26];
using KIRQL = UCHAR;

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

struct FX_SAVE_AREA {
    FLOATING_SAVE_AREA FloatSave;
    ULONG Align16Byte[3];
};
using PFX_SAVE_AREA = FX_SAVE_AREA *;

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


extern KPCR KiPcr;
extern uint8_t KiIdleThreadStack[KERNEL_STACK_SIZE];

extern KTSS KiTss;
extern const KGDT KiGdt[5];
inline constexpr uint16_t KiGdtSize = sizeof(KiGdt);


void InitializeCrt();
