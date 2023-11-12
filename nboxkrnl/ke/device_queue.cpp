/*
 * Luca1991              Copyright (c) 2017
 */

#include "ke.hpp"
#include "..\kernel.hpp"


EXPORTNUM(106) VOID XBOXAPI KeInitializeDeviceQueue
(
	PKDEVICE_QUEUE DeviceQueue
)
{
	DeviceQueue->Type = DeviceQueueObject;
	DeviceQueue->Size = sizeof(KDEVICE_QUEUE);
	DeviceQueue->Busy = FALSE;
	InitializeListHead(&DeviceQueue->DeviceListHead);
}
