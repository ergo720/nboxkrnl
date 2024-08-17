/*
 * ergo720                Copyright (c) 2023
 */

#pragma once

#include "..\types.hpp"
#include "ke.hpp"


using PHAL_SHUTDOWN_NOTIFICATION = VOID(XBOXAPI *)(
	struct HAL_SHUTDOWN_REGISTRATION *ShutdownRegistration
	);

struct HAL_SHUTDOWN_REGISTRATION {
	PHAL_SHUTDOWN_NOTIFICATION NotificationRoutine;
	LONG Priority;
	LIST_ENTRY ListEntry;
};
using PHAL_SHUTDOWN_REGISTRATION = HAL_SHUTDOWN_REGISTRATION *;

#ifdef __cplusplus
extern "C" {
#endif

EXPORTNUM(40) DLLEXPORT extern ULONG HalDiskCachePartitionCount;

EXPORTNUM(43) DLLEXPORT VOID XBOXAPI HalEnableSystemInterrupt
(
	ULONG BusInterruptLevel,
	KINTERRUPT_MODE InterruptMode
);

EXPORTNUM(44) DLLEXPORT ULONG XBOXAPI HalGetInterruptVector
(
	ULONG BusInterruptLevel,
	PKIRQL Irql
);

EXPORTNUM(45) DLLEXPORT NTSTATUS HalReadSMBusValue
(
	UCHAR SlaveAddress,
	UCHAR CommandCode,
	BOOLEAN ReadWordValue,
	ULONG *DataValue
);

EXPORTNUM(46) DLLEXPORT VOID XBOXAPI HalReadWritePCISpace
(
	ULONG BusNumber,
	ULONG SlotNumber,
	ULONG RegisterNumber,
	PVOID Buffer,
	ULONG Length,
	BOOLEAN WritePCISpace
);

EXPORTNUM(47) DLLEXPORT VOID XBOXAPI HalRegisterShutdownNotification
(
	PHAL_SHUTDOWN_REGISTRATION ShutdownRegistration,
	BOOLEAN Register
);

EXPORTNUM(48) DLLEXPORT VOID FASTCALL HalRequestSoftwareInterrupt
(
	KIRQL Request
);

EXPORTNUM(50) DLLEXPORT NTSTATUS XBOXAPI HalWriteSMBusValue
(
	UCHAR SlaveAddress,
	UCHAR CommandCode,
	BOOLEAN WriteWordValue,
	ULONG DataValue
);

#ifdef __cplusplus
}
#endif

VOID HalInitSystem();
