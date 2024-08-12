/*
 * ergo720                Copyright (c) 2024
 */

#include "raw.hpp"
#include <assert.h>


static DRIVER_OBJECT RawDriverObject = {
	nullptr,                            // DriverStartIo
	nullptr,                            // DriverDeleteDevice
	nullptr,                            // DriverDismountVolume
	{
		IoInvalidDeviceRequest,         // IRP_MJ_CREATE
		IoInvalidDeviceRequest,         // IRP_MJ_CLOSE
		IoInvalidDeviceRequest,         // IRP_MJ_READ
		IoInvalidDeviceRequest,         // IRP_MJ_WRITE
		IoInvalidDeviceRequest,         // IRP_MJ_QUERY_INFORMATION
		IoInvalidDeviceRequest,         // IRP_MJ_SET_INFORMATION
		IoInvalidDeviceRequest,         // IRP_MJ_FLUSH_BUFFERS
		IoInvalidDeviceRequest       ,  // IRP_MJ_QUERY_VOLUME_INFORMATION
		IoInvalidDeviceRequest,         // IRP_MJ_DIRECTORY_CONTROL
		IoInvalidDeviceRequest,         // IRP_MJ_FILE_SYSTEM_CONTROL
		IoInvalidDeviceRequest,         // IRP_MJ_DEVICE_CONTROL
		IoInvalidDeviceRequest,         // IRP_MJ_INTERNAL_DEVICE_CONTROL
		IoInvalidDeviceRequest,         // IRP_MJ_SHUTDOWN
		IoInvalidDeviceRequest,         // IRP_MJ_CLEANUP
	}
};


static VOID RawDeleteVolume(PDEVICE_OBJECT DeviceObject)
{
	PRAW_VOLUME_EXTENSION VolumeExtension = (PRAW_VOLUME_EXTENSION)DeviceObject->DeviceExtension;

	// NOTE: we never delete the HDD TargetDeviceObject
	PDEVICE_OBJECT TargetDeviceObject = VolumeExtension->TargetDeviceObject;
	ULONG DeviceObjectHasName = DeviceObject->Flags & DO_DEVICE_HAS_NAME;
	assert(VolumeExtension->Dismounted);
	assert(VolumeExtension->TargetDeviceObject);

	// Set DeletePending flag so that new open requests in IoParseDevice will now fail
	IoDeleteDevice(DeviceObject);

	KIRQL OldIrql = KeRaiseIrqlToDpcLevel();
	TargetDeviceObject->MountedOrSelfDevice = nullptr;
	KfLowerIrql(OldIrql);

	// Dereference DeviceObject because IoCreateDevice creates the object with a ref counter biased by one. Only do this if the device doesn't have a name, because IoDeleteDevice
	// internally calls ObMakeTemporaryObject for named devices, which dereferences the DeviceObject already
	if (!DeviceObjectHasName) {
		ObfDereferenceObject(DeviceObject);
	}
}

NTSTATUS RawCreateVolume(PDEVICE_OBJECT DeviceObject)
{
	// NOTE: This is called while holding DeviceObject->DeviceLock

	switch (DeviceObject->DeviceType)
	{
	default:
	case FILE_DEVICE_MEDIA_BOARD: // TODO
		return STATUS_UNRECOGNIZED_VOLUME;

	case FILE_DEVICE_DISK:
		break;
	}

	PDEVICE_OBJECT RawDeviceObject;
	NTSTATUS Status = IoCreateDevice(&RawDriverObject, sizeof(RAW_VOLUME_EXTENSION), nullptr, FILE_DEVICE_DISK_FILE_SYSTEM, FALSE, &RawDeviceObject);
	if (!NT_SUCCESS(Status)) {
		return Status;
	}

	RawDeviceObject->StackSize = RawDeviceObject->StackSize + DeviceObject->StackSize;
	RawDeviceObject->SectorSize = DeviceObject->SectorSize;

	if (RawDeviceObject->AlignmentRequirement < DeviceObject->AlignmentRequirement) {
		RawDeviceObject->AlignmentRequirement = DeviceObject->AlignmentRequirement;
	}

	if (DeviceObject->Flags & DO_SCATTER_GATHER_IO) {
		RawDeviceObject->Flags |= DO_SCATTER_GATHER_IO;
	}

	if (DeviceObject->Flags & DO_DIRECT_IO) {
		RawDeviceObject->Flags |= DO_DIRECT_IO;
	}

	PRAW_VOLUME_EXTENSION VolumeExtension = (PRAW_VOLUME_EXTENSION)RawDeviceObject->DeviceExtension;

	VolumeExtension->TargetDeviceObject = DeviceObject;
	ExInitializeReadWriteLock(&VolumeExtension->VolumeMutex);

	RawDeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

	DeviceObject->MountedOrSelfDevice = RawDeviceObject;

	return STATUS_SUCCESS;
}
