/*
 * ergo720                Copyright (c) 2023
 */

#include "hal.hpp"
#include "halp.hpp"
#include "rtl.hpp"


 // Global list of routines executed during a reboot
static LIST_ENTRY ShutdownRoutineList = { &ShutdownRoutineList , &ShutdownRoutineList };

VOID HalInitSystem()
{
	HalpInitPIC();
	HalpInitPIT();

	TIME_FIELDS TimeFields;
	LARGE_INTEGER SystemTime, OldTime;
	HalpReadCmosTime(&TimeFields);
	if (!RtlTimeFieldsToTime(&TimeFields, &SystemTime)) {
		// If the conversion fails, just set some random date for the system time instead of failing the initialization
		SystemTime.HighPart = 0x80000000;
		SystemTime.LowPart = 0;
	}
	KeSetSystemTime(&SystemTime, &OldTime);

	// Connect the PIT (clock) interrupt (NOTE: this will also enable interrupts)
	KiIdt[IDT_INT_VECTOR_BASE + 0] = ((uint64_t)0x8 << 16) | ((uint64_t)&HalpClockIsr & 0x0000FFFF) | (((uint64_t)&HalpClockIsr & 0xFFFF0000) << 32) | ((uint64_t)0x8E00 << 32);
	HalEnableSystemInterrupt(0, Edge);

	// Connect SMBUS interrupt
	KeInitializeEvent(&HalpSmbusLock, SynchronizationEvent, 1);
	KeInitializeEvent(&HalpSmbusComplete, NotificationEvent, 0);
	KeInitializeDpc(&HalpSmbusDpcObject, HalpSmbusDpcRoutine, nullptr);
	KiIdt[IDT_INT_VECTOR_BASE + 11] = ((uint64_t)0x8 << 16) | ((uint64_t)&HalpSmbusIsr & 0x0000FFFF) | (((uint64_t)&HalpSmbusIsr & 0xFFFF0000) << 32) | ((uint64_t)0x8E00 << 32);
	HalEnableSystemInterrupt(11, LevelSensitive);

	if (XboxType == SYSTEM_DEVKIT) {
		XboxHardwareInfo.Flags |= 2;
	}
	else if (XboxType == SYSTEM_CHIHIRO) {
		XboxHardwareInfo.Flags |= 8;
	}
}

EXPORTNUM(40) ULONG HalDiskCachePartitionCount = 3;

EXPORTNUM(46) VOID XBOXAPI HalReadWritePCISpace
(
	ULONG BusNumber,
	ULONG SlotNumber,
	ULONG RegisterNumber,
	PVOID Buffer,
	ULONG Length,
	BOOLEAN WritePCISpace
)
{
	disable();

	ULONG BufferOffset = 0;
	PBYTE Buffer1 = (PBYTE)Buffer;
	ULONG SizeLeft = Length;
	ULONG CfgAddr = 0x80000000 | ((BusNumber & 0xFF) << 16) | ((SlotNumber & 0x1F) << 11) | ((SlotNumber & 0xE0) << 3) | (RegisterNumber & 0xFC);

	while (SizeLeft > 0) {
		ULONG BytesToAccess = (SizeLeft > 4) ? 4 : (SizeLeft == 3) ? 2 : SizeLeft;
		ULONG RegisterOffset = RegisterNumber % 4;
		outl(PCI_CONFIG_ADDRESS, CfgAddr);

		switch (BytesToAccess)
		{
		case 1:
			if (WritePCISpace) {
				outb(PCI_CONFIG_DATA + RegisterOffset, *(Buffer1 + BufferOffset));
			}
			else {
				*(Buffer1 + BufferOffset) = inb(PCI_CONFIG_DATA + RegisterOffset);
			}
			break;

		case 2:
			if (WritePCISpace) {
				outw(PCI_CONFIG_DATA + RegisterOffset, *PUSHORT(Buffer1 + BufferOffset));
			}
			else {
				*PUSHORT(Buffer1 + BufferOffset) = inw(PCI_CONFIG_DATA + RegisterOffset);
			}
			break;

		default:
			if (WritePCISpace) {
				outl(PCI_CONFIG_DATA + RegisterOffset, *PULONG(Buffer1 + BufferOffset));
			}
			else {
				*PULONG(Buffer1 + BufferOffset) = inl(PCI_CONFIG_DATA + RegisterOffset);
			}
		}

		RegisterNumber += BytesToAccess;
		CfgAddr = (CfgAddr & ~0xFF) | (RegisterNumber >> 2);
		BufferOffset += BytesToAccess;
		SizeLeft -= BytesToAccess;
	}

	enable();
}

// Source: Cxbx-Reloaded
EXPORTNUM(47) VOID XBOXAPI HalRegisterShutdownNotification
(
	PHAL_SHUTDOWN_REGISTRATION ShutdownRegistration,
	BOOLEAN Register
)
{
	KIRQL OldIrql = KeRaiseIrqlToDpcLevel();

	if (Register) {
		PLIST_ENTRY ListEntry = ShutdownRoutineList.Flink;
		while (ListEntry != &ShutdownRoutineList) {
			if (ShutdownRegistration->Priority > CONTAINING_RECORD(ListEntry, HAL_SHUTDOWN_REGISTRATION, ListEntry)->Priority) {
				InsertTailList(ListEntry, &ShutdownRegistration->ListEntry);
				break;
			}
			ListEntry = ListEntry->Flink;
		}

		if (ListEntry == &ShutdownRoutineList) {
			InsertTailList(ListEntry, &ShutdownRegistration->ListEntry);
		}
	}
	else {
		PLIST_ENTRY ListEntry = ShutdownRoutineList.Flink;
		while (ListEntry != &ShutdownRoutineList) {
			if (ShutdownRegistration == CONTAINING_RECORD(ListEntry, HAL_SHUTDOWN_REGISTRATION, ListEntry)) {
				RemoveEntryList(&ShutdownRegistration->ListEntry);
				break;
			}
			ListEntry = ListEntry->Flink;
		}
	}

	KfLowerIrql(OldIrql);
}
