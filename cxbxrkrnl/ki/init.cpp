/*
 * ergo720                Copyright (c) 2022
 */

#include "ki.h"
#include "..\kernel.h"
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

const KIDT KiIdt[] = {
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
	((uint64_t)0x8 << 16) | ((uint64_t)&KiUnexpectedInterrupt & 0x0000FFFF) | (((uint64_t)&KiUnexpectedInterrupt & 0xFFFF0000) << 32) | ((uint64_t)0x8E00 << 32)
};


void InitializeCrt()
{
	PIMAGE_DOS_HEADER dosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(KERNEL_BASE);
	PIMAGE_NT_HEADERS32 pNtHeader = reinterpret_cast<PIMAGE_NT_HEADERS32>(KERNEL_BASE + dosHeader->e_lfanew);

	PIMAGE_SECTION_HEADER crtSection = nullptr;
	PIMAGE_SECTION_HEADER section = reinterpret_cast<PIMAGE_SECTION_HEADER>(pNtHeader + 1);
	for (WORD i = 0; i < pNtHeader->FileHeader.NumberOfSections; i++) {
		if (strcmp_((char *)&section[i].Name, ".CRT") == 0) {
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

void KiInitializeKernel()
{
	KiPcr.SelfPcr = &KiPcr;
	KiPcr.Prcb = &KiPcr.PrcbData;

	KiPcr.NtTib.ExceptionList = EXCEPTION_CHAIN_END;
	KiPcr.NtTib.StackLimit = KiIdleThreadStack;
	KiPcr.NtTib.StackBase = KiIdleThreadStack + KERNEL_STACK_SIZE - KiFxAreaSize;
	KiPcr.PrcbData.CurrentThread = &KiIdleThread;

	KiIdleThread.NpxState = NPX_STATE_NOT_LOADED;
	KiIdleThread.StackLimit = KiPcr.NtTib.StackLimit;
	KiIdleThread.StackBase = static_cast<uint8_t *>(KiPcr.NtTib.StackBase) + KiFxAreaSize;

	InitializeListHead(&KiPcr.Prcb->DpcListHead);
	KiPcr.Prcb->DpcRoutineActive = FALSE;

	KiInitSystem();
}
