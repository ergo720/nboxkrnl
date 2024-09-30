/*
 * ergo720                Copyright (c) 2022
 */


#include "ki\ki.hpp"


[[noreturn]] __declspec(naked) VOID KernelEntry()
{
	// Assumptions: cs/ds/ss/es/fs/gs base=0 and flags=valid; physical memory and contiguous memory identity mapped with large pages;
	// protected mode and paging=on; cpl=0; stack=crypt keys; interrupts=off, df=0

	ASM_BEGIN
		// Load the eeprom and certificate keys. The host should have passed them in the stack
		ASM(mov esi, esp);
		ASM(mov edi, offset XboxEEPROMKey);
		ASM(mov ecx, 4);
		ASM(rep movsd);
		ASM(mov edi, offset XboxCERTKey);
		ASM(mov ecx, 4);
		ASM(rep movsd);

		// Use the global KiIdleThreadStack as the stack of the startup thread
		ASM(xor ebp, ebp);
		ASM(mov esp, offset KiIdleThreadStack + KERNEL_STACK_SIZE - (SIZE FX_SAVE_AREA + SIZE KSTART_FRAME + SIZE KSWITCHFRAME));

		// Update cr0
		ASM(mov eax, cr0);
		ASM(or eax, CR0_NE);
		ASM(mov cr0, eax);

		// Initialize the CRT of the kernel executable
		ASM(call InitializeCrt);

		// Load the GDT from the hardcoded KiGdt
		ASM(sub esp, 8);
		ASM(mov ax, KiGdtLimit);
		ASM(mov word ptr [esp], ax);
		ASM(mov dword ptr [esp + 2], offset KiGdt);
		ASM(lgdt [esp]);

		// Load the segment selectors
		ASM(push 0x8);
		ASM(push reload_CS);
		ASM(_emit 0xCB);

	reload_CS:
		ASM(mov ax, 0x10);
		ASM(mov ds, ax);
		ASM(mov es, ax);
		ASM(mov ss, ax);
		ASM(mov gs, ax);
		ASM(mov ax, 0x18);
		ASM(mov fs, ax);

		// Load the tss from the hardcoded KiTss
		ASM(mov ax, 0x20);
		ASM(ltr ax);

		// Load the IDT from the hardcoded KiIdt
		ASM(mov ax, KiIdtLimit);
		ASM(mov word ptr [esp], ax);
		ASM(mov dword ptr [esp + 2], offset KiIdt);
		ASM(lidt [esp]);
	ASM_END

	KiInitializeKernel(); // won't return
}
