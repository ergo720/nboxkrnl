/*
 * ergo720                Copyright (c) 2022
 */

#include "ki.hpp"
#include "hw_exp.hpp"
#include "..\mm\mm.hpp"
#include "..\ob\ob.hpp"
#include "..\kernel.hpp"
#include "..\hal\hal.hpp"
#include "..\ps\ps.hpp"
#include "..\io\io.hpp"
#include <string.h>


KTSS KiTss = {
	0,         // previous tss selector
	0,         // esp0
	0,         // ss0
	0,         // esp1
	0,         // ss1
	0,         // esp2
	0,         // ss2
	0xF000,    // cr3
	0,         // eip
	0,         // eflags
	0,         // eax
	0,         // ecx
	0,         // edx
	0,         // ebx
	0,         // esp
	0,         // ebp
	0,         // esi
	0,         // edi
	0x10,      // es
	0x8,       // cs
	0x10,      // ss
	0x10,      // ds
	0x18,      // fs
	0x10,      // gs
	0,         // ldt selector
	0x67 << 16 // i/o map table address
};

const KGDT KiGdt[] = {
	// first entry is always a null segment descriptor
	0,
	// 32bit code segment, non-conforming, rx, present
	0xCF9B000000FFFF,
	// 32bit data segment, rw, present
	0xCF93000000FFFF,
	// 32bit data segment, rw, present (used for fs)
	(((uint64_t)&KiPcr & 0x00FFFFFF) << 16) | (((uint64_t)&KiPcr & 0xFF000000) << 32) | ((uint64_t)0xC093 << 40),
	// 32bit tss, present
	(((uint64_t)&KiTss & 0x00FFFFFF) << 16) | (((uint64_t)&KiTss & 0xFF000000) << 32) | ((uint64_t)0x89 << 40) | ((uint64_t)0x67)
};

KIDT KiIdt[] = {
	// These are the exception handlers, as specified in the x86 architecture
	((uint64_t)0x8 << 16) | ((uint64_t)&KiTrap0 & 0x0000FFFF) | (((uint64_t)&KiTrap0 & 0xFFFF0000) << 32) | ((uint64_t)0x8E00 << 32),
	((uint64_t)0x8 << 16) | ((uint64_t)&KiTrap1 & 0x0000FFFF) | (((uint64_t)&KiTrap1 & 0xFFFF0000) << 32) | ((uint64_t)0x8E00 << 32),
	((uint64_t)0x8 << 16) | ((uint64_t)&KiTrap2 & 0x0000FFFF) | (((uint64_t)&KiTrap2 & 0xFFFF0000) << 32) | ((uint64_t)0x8E00 << 32),
	((uint64_t)0x8 << 16) | ((uint64_t)&KiTrap3 & 0x0000FFFF) | (((uint64_t)&KiTrap3 & 0xFFFF0000) << 32) | ((uint64_t)0x8E00 << 32),
	((uint64_t)0x8 << 16) | ((uint64_t)&KiTrap4 & 0x0000FFFF) | (((uint64_t)&KiTrap4 & 0xFFFF0000) << 32) | ((uint64_t)0x8E00 << 32),
	((uint64_t)0x8 << 16) | ((uint64_t)&KiTrap5 & 0x0000FFFF) | (((uint64_t)&KiTrap5 & 0xFFFF0000) << 32) | ((uint64_t)0x8E00 << 32),
	((uint64_t)0x8 << 16) | ((uint64_t)&KiTrap6 & 0x0000FFFF) | (((uint64_t)&KiTrap6 & 0xFFFF0000) << 32) | ((uint64_t)0x8E00 << 32),
	((uint64_t)0x8 << 16) | ((uint64_t)&KiTrap7 & 0x0000FFFF) | (((uint64_t)&KiTrap7 & 0xFFFF0000) << 32) | ((uint64_t)0x8E00 << 32),
	((uint64_t)0x8 << 16) | ((uint64_t)&KiTrap8 & 0x0000FFFF) | (((uint64_t)&KiTrap8 & 0xFFFF0000) << 32) | ((uint64_t)0x8E00 << 32),
	((uint64_t)0x8 << 16) | ((uint64_t)&KiUnexpectedInterrupt & 0x0000FFFF) | (((uint64_t)&KiUnexpectedInterrupt & 0xFFFF0000) << 32) | ((uint64_t)0x8E00 << 32),
	((uint64_t)0x8 << 16) | ((uint64_t)&KiTrap10 & 0x0000FFFF) | (((uint64_t)&KiTrap10 & 0xFFFF0000) << 32) | ((uint64_t)0x8E00 << 32),
	((uint64_t)0x8 << 16) | ((uint64_t)&KiTrap11 & 0x0000FFFF) | (((uint64_t)&KiTrap11 & 0xFFFF0000) << 32) | ((uint64_t)0x8E00 << 32),
	((uint64_t)0x8 << 16) | ((uint64_t)&KiTrap12 & 0x0000FFFF) | (((uint64_t)&KiTrap12 & 0xFFFF0000) << 32) | ((uint64_t)0x8E00 << 32),
	((uint64_t)0x8 << 16) | ((uint64_t)&KiTrap13 & 0x0000FFFF) | (((uint64_t)&KiTrap13 & 0xFFFF0000) << 32) | ((uint64_t)0x8E00 << 32),
	((uint64_t)0x8 << 16) | ((uint64_t)&KiTrap14 & 0x0000FFFF) | (((uint64_t)&KiTrap14 & 0xFFFF0000) << 32) | ((uint64_t)0x8E00 << 32),
	((uint64_t)0x8 << 16) | ((uint64_t)&KiUnexpectedInterrupt & 0x0000FFFF) | (((uint64_t)&KiUnexpectedInterrupt & 0xFFFF0000) << 32) | ((uint64_t)0x8E00 << 32),
	((uint64_t)0x8 << 16) | ((uint64_t)&KiTrap16 & 0x0000FFFF) | (((uint64_t)&KiTrap16 & 0xFFFF0000) << 32) | ((uint64_t)0x8E00 << 32),
	((uint64_t)0x8 << 16) | ((uint64_t)&KiTrap17 & 0x0000FFFF) | (((uint64_t)&KiTrap17 & 0xFFFF0000) << 32) | ((uint64_t)0x8E00 << 32),
	((uint64_t)0x8 << 16) | ((uint64_t)&KiTrap18 & 0x0000FFFF) | (((uint64_t)&KiTrap18 & 0xFFFF0000) << 32) | ((uint64_t)0x8E00 << 32),
	((uint64_t)0x8 << 16) | ((uint64_t)&KiTrap19 & 0x0000FFFF) | (((uint64_t)&KiTrap19 & 0xFFFF0000) << 32) | ((uint64_t)0x8E00 << 32),
	((uint64_t)0x8 << 16) | ((uint64_t)&KiUnexpectedInterrupt & 0x0000FFFF) | (((uint64_t)&KiUnexpectedInterrupt & 0xFFFF0000) << 32) | ((uint64_t)0x8E00 << 32),
	((uint64_t)0x8 << 16) | ((uint64_t)&KiUnexpectedInterrupt & 0x0000FFFF) | (((uint64_t)&KiUnexpectedInterrupt & 0xFFFF0000) << 32) | ((uint64_t)0x8E00 << 32),
	((uint64_t)0x8 << 16) | ((uint64_t)&KiUnexpectedInterrupt & 0x0000FFFF) | (((uint64_t)&KiUnexpectedInterrupt & 0xFFFF0000) << 32) | ((uint64_t)0x8E00 << 32),
	((uint64_t)0x8 << 16) | ((uint64_t)&KiUnexpectedInterrupt & 0x0000FFFF) | (((uint64_t)&KiUnexpectedInterrupt & 0xFFFF0000) << 32) | ((uint64_t)0x8E00 << 32),
	((uint64_t)0x8 << 16) | ((uint64_t)&KiUnexpectedInterrupt & 0x0000FFFF) | (((uint64_t)&KiUnexpectedInterrupt & 0xFFFF0000) << 32) | ((uint64_t)0x8E00 << 32),
	((uint64_t)0x8 << 16) | ((uint64_t)&KiUnexpectedInterrupt & 0x0000FFFF) | (((uint64_t)&KiUnexpectedInterrupt & 0xFFFF0000) << 32) | ((uint64_t)0x8E00 << 32),
	((uint64_t)0x8 << 16) | ((uint64_t)&KiUnexpectedInterrupt & 0x0000FFFF) | (((uint64_t)&KiUnexpectedInterrupt & 0xFFFF0000) << 32) | ((uint64_t)0x8E00 << 32),
	((uint64_t)0x8 << 16) | ((uint64_t)&KiUnexpectedInterrupt & 0x0000FFFF) | (((uint64_t)&KiUnexpectedInterrupt & 0xFFFF0000) << 32) | ((uint64_t)0x8E00 << 32),
	((uint64_t)0x8 << 16) | ((uint64_t)&KiUnexpectedInterrupt & 0x0000FFFF) | (((uint64_t)&KiUnexpectedInterrupt & 0xFFFF0000) << 32) | ((uint64_t)0x8E00 << 32),
	((uint64_t)0x8 << 16) | ((uint64_t)&KiUnexpectedInterrupt & 0x0000FFFF) | (((uint64_t)&KiUnexpectedInterrupt & 0xFFFF0000) << 32) | ((uint64_t)0x8E00 << 32),
	((uint64_t)0x8 << 16) | ((uint64_t)&KiUnexpectedInterrupt & 0x0000FFFF) | (((uint64_t)&KiUnexpectedInterrupt & 0xFFFF0000) << 32) | ((uint64_t)0x8E00 << 32),
	((uint64_t)0x8 << 16) | ((uint64_t)&KiUnexpectedInterrupt & 0x0000FFFF) | (((uint64_t)&KiUnexpectedInterrupt & 0xFFFF0000) << 32) | ((uint64_t)0x8E00 << 32),

	// The following 16 vectors are used internally by the kernel to provide system services
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	((uint64_t)0x8 << 16) | ((uint64_t)&KiContinueService & 0x0000FFFF) | (((uint64_t)&KiContinueService & 0xFFFF0000) << 32) | ((uint64_t)0x8E00 << 32), // used by ZwContinue
	((uint64_t)0x8 << 16) | ((uint64_t)&KiRaiseExceptionService & 0x0000FFFF) | (((uint64_t)&KiRaiseExceptionService & 0xFFFF0000) << 32) | ((uint64_t)0x8E00 << 32), // used by ZwRaiseException
	0,
	0,
	0,
	0,
	0,
	0,

	// The following 16 vectors are used for hw interrupts. They need to be connected first with KeConnectInterrupt
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0
};

static_assert((KiIdtLimit + 1) / sizeof(KIDT) == 64);

void InitializeCrt()
{
	PIMAGE_DOS_HEADER dosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(KERNEL_BASE);
	PIMAGE_NT_HEADERS32 pNtHeader = reinterpret_cast<PIMAGE_NT_HEADERS32>(KERNEL_BASE + dosHeader->e_lfanew);

	PIMAGE_SECTION_HEADER crtSection = nullptr;
	PIMAGE_SECTION_HEADER section = reinterpret_cast<PIMAGE_SECTION_HEADER>(pNtHeader + 1);
	for (WORD i = 0; i < pNtHeader->FileHeader.NumberOfSections; i++) {
		if (strcmp((char *)&section[i].Name, ".CRT") == 0) {
			crtSection = &section[i];
			break;
		}
	}

	if (crtSection == nullptr) {
		return;
	}

	using CrtFunc = void(*)();
	CrtFunc *initializer = reinterpret_cast<CrtFunc *>(KERNEL_BASE + crtSection->VirtualAddress);
	while (*initializer) {
		(*initializer)();
		++initializer;
	}
}

VOID KiInitializeKernel()
{
	KiPcr.SelfPcr = &KiPcr;
	KiPcr.Prcb = &KiPcr.PrcbData;

	// NOTE: unlike the StackBase member of KTHREAD, the StackBase in NtTib is corrected by subtracting sizeof(FX_SAVE_AREA). This is necessary because otherwise
	// KiCopyKframeToContext / KiCopyContextToKframe will flush the fpu state in the wrong spot
	KiPcr.NtTib.ExceptionList = EXCEPTION_CHAIN_END;
	KiPcr.NtTib.StackLimit = KiIdleThreadStack;
	KiPcr.NtTib.StackBase = KiIdleThreadStack + KERNEL_STACK_SIZE - sizeof(FX_SAVE_AREA);
	KiPcr.NtTib.Self = &KiPcr.NtTib;
	KiPcr.PrcbData.CurrentThread = &KiIdleThread;

	InitializeListHead(&KiPcr.Prcb->DpcListHead);
	KiPcr.Prcb->DpcRoutineActive = FALSE;

	KiInitSystem();

	KiInitializeProcess(&KiIdleProcess, LOW_PRIORITY, 127);
	KiInitializeProcess(&KiUniqueProcess, NORMAL_BASE_PRIORITY, THREAD_QUANTUM);

	KeInitializeThread(&KiIdleThread, KiIdleThreadStack + KERNEL_STACK_SIZE,
		KERNEL_STACK_SIZE, 0, nullptr, nullptr, nullptr, &KiIdleProcess);

	KiIdleThread.Priority = LOW_PRIORITY;
	KiIdleThread.State = Running;
	KiIdleThread.WaitIrql = PASSIVE_LEVEL;

	KiPcr.Prcb->NextThread = nullptr;
	KiPcr.Prcb->IdleThread = &KiIdleThread;

	if (MmInitSystem() == FALSE) {
		KeBugCheckEx(INIT_FAILURE, MM_FAILURE, 0, 0, 0);
	}

	if (ObInitSystem() == FALSE) {
		KeBugCheckEx(INIT_FAILURE, OB_FAILURE, 0, 0, 0);
	}

	HalInitSystem();

	if (IoInitSystem() == FALSE) {
		KeBugCheckEx(INIT_FAILURE, IO_FAILURE, 0, 0, 0);
	}

	if (PsInitSystem() == FALSE) {
		KeBugCheckEx(INIT_FAILURE, PS_FAILURE, 0, 0, 0);
	}

	KiIdleLoopThread(); // won't return
}
