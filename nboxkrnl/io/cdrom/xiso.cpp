/*
 * ergo720                Copyright (c) 2024
 */

#include "cdrom.hpp"
#include "xiso.hpp"


static DRIVER_OBJECT XisoDriverObject = {
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
		IoInvalidDeviceRequest,         // IRP_MJ_QUERY_VOLUME_INFORMATION
		IoInvalidDeviceRequest,         // IRP_MJ_DIRECTORY_CONTROL
		IoInvalidDeviceRequest,         // IRP_MJ_FILE_SYSTEM_CONTROL
		IoInvalidDeviceRequest,         // IRP_MJ_DEVICE_CONTROL
		IoInvalidDeviceRequest,         // IRP_MJ_INTERNAL_DEVICE_CONTROL
		IoInvalidDeviceRequest,         // IRP_MJ_SHUTDOWN
		IoInvalidDeviceRequest,         // IRP_MJ_CLEANUP
	}
};

NTSTATUS XisoCreateVolume(PDEVICE_OBJECT DeviceObject)
{
	DISK_GEOMETRY DiskGeometry;
	DiskGeometry.Cylinders.QuadPart = CDROM_TOTAL_NUM_OF_SECTORS;
	DiskGeometry.MediaType = RemovableMedia;
	DiskGeometry.TracksPerCylinder = 1;
	DiskGeometry.SectorsPerTrack = 1;
	DiskGeometry.BytesPerSector = CDROM_SECTOR_SIZE;

	PDEVICE_OBJECT XisoDeviceObject;
	NTSTATUS Status = IoCreateDevice(&XisoDriverObject, sizeof(XISO_VOLUME_EXTENSION), nullptr, FILE_DEVICE_CD_ROM_FILE_SYSTEM, FALSE, &XisoDeviceObject);
	if (!NT_SUCCESS(Status)) {
		return Status;
	}

	XisoDeviceObject->StackSize = XisoDeviceObject->StackSize + DeviceObject->StackSize;
	XisoDeviceObject->SectorSize = DiskGeometry.BytesPerSector;

	if (XisoDeviceObject->AlignmentRequirement < DeviceObject->AlignmentRequirement) {
		XisoDeviceObject->AlignmentRequirement = DeviceObject->AlignmentRequirement;
	}

	if (DeviceObject->Flags & DO_SCATTER_GATHER_IO) {
		XisoDeviceObject->Flags |= DO_SCATTER_GATHER_IO;
	}

	PXISO_VOLUME_EXTENSION VolumeExtension = (PXISO_VOLUME_EXTENSION)XisoDeviceObject->DeviceExtension;

	VolumeExtension->TargetDeviceObject = DeviceObject;
	VolumeExtension->SectorSize = DiskGeometry.BytesPerSector;
	VolumeExtension->PartitionSectorCount = DiskGeometry.Cylinders.LowPart;
	VolumeExtension->PartitionLength.QuadPart = (ULONGLONG)DiskGeometry.Cylinders.LowPart << 11;

	XisoDeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

	DeviceObject->MountedOrSelfDevice = XisoDeviceObject;

	return STATUS_SUCCESS;
}
