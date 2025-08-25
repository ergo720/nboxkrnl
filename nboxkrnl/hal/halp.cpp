/*
 * ergo720                Copyright (c) 2023
 */

#include "hal.hpp"
#include "halp.hpp"
#include <string.h>

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
	outb(PIC_MASTER_CMD, ICW1_ICW4_NEEDED | ICW1_CASCADE | ICW1_INTERVAL8 | ICW1_EDGE | ICW1_INIT);
	outb(PIC_SLAVE_CMD, ICW1_ICW4_NEEDED | ICW1_CASCADE | ICW1_INTERVAL8 | ICW1_EDGE | ICW1_INIT);
	outb(PIC_MASTER_DATA, PIC_MASTER_VECTOR_BASE);
	outb(PIC_SLAVE_DATA, PIC_SLAVE_VECTOR_BASE);
	outb(PIC_MASTER_DATA, 4);
	outb(PIC_SLAVE_DATA, 2);
	outb(PIC_MASTER_DATA, ICW4_8086 | ICW4_NORNAL_EOI | ICW4_NON_BUFFERED | ICW4_NOT_FULLY_NESTED);
	outb(PIC_SLAVE_DATA, ICW4_8086 | ICW4_NORNAL_EOI | ICW4_NON_BUFFERED | ICW4_NOT_FULLY_NESTED);
	// Mask all interrupts in the IMR (except for IRQ2 on the master)
	outb(PIC_MASTER_DATA, 0xFB);
	outb(PIC_SLAVE_DATA, 0xFF);
}

VOID HalpInitPIT()
{
	outb(PIT_PORT_CMD, PIT_COUNT_BINARY | PIT_COUNT_MODE | PIT_COUNT_16BIT | PIT_COUNT_CHAN0);
	outb(PIT_CHANNEL0_DATA, (BYTE)PIT_COUNTER_1MS);
	outb(PIT_CHANNEL0_DATA, (BYTE)(PIT_COUNTER_1MS >> 8));
}

VOID HalpInitSMCstate()
{
	ULONG BootVideoMode;
	if (NT_SUCCESS(HalReadSMBusValue(SMC_READ_ADDR, SMC_VIDEO_MODE_COMMAND, FALSE, &BootVideoMode))) {
		HalBootSMCVideoMode = BootVideoMode;
	}
}

static BOOL HalpIsCmosUpdatingTime()
{
	outb(CMOS_PORT_CMD, 0x0A);
	return inb(CMOS_PORT_DATA) & 0x80;
}

BYTE HalpReadCmosRegister(BYTE Register)
{
	outb(CMOS_PORT_CMD, Register);
	return inb(CMOS_PORT_DATA);
}

VOID HalpWriteCmosRegister(BYTE Register, BYTE Data)
{
	outb(CMOS_PORT_CMD, Register);
	outb(CMOS_PORT_DATA, Data);
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
		ASM_BEGIN
			ASM(cli);
			ASM(hlt);
		ASM_END
	}
}

VOID HalpExecuteReadSmbusCycle(UCHAR SlaveAddress, UCHAR CommandCode, BOOLEAN ReadWordValue)
{
	outb(SMBUS_ADDRESS, SlaveAddress | HA_RC);
	outb(SMBUS_COMMAND, CommandCode);
	if (ReadWordValue) {
		outb(SMBUS_CONTROL, GE_HOST_STC | GE_HCYC_EN | GE_RW_WORD);
	}
	else {
		outb(SMBUS_CONTROL, GE_HOST_STC | GE_HCYC_EN | GE_RW_BYTE);
	}
}

VOID HalpExecuteWriteSmbusCycle(UCHAR SlaveAddress, UCHAR CommandCode, BOOLEAN WriteWordValue, ULONG DataValue)
{
	outb(SMBUS_ADDRESS, SlaveAddress & ~HA_RC);
	outb(SMBUS_COMMAND, CommandCode);
	if (WriteWordValue) {
		outb(SMBUS_DATA, DataValue);
		outb(SMBUS_CONTROL, GE_HOST_STC | GE_HCYC_EN | GE_RW_WORD);
	}
	else {
		outw(SMBUS_DATA, DataValue);
		outb(SMBUS_CONTROL, GE_HOST_STC | GE_HCYC_EN | GE_RW_BYTE);
	}
}

VOID HalpExecuteBlockReadSmbusCycle(UCHAR SlaveAddress, UCHAR CommandCode)
{
	outb(SMBUS_ADDRESS, SlaveAddress | HA_RC);
	outb(SMBUS_COMMAND, CommandCode);
	outb(SMBUS_DATA, HalpSmbusCycleInfo.BlockAmount);
	outb(SMBUS_CONTROL, GE_HOST_STC | GE_HCYC_EN | GE_RW_BLOCK);
}

VOID HalpExecuteBlockWriteSmbusCycle(UCHAR SlaveAddress, UCHAR CommandCode, PBYTE Data)
{
	outb(SMBUS_ADDRESS, SlaveAddress & ~HA_RC);
	outb(SMBUS_COMMAND, CommandCode);
	outb(SMBUS_DATA, HalpSmbusCycleInfo.BlockAmount);
	for (unsigned i = 0; i < HalpSmbusCycleInfo.BlockAmount; ++i) {
		outb(SMBUS_FIFO, Data[i]);
	}
	outb(SMBUS_CONTROL, GE_HOST_STC | GE_HCYC_EN | GE_RW_BLOCK);
}

VOID XBOXAPI HalpSmbusDpcRoutine(PKDPC Dpc, PVOID DeferredContext, PVOID SystemArgument1, PVOID SystemArgument2)
{
	// NOTE: the smbus implementation in nxbx only sets GS_PRERR_STS or GS_HCYC_STS

	ULONG SmbusStatus = (ULONG)SystemArgument1;
	if (SmbusStatus & GS_PRERR_STS) {
		HalpSmbusCycleInfo.Status = STATUS_IO_DEVICE_ERROR;
		memset(HalpSmbusCycleInfo.Data, 0, 32);
	}
	else {
		HalpSmbusCycleInfo.Status = STATUS_SUCCESS;
		if (HalpSmbusCycleInfo.IsWrite == FALSE) {
			if (HalpSmbusCycleInfo.BlockAmount) {
				for (unsigned i = 0; i < HalpSmbusCycleInfo.BlockAmount; ++i) {
					HalpSmbusCycleInfo.Data[i] = inb(SMBUS_FIFO);
				}
			}
			else {
				PULONG Buffer = (PULONG)HalpSmbusCycleInfo.Data;
				*Buffer = inw(SMBUS_DATA);
			}
		}
	}

	KeSetEvent(&HalpSmbusCycleInfo.EventComplete, 0, FALSE);
}

NTSTATUS HalpReadSMBusBlock(UCHAR SlaveAddress, UCHAR CommandCode, UCHAR ReadAmount, BYTE *Buffer)
{
	if ((ReadAmount == 0) || (ReadAmount > 32)) {
		return STATUS_IO_DEVICE_ERROR;
	}

	KeEnterCriticalRegion(); // prevent suspending this thread while we hold the smbus lock below
	KeWaitForSingleObject(&HalpSmbusCycleInfo.EventLock, Executive, KernelMode, FALSE, nullptr); // prevent concurrent smbus cycles

	HalpSmbusCycleInfo.IsWrite = FALSE;
	HalpSmbusCycleInfo.BlockAmount = ReadAmount;
	HalpExecuteBlockReadSmbusCycle(SlaveAddress, CommandCode);

	KeWaitForSingleObject(&HalpSmbusCycleInfo.EventComplete, Executive, KernelMode, FALSE, nullptr); // wait until the cycle is completed by the dpc

	NTSTATUS Status = HalpSmbusCycleInfo.Status;
	for (unsigned i = 0; i < HalpSmbusCycleInfo.BlockAmount; ++i) {
		Buffer[i] = HalpSmbusCycleInfo.Data[i];
	}

	KeSetEvent(&HalpSmbusCycleInfo.EventLock, 0, FALSE);
	KeLeaveCriticalRegion();

	return Status;
}

NTSTATUS HalpWriteSMBusBlock(UCHAR SlaveAddress, UCHAR CommandCode, UCHAR WriteAmount, BYTE *Buffer)
{
	if ((WriteAmount == 0) || (WriteAmount > 32)) {
		return STATUS_IO_DEVICE_ERROR;
	}

	KeEnterCriticalRegion(); // prevent suspending this thread while we hold the smbus lock below
	KeWaitForSingleObject(&HalpSmbusCycleInfo.EventLock, Executive, KernelMode, FALSE, nullptr); // prevent concurrent smbus cycles

	HalpSmbusCycleInfo.IsWrite = TRUE;
	HalpSmbusCycleInfo.BlockAmount = WriteAmount;
	HalpExecuteBlockWriteSmbusCycle(SlaveAddress, CommandCode, Buffer);

	KeWaitForSingleObject(&HalpSmbusCycleInfo.EventComplete, Executive, KernelMode, FALSE, nullptr); // wait until the cycle is completed by the dpc

	NTSTATUS Status = HalpSmbusCycleInfo.Status;

	KeSetEvent(&HalpSmbusCycleInfo.EventLock, 0, FALSE);
	KeLeaveCriticalRegion();

	return Status;
}
