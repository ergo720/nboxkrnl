/*
 * ergo720                Copyright (c) 2023
 */

#include "halp.hpp"

// PIT i/o ports
#define PIT_CHANNEL0_DATA  0x40
#define PIT_PORT_CMD       0x43

// PIT initialization commands
#define PIT_COUNT_BINARY   0x00
#define PIT_COUNT_MODE     0x04
#define PIT_COUNT_16BIT    0x30
#define PIT_COUNT_CHAN0    0x00

// PIT counter required to trigger IRQ0 every one ms
// NOTE: on the xbox, the pit frequency is 6% lower than the default one, see https://xboxdevwiki.net/Porting_an_Operating_System_to_the_Xbox_HOWTO#Timer_Frequency
#define PIT_COUNTER_1MS    1125


VOID HalpInitPIC()
{
	__asm {
		mov al, ICW1_ICW4_NEEDED | ICW1_CASCADE | ICW1_INTERVAL8 | ICW1_EDGE | ICW1_INIT
		out PIC_MASTER_CMD, al
		out PIC_SLAVE_CMD, al
		mov al, PIC_MASTER_VECTOR_BASE
		out PIC_MASTER_DATA, al
		mov al, PIC_SLAVE_VECTOR_BASE
		out PIC_SLAVE_DATA, al
		mov al, 4
		out PIC_MASTER_DATA, al
		mov al, 2
		out PIC_SLAVE_DATA, al
		mov al, ICW4_8086 | ICW4_NORNAL_EOI | ICW4_NON_BUFFERED | ICW4_NOT_FULLY_NESTED
		out PIC_MASTER_DATA, al
		out PIC_SLAVE_DATA, al
		// Mask all interrupts in the IMR
		mov al, 0xFF
		out PIC_MASTER_DATA, al
		out PIC_SLAVE_DATA, al
	}
}

VOID HalpInitPIT()
{
	__asm {
		mov al, PIT_COUNT_BINARY | PIT_COUNT_MODE | PIT_COUNT_16BIT | PIT_COUNT_CHAN0
		out PIT_PORT_CMD, al
		mov ax, PIT_COUNTER_1MS
		out PIT_CHANNEL0_DATA, al
		shr ax, 8
		out PIT_CHANNEL0_DATA, al
	}
}

VOID HalpShutdownSystem()
{
	OutputToHost(0, KE_ABORT);

	while (true) {
		__asm {
			cli
			hlt
		}
	}
}
