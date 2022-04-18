/*
 * ergo720                Copyright (c) 2022
 */

#include "ki.h"


// We don't expect any exceptions at the moment, so we will just abort if we do get them.
// These should probably call KeBugCheck in the future

// Divide Error
void __declspec(naked) KiTrap0()
{
	__asm {
		cli
		hlt
	}
}

// Debug breakpoint
void __declspec(naked) KiTrap1()
{
	__asm {
		cli
		hlt
	}
}

// NMI interrupt
void __declspec(naked) KiTrap2()
{
	__asm {
		cli
		hlt
	}
}

// Breakpoint (int 3)
void __declspec(naked) KiTrap3()
{
	__asm {
		cli
		hlt
	}
}

// Overflow (int O)
void __declspec(naked) KiTrap4()
{
	__asm {
		cli
		hlt
	}
}

// Bound range exceeded
void __declspec(naked) KiTrap5()
{
	__asm {
		cli
		hlt
	}
}

// Invalid opcode
void __declspec(naked) KiTrap6()
{
	__asm {
		cli
		hlt
	}
}

// No math coprocessor
void __declspec(naked) KiTrap7()
{
	__asm {
		cli
		hlt
	}
}

// Double fault
void __declspec(naked) KiTrap8()
{
	__asm {
		cli
		hlt
	}
}

// Invalid tss
void __declspec(naked) KiTrap10()
{
	__asm {
		cli
		hlt
	}
}

// Segment not present
void __declspec(naked) KiTrap11()
{
	__asm {
		cli
		hlt
	}
}

// Stack-segment fault
void __declspec(naked) KiTrap12()
{
	__asm {
		cli
		hlt
	}
}

// General protection
void __declspec(naked) KiTrap13()
{
	__asm {
		cli
		hlt
	}
}

// Page fault
void __declspec(naked) KiTrap14()
{
	__asm {
		cli
		hlt
	}
}

// Math fault
void __declspec(naked) KiTrap16()
{
	__asm {
		cli
		hlt
	}
}

// Alignment check
void __declspec(naked) KiTrap17()
{
	__asm {
		cli
		hlt
	}
}

// Machine check
void __declspec(naked) KiTrap18()
{
	__asm {
		cli
		hlt
	}
}

// SIMD floating-point exception
void __declspec(naked) KiTrap19()
{
	__asm {
		cli
		hlt
	}
}

// Used to catch any intel-reserved exception
void __declspec(naked) KiUnexpectedInterrupt()
{
	__asm {
		cli
		hlt
	}
}
