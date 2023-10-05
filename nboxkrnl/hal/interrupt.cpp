/*
 * ergo720                Copyright (c) 2022
 */

#include "hal.hpp"
#include "halp.hpp"
#include "..\rtl\rtl.hpp"


// Mask of interrupts currently disabled on the pic. 1st byte: master, 2nd byte: slave. Note that IRQ2 of the master connects to the slave, so it's never disabled
static WORD HalpIntDisabled = 0xFFFB;

VOID XBOXAPI HalpSwIntApc()
{
	// Apc interrupt not implemented yet
	RIP_UNIMPLEMENTED();
}

VOID XBOXAPI HalpSwIntDpc()
{
	// Dpc interrupt not implemented yet
	RIP_UNIMPLEMENTED();
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
		int IDT_INT_VECTOR_BASE + 2
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

VOID XBOXAPI HalpClockIsr()
{
	RIP_UNIMPLEMENTED();
}

EXPORTNUM(43) VOID XBOXAPI HalEnableSystemInterrupt
(
	ULONG BusInterruptLevel,
	KINTERRUPT_MODE InterruptMode
)
{
	__asm cli

	// NOTE: the bit in KINTERRUPT_MODE is the opposite of what needs to be set in elcr
	ULONG ElcrPort, DataPort;
	BYTE PicImr, ElcrMask = (InterruptMode == Edged) ? 0 : 1 << (BusInterruptLevel & 7);
	HalpIntDisabled &= ~(1 << BusInterruptLevel);

	if (BusInterruptLevel > 7) {
		ElcrPort = PIC_SLAVE_ELCR;
		DataPort = PIC_SLAVE_DATA;
		PicImr = (HalpIntDisabled >> 8) & 0xFF;
	}
	else {
		ElcrPort = PIC_MASTER_ELCR;
		DataPort = PIC_MASTER_DATA;
		PicImr = HalpIntDisabled & 0xFF;
	}

	__asm {
		mov edx, ElcrPort
		in al, dx
		or al, ElcrMask
		out dx, al
		mov al, PicImr
		mov edx, DataPort
		out dx, al
		sti
	}
}
