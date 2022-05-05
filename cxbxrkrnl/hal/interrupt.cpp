/*
 * ergo720                Copyright (c) 2022
 */

#include "halp.h"


VOID XBOXAPI HalpSwIntApc()
{
	// Apc interrupt not implemented yet
	__asm {
		cli
		hlt
	}
}

VOID XBOXAPI HalpSwIntDpc()
{
	// Dpc interrupt not implemented yet
	__asm {
		cli
		hlt
	}
}

VOID XBOXAPI HalpHwInt0()
{
	__asm {
		int IDT_INT_VECTOR_BASE + 0
	}
}

VOID XBOXAPI HalpHwInt1()
{
	__asm {
		int IDT_INT_VECTOR_BASE + 1
	}
}

VOID XBOXAPI HalpHwInt2()
{
	__asm {
		int IDT_INT_VECTOR_BASE+ 2
	}
}

VOID XBOXAPI HalpHwInt3()
{
	__asm {
		int IDT_INT_VECTOR_BASE + 3
	}
}

VOID XBOXAPI HalpHwInt4()
{
	__asm {
		int IDT_INT_VECTOR_BASE + 4
	}
}

VOID XBOXAPI HalpHwInt5()
{
	__asm {
		int IDT_INT_VECTOR_BASE + 5
	}
}

VOID XBOXAPI HalpHwInt6()
{
	__asm {
		int IDT_INT_VECTOR_BASE + 6
	}
}

VOID XBOXAPI HalpHwInt7()
{
	__asm {
		int IDT_INT_VECTOR_BASE + 7
	}
}

VOID XBOXAPI HalpHwInt8()
{
	__asm {
		int IDT_INT_VECTOR_BASE + 8
	}
}

VOID XBOXAPI HalpHwInt9()
{
	__asm {
		int IDT_INT_VECTOR_BASE + 9
	}
}

VOID XBOXAPI HalpHwInt10()
{
	__asm {
		int IDT_INT_VECTOR_BASE + 10
	}
}

VOID XBOXAPI HalpHwInt11()
{
	__asm {
		int IDT_INT_VECTOR_BASE + 11
	}
}

VOID XBOXAPI HalpHwInt12()
{
	__asm {
		int IDT_INT_VECTOR_BASE + 12
	}
}

VOID XBOXAPI HalpHwInt13()
{
	__asm {
		int IDT_INT_VECTOR_BASE + 13
	}
}

VOID XBOXAPI HalpHwInt14()
{
	__asm {
		int IDT_INT_VECTOR_BASE + 14
	}
}

VOID XBOXAPI HalpHwInt15()
{
	__asm {
		int IDT_INT_VECTOR_BASE + 15
	}
}
