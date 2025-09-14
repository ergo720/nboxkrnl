/*
 * ergo720                Copyright (c) 2022
 */


#include "ki\ki.hpp"


[[noreturn]] __declspec(naked) VOID KernelEntry()
{
	// Assumptions: cs/ds/ss/es/fs/gs base=0 and flags=valid; physical memory and contiguous memory identity mapped with large pages;
	// cpl=0; stack=crypt keys; eflags|if,df=0, cr4|pse,osfxsr,osxmmexcpt=1, cr0|mp,em,ts=0/ne,pe,pg=1

	// clang-format off
	__asm {
		// Load the eeprom and certificate keys. The host should have passed them in the stack
		mov esi, esp
		mov edi, offset XboxEEPROMKey
		mov ecx, 4
		rep movsd
		mov edi, offset XboxCERTKey
		mov ecx, 4
		rep movsd

		// Use the global KiIdleThreadStack as the stack of the startup thread
		xor ebp, ebp
		mov esp, offset KiIdleThreadStack + KERNEL_STACK_SIZE - (SIZE FX_SAVE_AREA + SIZE KSTART_FRAME + SIZE KSWITCHFRAME)

		// Initialize the CRT of the kernel executable
		call InitializeCrt

		// Load the GDT from the hardcoded KiGdt
		sub esp, 8
		mov ax, KiGdtLimit
		mov word ptr [esp], ax
		mov dword ptr [esp + 2], offset KiGdt
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
		mov word ptr [esp], ax
		mov dword ptr [esp + 2], offset KiIdt
		lidt [esp]

		// Load a default x87 state and mask all exceptions. This way, we can use fpu instructions without faulting (nanoprintf uses them when printing floats)
		fninit
		push 0x0000027F
		fldcw word ptr [esp]

		// Initialize the rest of the kernel
		call KiInitializeKernel // won't return
	}
	// clang-format on
}
