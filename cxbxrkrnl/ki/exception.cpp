/*
 * ergo720                Copyright (c) 2022
 */

#include "ki.h"
#include "..\hal\halp.h"


// We don't expect any exceptions at the moment, so we will just abort if we do get them.
// These should probably call KeBugCheck in the future

// Divide Error
void __declspec(naked) XBOXAPI KiTrap0()
{
	HalpShutdownSystem();
}

// Debug breakpoint
void __declspec(naked) XBOXAPI KiTrap1()
{
	HalpShutdownSystem();
}

// NMI interrupt
void __declspec(naked) XBOXAPI KiTrap2()
{
	HalpShutdownSystem();
}

// Breakpoint (int 3)
void __declspec(naked) XBOXAPI KiTrap3()
{
	HalpShutdownSystem();
}

// Overflow (int O)
void __declspec(naked) XBOXAPI KiTrap4()
{
	HalpShutdownSystem();
}

// Bound range exceeded
void __declspec(naked) XBOXAPI KiTrap5()
{
	HalpShutdownSystem();
}

// Invalid opcode
void __declspec(naked) XBOXAPI KiTrap6()
{
	HalpShutdownSystem();
}

// No math coprocessor
void __declspec(naked) XBOXAPI KiTrap7()
{
	HalpShutdownSystem();
}

// Double fault
void __declspec(naked) XBOXAPI KiTrap8()
{
	HalpShutdownSystem();
}

// Invalid tss
void __declspec(naked) XBOXAPI KiTrap10()
{
	HalpShutdownSystem();
}

// Segment not present
void __declspec(naked) XBOXAPI KiTrap11()
{
	HalpShutdownSystem();
}

// Stack-segment fault
void __declspec(naked) XBOXAPI KiTrap12()
{
	HalpShutdownSystem();
}

// General protection
void __declspec(naked) XBOXAPI KiTrap13()
{
	HalpShutdownSystem();
}

// Page fault
void __declspec(naked) XBOXAPI KiTrap14()
{
	HalpShutdownSystem();
}

// Math fault
void __declspec(naked) XBOXAPI KiTrap16()
{
	HalpShutdownSystem();
}

// Alignment check
void __declspec(naked) XBOXAPI KiTrap17()
{
	HalpShutdownSystem();
}

// Machine check
void __declspec(naked) XBOXAPI KiTrap18()
{
	HalpShutdownSystem();
}

// SIMD floating-point exception
void __declspec(naked) XBOXAPI KiTrap19()
{
	HalpShutdownSystem();
}

// Used to catch any intel-reserved exception
void __declspec(naked) XBOXAPI KiUnexpectedInterrupt()
{
	HalpShutdownSystem();
}
