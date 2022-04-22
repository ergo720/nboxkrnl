/*
 * ergo720                Copyright (c) 2022
 */


#include "ki\ki.h"
#include "ke\bug_codes.h"


[[noreturn]] void KernelEntry()
{
	// Assumptions: cs/ds/ss/es/fs/gs base=0 and flags=valid; physical memory and contiguous memory identity mapped with large pages;
	// protected mode and paging on, cpl=0, stack=valid

	__asm {
		// Disable interrupts
		cli

		// Use the global KiIdleThreadStack as the stack of the startup thread
		xor ebp, ebp
		mov esp, offset KiIdleThreadStack + KERNEL_STACK_SIZE - SIZE FX_SAVE_AREA

		// Initialize the CRT of the kernel executable
		call InitializeCrt

		// Load the GDT from the hardcoded KiGdt
		mov ax, KiGdtLimit
		mov WORD PTR [esp], ax
		mov DWORD PTR [esp + 2], offset KiGdt
		lgdt [esp]

		// Load the segment selectors
		push 0x8
		push reload_CS
		_emit 0xCB

	reload_CS:
		mov ax, 0x10
		mov ds, ax
		mov es, ax
		mov ss, ax
		mov gs, ax
		mov ax, 0x18
		mov fs, ax

		// Load the tss from the hardcoded KiTss
		mov ax, 0x20
		ltr ax

		// Load the IDT from the hardcoded KiIdt
		mov ax, KiIdtLimit
		mov WORD PTR [esp], ax
		mov DWORD PTR [esp + 2], offset KiIdt
		lidt [esp]
	}

	KiInitializeKernel();

	// We should never return from the entry point
	KeBugCheck(BUG_KI_INIT);
}
