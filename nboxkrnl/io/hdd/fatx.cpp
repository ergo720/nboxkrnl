/*
 * ergo720                Copyright (c) 2023
 */

#include "hdd.hpp"
#include "fatx.hpp"
#include "..\..\ob\obp.hpp"
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


static NTSTATUS FatxCompleteRequest(PIRP Irp, NTSTATUS Status, PFAT_VOLUME_EXTENSION VolumeExtension)
{
	FatxUnlock();
	Irp->IoStatus.Status = Status;
	IofCompleteRequest(Irp, PRIORITY_BOOST_IO);
	return Status;
}

static BOOLEAN FatxIsNameValid(POBJECT_STRING Name)
{
	if ((Name->Length == 0) || (Name->Length > FATX_MAX_FILE_NAME_LENGTH)) { // cannot be empty or exceed 42 chars
		return FALSE;
	}

	if (Name->Buffer[0] == '.') {
		if ((Name->Length == 1) || ((Name->Length == 2) && (Name->Buffer[1] == '.'))) { // . or .. not allowed
			return FALSE;
		}
	}

	static constexpr ULONG FatxIllegalCharTable[] = {
		0xFFFFFFFF, /* [0 - 31] chars not allowed */
		0xFC009C04, /* " * / : < > ? not allowed */
		0x10000000, /* \ not allowed */
		0x10000000, /* | not allowed */
		0x00000000,
		0x00000000,
		0x00000000,
		0x00000000,
	};

	PCHAR Buffer = Name->Buffer, BufferEnd = Name->Buffer + Name->Length;
	while (Buffer < BufferEnd) {
		CHAR Char = *Buffer++;
		if (FatxIllegalCharTable[Char >> 5] & (1 << (Char & 31))) {
			return FALSE;
		}
	}

	return TRUE;
}

NTSTATUS FatxMountVolume(PDEVICE_OBJECT DeviceObject)
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
	POBJECT_STRING RemainderName = IrpStackPointer->Parameters.Create.RemainingName;

	// TODO: instead of using a global lock, try instead to use a lock specific for the volume being accessed
	FatxLock();

	PFAT_VOLUME_EXTENSION VolumeExtension = (PFAT_VOLUME_EXTENSION)DeviceObject->DeviceExtension;
	if (VolumeExtension->Flags & FATX_VOLUME_DISMOUNTED) {
		return FatxCompleteRequest(Irp, STATUS_VOLUME_DISMOUNTED, VolumeExtension);
	}

	ULONG Disposition = (IrpStackPointer->Parameters.Create.Options >> 24) & 0xFF;
	ULONG CreateOptions = IrpStackPointer->Parameters.Create.Options & 0xFFFFFF;
	if (CreateOptions & FILE_OPEN_BY_FILE_ID) { // not supported on FAT
		return FatxCompleteRequest(Irp, STATUS_NOT_IMPLEMENTED, VolumeExtension);
	}

	if (Irp->Overlay.AllocationSize.HighPart) { // 4 GiB file size limit on FAT
		return FatxCompleteRequest(Irp, STATUS_INVALID_PARAMETER, VolumeExtension);
	}

	if (RemainderName->Length == 0) {
		// Special case: open the volume itself

		if ((Disposition != FILE_OPEN) && (Disposition != FILE_OPEN_IF)) { // must be an open operation
			return FatxCompleteRequest(Irp, STATUS_ACCESS_DENIED, VolumeExtension);
		}

		if (CreateOptions & FILE_DIRECTORY_FILE) { // volume is not a directory
			return FatxCompleteRequest(Irp, STATUS_NOT_A_DIRECTORY, VolumeExtension);
		}

		if (CreateOptions & FILE_DELETE_ON_CLOSE) { // cannot delete a volume
			return FatxCompleteRequest(Irp, STATUS_CANNOT_DELETE, VolumeExtension);
		}

		if (IrpStackPointer->Flags & SL_OPEN_TARGET_DIRECTORY) { // cannot rename a volume
			return FatxCompleteRequest(Irp, STATUS_INVALID_PARAMETER, VolumeExtension);
		}

		// No host I/O required for opening a volume
		Irp->IoStatus.Information = FILE_OPENED;
		return FatxCompleteRequest(Irp, STATUS_SUCCESS, VolumeExtension);
	}
	else {
		BOOLEAN HasBackslashAtEnd = FALSE;
		if ((RemainderName->Length == 1) && (RemainderName->Buffer[0] == OB_PATH_DELIMITER)) {
			// Special case: open the root directory of the volume

			if (IrpStackPointer->Flags & SL_OPEN_TARGET_DIRECTORY) { // cannot rename the root directory
				return FatxCompleteRequest(Irp, STATUS_INVALID_PARAMETER, VolumeExtension);
			}

			HasBackslashAtEnd = TRUE;
		}
		else {
			if (RemainderName->Buffer[RemainderName->Length - 1] == OB_PATH_DELIMITER) {
				HasBackslashAtEnd = TRUE; // creating or opening a directory
			}

			OBJECT_STRING FirstName, LocalRemainderName, OriName = *RemainderName;
			while (true) {
				// Iterate until we validate all path names
				ObpParseName(&OriName, &FirstName, &LocalRemainderName);

				// NOTE: ObpParseName discards the backslash from a name, so LocalRemainderName must be checked separately
				if (LocalRemainderName.Length && (LocalRemainderName.Buffer[0] == OB_PATH_DELIMITER)) {
					// Another delimiter in the name is invalid
					return FatxCompleteRequest(Irp, STATUS_OBJECT_NAME_INVALID, VolumeExtension);
				}

				if (FatxIsNameValid(&FirstName) == FALSE) {
					return FatxCompleteRequest(Irp, STATUS_OBJECT_NAME_INVALID, VolumeExtension);
				}

				if (LocalRemainderName.Length == 0) {
					break;
				}

				OriName = LocalRemainderName;
			}
		}

		if (HasBackslashAtEnd && (CreateOptions & FILE_NON_DIRECTORY_FILE)) { // file must not be a directory
			return FatxCompleteRequest(Irp, STATUS_NOT_A_DIRECTORY, VolumeExtension);
		}
		else if (!HasBackslashAtEnd && (CreateOptions & FILE_DIRECTORY_FILE)) { // file must be a directory
			return FatxCompleteRequest(Irp, STATUS_NOT_A_DIRECTORY, VolumeExtension);
		}

		IrpStackPointer->FileObject->FsContext2 = ExAllocatePool(sizeof(IoHostFileHandle));
		if (IrpStackPointer->FileObject->FsContext2 == nullptr) {
			return FatxCompleteRequest(Irp, STATUS_INSUFFICIENT_RESOURCES, VolumeExtension);
		}

		// Finally submit the I/O request to the host to do the actual work
		// NOTE1: we cannot use the xbox handle as the host file handle, because the xbox handle is created by OB only after this I/O request succeeds. This new handle
		// should then be deleted when the file object goes away with IopCloseFile and/or IopDeleteFile
		// NOTE2: this is currently ignoring the file attributes, permissions and share access. This is because the host doesn't support these yet
		IoInfoBlock InfoBlock;
		IoRequest Packet;
		Packet.Id = InterlockedIncrement64(&IoRequestId);
		Packet.Type = (HasBackslashAtEnd ? IoFlags::IsDirectory : 0) | (CreateOptions & FILE_NON_DIRECTORY_FILE ? IoFlags::MustNotBeADirectory : 0) |
			(CreateOptions & FILE_DIRECTORY_FILE ? IoFlags::MustBeADirectory : 0) | (Disposition & 7) | IoRequestType::Open;
		Packet.HandleOrAddress = InterlockedIncrement64(&IoHostFileHandle);
		Packet.Offset = 0;
		Packet.Size = POBJECT_STRING(Irp->Tail.Overlay.DriverContext[0])->Length;
		Packet.HandleOrPath = (ULONG_PTR)(POBJECT_STRING(Irp->Tail.Overlay.DriverContext[0])->Buffer);
		SubmitIoRequestToHost(&Packet);
		RetrieveIoRequestFromHost(&InfoBlock, Packet.Id);

		switch (InfoBlock.Status)
		{
		case IoStatus::Success:
			InfoBlock.Status = (IoStatus)STATUS_SUCCESS;
			// Save the host handle in the object so that we can find it later for other requests to this same file/directory
			*(PULONGLONG)IrpStackPointer->FileObject->FsContext2 = Packet.HandleOrAddress;
			break;

		case IoStatus::Pending:
			// Should not happen right now, because RetrieveIoRequestFromHost is always synchronous
			RIP_API_MSG("Asynchronous IO is not supported");

		case IoStatus::Error:
			InfoBlock.Status = (IoStatus)STATUS_IO_DEVICE_ERROR;
			break;

		case IoStatus::Failed:
			InfoBlock.Status = (IoStatus)STATUS_ACCESS_DENIED;
			break;

		case IoStatus::IsADirectory:
			InfoBlock.Status = (IoStatus)STATUS_FILE_IS_A_DIRECTORY;
			break;

		case IoStatus::NotADirectory:
			InfoBlock.Status = (IoStatus)STATUS_NOT_A_DIRECTORY;
			break;

		case IoStatus::NotFound:
			InfoBlock.Status = (IoStatus)STATUS_OBJECT_NAME_NOT_FOUND;
			break;
		}

		Irp->IoStatus.Information = InfoBlock.Info;
		return FatxCompleteRequest(Irp, InfoBlock.Status, VolumeExtension);
	}
}
