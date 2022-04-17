/*
 * ergo720                Copyright (c) 2022
 */

#define _HAS_EXCEPTIONS 0

#include "ki\ki.h"


[[noreturn]] void KernelEntry()
{
	// Assumptions: cs/ds/ss/es/fs/gs base=0 and flags=valid; physical memory and contiguous memory identity mapped with large pages;
	// protected mode and paging on, cpl=0, stack=valid

	__asm {
		// Disable interrupts
		cli

		// Use the global KiIdleThreadStack as the stack of the startup thread
		xor ebp, ebp
		mov esp, offset KiIdleThreadStack + KERNEL_STACK_SIZE

		// Initialize the CRT of the kernel executable
		call InitializeCrt

		// Load the GDT from the hardcoded KiGdt
		mov ax, KiGdtSize
		mov WORD PTR [esp], ax
		mov DWORD PTR [esp+2], offset KiGdt
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

		cli
		hlt
	}
}
