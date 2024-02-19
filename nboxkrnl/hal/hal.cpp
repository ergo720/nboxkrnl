/*
 * ergo720                Copyright (c) 2023
 */

#include "hal.hpp"
#include "halp.hpp"
#include "..\rtl\rtl.hpp"


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

	if (XboxType == SYSTEM_DEVKIT) {
		XboxHardwareInfo.Flags |= 2;
	}
	else if (XboxType == SYSTEM_CHIHIRO) {
		XboxHardwareInfo.Flags |= 8;
	}

	// TODO: this should also setup the SMC
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
