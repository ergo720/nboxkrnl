/*
 * ergo720                Copyright (c) 2024
 */

#include "cdrom.hpp"
#include "..\io.hpp"
#include "..\..\ob\obp.hpp"


static DRIVER_OBJECT CdromDriverObject = {
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

PDEVICE_OBJECT CdromDeviceObject;


BOOLEAN CdromInitDriver()
{
	// This will also create an object of type IoDeviceObjectType with the name "CdRom0". This is used by OB to resolve paths to the CDROM device

	INITIALIZE_GLOBAL_OBJECT_STRING(CdromDirectoryName, "\\Device\\CdRom0");
	INITIALIZE_GLOBAL_OBJECT_STRING(CdromDosDirectoryName, "\\??\\CdRom0:");

	NTSTATUS Status = IoCreateDevice(&CdromDriverObject, 0, &CdromDirectoryName, FILE_DEVICE_CD_ROM, FALSE, &CdromDeviceObject);
	if (!NT_SUCCESS(Status)) {
		return FALSE;
	}

	Status = IoCreateSymbolicLink(&CdromDosDirectoryName, &CdromDirectoryName);
	if (!NT_SUCCESS(Status)) {
		return FALSE;
	}

	CdromDeviceObject->Flags |= (DO_DIRECT_IO | DO_SCATTER_GATHER_IO);
	CdromDeviceObject->AlignmentRequirement = CDROM_ALIGNMENT_REQUIREMENT;
	CdromDeviceObject->SectorSize = CDROM_SECTOR_SIZE;
	CdromDeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

	return TRUE;
}
