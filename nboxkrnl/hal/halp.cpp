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

// CMOS i/o ports
#define CMOS_PORT_CMD 0x70
#define CMOS_PORT_DATA 0x71


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
		// Mask all interrupts in the IMR (except for IRQ2 on the master)
		mov al, 0xFB
		out PIC_MASTER_DATA, al
		add al, 4
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

static BOOL HalpIsCmosUpdatingTime()
{
	outb(CMOS_PORT_CMD, 0x0A);
	return inb(CMOS_PORT_DATA) & 0x80;
}

static BYTE HalpReadCmosRegister(BYTE Register)
{
	outb(CMOS_PORT_CMD, Register);
	return inb(CMOS_PORT_DATA);
}

VOID HalpReadCmosTime(PTIME_FIELDS TimeFields)
{
	while (HalpIsCmosUpdatingTime()) {}

	TimeFields->Millisecond = 0;
	TimeFields->Second = HalpReadCmosRegister(0x00);
	TimeFields->Minute = HalpReadCmosRegister(0x02);
	TimeFields->Hour = HalpReadCmosRegister(0x04);
	TimeFields->Day = HalpReadCmosRegister(0x07);
	TimeFields->Month = HalpReadCmosRegister(0x08);
	TimeFields->Year = HalpReadCmosRegister(0x09);
	BYTE Century = HalpReadCmosRegister(0x7F);
	USHORT LastSecond, LastMinute, LastHour, LastDay, LastMonth, LastYear, LastCentury;

	do {
		LastSecond = TimeFields->Second;
		LastMinute = TimeFields->Minute;
		LastHour = TimeFields->Hour;
		LastDay = TimeFields->Day;
		LastMonth = TimeFields->Month;
		LastYear = TimeFields->Year;
		LastCentury = Century;

		while (HalpIsCmosUpdatingTime()) {}

		TimeFields->Second = HalpReadCmosRegister(0x00);
		TimeFields->Minute = HalpReadCmosRegister(0x02);
		TimeFields->Hour = HalpReadCmosRegister(0x04);
		TimeFields->Day = HalpReadCmosRegister(0x07);
		TimeFields->Month = HalpReadCmosRegister(0x08);
		TimeFields->Year = HalpReadCmosRegister(0x09);
		Century = HalpReadCmosRegister(0x7F);

	} while ((LastSecond != TimeFields->Second) || (LastMinute != TimeFields->Minute) || (LastHour != TimeFields->Hour) ||
		(LastDay != TimeFields->Day) || (LastMonth != TimeFields->Month) || (LastYear != TimeFields->Year) || (LastCentury != Century));

	BYTE RegisterB = HalpReadCmosRegister(0x0B);

	if (!(RegisterB & 0x04)) { // bcd -> binary
		TimeFields->Second = (TimeFields->Second & 0x0F) + ((TimeFields->Second / 16) * 10);
		TimeFields->Minute = (TimeFields->Minute & 0x0F) + ((TimeFields->Minute / 16) * 10);
		TimeFields->Hour = ((TimeFields->Hour & 0x0F) + (((TimeFields->Hour & 0x70) / 16) * 10)) | (TimeFields->Hour & 0x80);
		TimeFields->Day = (TimeFields->Day & 0x0F) + ((TimeFields->Day / 16) * 10);
		TimeFields->Month = (TimeFields->Month & 0x0F) + ((TimeFields->Month / 16) * 10);
		TimeFields->Year = (TimeFields->Year & 0x0F) + ((TimeFields->Year / 16) * 10);
		Century = (Century & 0x0F) + ((Century / 16) * 10);
	}

	if (!(RegisterB & 0x02) && (TimeFields->Hour & 0x80)) { // 12 -> 24 hour format
		TimeFields->Hour = ((TimeFields->Hour & 0x7F) + 12) % 24;
	}

	// Calculate the full 4-digit year
	TimeFields->Year += (Century * 100);
}

VOID HalpShutdownSystem()
{
	outl(KE_ABORT, 0);

	while (true) {
		__asm {
			cli
			hlt
		}
	}
}
