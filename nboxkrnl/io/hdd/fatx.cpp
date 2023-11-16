/*
 * ergo720                Copyright (c) 2023
 */

#include "hdd.hpp"
#include "fatx.hpp"
#include "..\..\rtl\rtl.hpp"

#define FatxLock() RtlEnterCriticalSectionAndRegion(&FatxCriticalRegion)
#define FatxUnlock()RtlLeaveCriticalSectionAndRegion(&FatxCriticalRegion)


NTSTATUS XBOXAPI FatxCreate(PDEVICE_OBJECT DeviceObject, PIRP Irp);

static DRIVER_OBJECT FatxDriverObject = {
	nullptr,                            // DriverStartIo
	nullptr,                            // DriverDeleteDevice
	nullptr,                            // DriverDismountVolume
	{
		FatxCreate,                     // IRP_MJ_CREATE
		IoInvalidDeviceRequest,         // IRP_MJ_CLOSE
		IoInvalidDeviceRequest,         // IRP_MJ_READ
		IoInvalidDeviceRequest,         // IRP_MJ_WRITE
		IoInvalidDeviceRequest,         // IRP_MJ_QUERY_INFORMATION
		IoInvalidDeviceRequest,         // IRP_MJ_SET_INFORMATION
		IoInvalidDeviceRequest,         // IRP_MJ_FLUSH_BUFFERS
		IoInvalidDeviceRequest,         // IRP_MJ_QUERY_VOLUME_INFORMATION
		IoInvalidDeviceRequest,         // IRP_MJ_DIRECTORY_CONTROL
		IoInvalidDeviceRequest,         // IRP_MJ_FILE_SYSTEM_CONTROL
		IoInvalidDeviceRequest,         // IRP_MJ_DEVICE_CONTROL
		IoInvalidDeviceRequest,         // IRP_MJ_INTERNAL_DEVICE_CONTROL
		IoInvalidDeviceRequest,         // IRP_MJ_SHUTDOWN
		IoInvalidDeviceRequest,         // IRP_MJ_CLEANUP
	}
};

static INITIALIZE_GLOBAL_CRITICAL_SECTION(FatxCriticalRegion);


NTSTATUS MountFatxVolume(PDEVICE_OBJECT DeviceObject)
{
	// NOTE: This is called while holding DeviceObject->DeviceLock

	DISK_GEOMETRY DiskGeometry;
	
	switch (DeviceObject->DeviceType)
	{
	default:
	case FILE_DEVICE_MEMORY_UNIT: // TODO
	case FILE_DEVICE_MEDIA_BOARD: // TODO
		return STATUS_UNRECOGNIZED_VOLUME;

	case FILE_DEVICE_DISK:
		// Fixed HDD of 8 GiB
		DiskGeometry.Cylinders.QuadPart = HDD_TOTAL_NUM_OF_SECTORS;
		DiskGeometry.MediaType = FixedMedia;
		DiskGeometry.TracksPerCylinder = 1;
		DiskGeometry.SectorsPerTrack = 1;
		DiskGeometry.BytesPerSector = HDD_SECTOR_SIZE;
		break;
	}

	FatxLock();

	PDEVICE_OBJECT FatxDeviceObject;
	NTSTATUS Status = IoCreateDevice(&FatxDriverObject, sizeof(FAT_VOLUME_EXTENSION), nullptr, FILE_DEVICE_DISK_FILE_SYSTEM, FALSE, &FatxDeviceObject);
	if (!NT_SUCCESS(Status)) {
		return Status;
	}

	FatxDeviceObject->StackSize = FatxDeviceObject->StackSize + DeviceObject->StackSize;
	FatxDeviceObject->SectorSize = DiskGeometry.BytesPerSector;

	if (FatxDeviceObject->AlignmentRequirement < DeviceObject->AlignmentRequirement) {
		FatxDeviceObject->AlignmentRequirement = DeviceObject->AlignmentRequirement;
	}

	if (DeviceObject->Flags & DO_SCATTER_GATHER_IO) {
		FatxDeviceObject->Flags |= DO_SCATTER_GATHER_IO;
	}

	FatxDeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

	DeviceObject->MountedOrSelfDevice = FatxDeviceObject;

	FatxUnlock();

	return STATUS_SUCCESS;
}

NTSTATUS XBOXAPI FatxCreate(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	PIO_STACK_LOCATION IrpStackPointer = IoGetCurrentIrpStackLocation(Irp);
	OBJECT_STRING RemainderName = *IrpStackPointer->Parameters.Create.RemainingName;

	// TODO: instead of using a global lock, try instead to use a lock specific for the volume being accessed
	FatxLock();

	PFAT_VOLUME_EXTENSION VolumeExtension = (PFAT_VOLUME_EXTENSION)DeviceObject->DeviceExtension;
	if (VolumeExtension->Flags & FATX_VOLUME_DISMOUNTED) {
		FatxUnlock();
		return STATUS_VOLUME_DISMOUNTED;
	}

	RIP_API_MSG("incomplete");
}
