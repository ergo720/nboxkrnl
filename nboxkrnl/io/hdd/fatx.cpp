/*
 * ergo720                Copyright (c) 2023
 */

#include "hdd.hpp"
#include "..\..\io\fsc.hpp"
#include "..\..\ob\obp.hpp"
#include "..\..\rtl\rtl.hpp"
#include "..\..\dbg\dbg.hpp"
#include <string.h>


#define FATX_NAME_LENGTH 32
#define FATX_ONLINE_DATA_LENGTH 2048
#define FATX_RESERVED_LENGTH 1968
#define FATX_SIGNATURE 'XTAF'

#pragma pack(1)
struct FATX_SUPERBLOCK {
	ULONG Signature;
	ULONG VolumeID;
	ULONG ClusterSize;
	ULONG RootDirCluster;
	WCHAR Name[FATX_NAME_LENGTH];
	UCHAR OnlineData[FATX_ONLINE_DATA_LENGTH];
	UCHAR Unused[FATX_RESERVED_LENGTH];
};
using PFATX_SUPERBLOCK = FATX_SUPERBLOCK *;
#pragma pack()

NTSTATUS XBOXAPI FatxIrpCreate(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS XBOXAPI FatxIrpQueryVolumeInformation(PDEVICE_OBJECT DeviceObject, PIRP Irp);

static DRIVER_OBJECT FatxDriverObject = {
	nullptr,                            // DriverStartIo
	nullptr,                            // DriverDeleteDevice
	nullptr,                            // DriverDismountVolume
	{
		FatxIrpCreate,                  // IRP_MJ_CREATE
		IoInvalidDeviceRequest,         // IRP_MJ_CLOSE
		IoInvalidDeviceRequest,         // IRP_MJ_READ
		IoInvalidDeviceRequest,         // IRP_MJ_WRITE
		IoInvalidDeviceRequest,         // IRP_MJ_QUERY_INFORMATION
		IoInvalidDeviceRequest,         // IRP_MJ_SET_INFORMATION
		IoInvalidDeviceRequest,         // IRP_MJ_FLUSH_BUFFERS
		FatxIrpQueryVolumeInformation,  // IRP_MJ_QUERY_VOLUME_INFORMATION
		IoInvalidDeviceRequest,         // IRP_MJ_DIRECTORY_CONTROL
		IoInvalidDeviceRequest,         // IRP_MJ_FILE_SYSTEM_CONTROL
		IoInvalidDeviceRequest,         // IRP_MJ_DEVICE_CONTROL
		IoInvalidDeviceRequest,         // IRP_MJ_INTERNAL_DEVICE_CONTROL
		IoInvalidDeviceRequest,         // IRP_MJ_SHUTDOWN
		IoInvalidDeviceRequest,         // IRP_MJ_CLEANUP
	}
};


static VOID FatxVolumeLockExclusive(PFAT_VOLUME_EXTENSION VolumeExtension)
{
	KeEnterCriticalRegion();
	ExAcquireReadWriteLockExclusive(&VolumeExtension->VolumeMutex);
}

static VOID FatxVolumeLockShared(PFAT_VOLUME_EXTENSION VolumeExtension)
{
	KeEnterCriticalRegion();
	ExAcquireReadWriteLockShared(&VolumeExtension->VolumeMutex);
}

static VOID FatxVolumeUnlock(PFAT_VOLUME_EXTENSION VolumeExtension)
{
	ExReleaseReadWriteLock(&VolumeExtension->VolumeMutex);
	KeLeaveCriticalRegion();
}

static NTSTATUS FatxCompleteRequest(PIRP Irp, NTSTATUS Status, PFAT_VOLUME_EXTENSION VolumeExtension)
{
	FatxVolumeUnlock(VolumeExtension);
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

static NTSTATUS FatxReadSuperblock(PFAT_VOLUME_EXTENSION VolumeExtension, LONGLONG PartitionLength)
{
	PFATX_SUPERBLOCK Superblock;
	if (NTSTATUS Status = FscMapElementPage(&VolumeExtension->CacheExtension, 0, (PVOID *)&Superblock, FALSE); !NT_SUCCESS(Status)) {
		return Status;
	}

	// If we fail to validate the superblock, we log the error because it probably means that the partition.bin file on the host side is corrupted
	if (Superblock->Signature != FATX_SIGNATURE) {
		DbgPrint("Superblock has an invalid signature");
		FscUnmapElementPage(Superblock);
		return STATUS_UNRECOGNIZED_VOLUME;
	}

	switch (Superblock->ClusterSize)
	{
	case 1:
	case 2:
	case 4:
	case 8:
	case 16:
	case 32:
	case 64:
	case 128:
		break;

	default:
		DbgPrint("Superblock has an invalid cluster size (it was %u)", Superblock->ClusterSize);
		FscUnmapElementPage(Superblock);
		return STATUS_UNRECOGNIZED_VOLUME;
	}

	VolumeExtension->BytesPerCluster = Superblock->ClusterSize << VolumeExtension->SectorShift;
	VolumeExtension->ClusterShift = RtlpBitScanForward(VolumeExtension->BytesPerCluster);
	VolumeExtension->VolumeID = Superblock->VolumeID;
	VolumeExtension->NumberOfClusters = ULONG((PartitionLength - sizeof(FATX_SUPERBLOCK)) >> VolumeExtension->ClusterShift);
	VolumeExtension->NumberOfClustersAvailable = VolumeExtension->NumberOfClusters; // FIXME: hardcoded number of free clusters

	FscUnmapElementPage(Superblock);

	return STATUS_SUCCESS;
}

NTSTATUS FatxCreateVolume(PDEVICE_OBJECT DeviceObject)
{
	// NOTE: This is called while holding DeviceObject->DeviceLock

	DISK_GEOMETRY DiskGeometry;
	LONGLONG PartitionLength;
	
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
		PartitionLength = PIDE_DISK_EXTENSION(DeviceObject->DeviceExtension)->PartitionInformation.PartitionLength.QuadPart;
		break;
	}

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

	PFAT_VOLUME_EXTENSION VolumeExtension = (PFAT_VOLUME_EXTENSION)FatxDeviceObject->DeviceExtension;
	VolumeExtension->CacheExtension.TargetDeviceObject = DeviceObject;
	VolumeExtension->CacheExtension.SectorSize = FatxDeviceObject->SectorSize;
	VolumeExtension->SectorShift = RtlpBitScanForward(DiskGeometry.BytesPerSector);
	ExInitializeReadWriteLock(&VolumeExtension->VolumeMutex);

	if (Status = FatxReadSuperblock(VolumeExtension, PartitionLength); !NT_SUCCESS(Status)) {
		// TODO: cleanup FatxDeviceObject when this fails
		return Status;
	}

	FatxDeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

	DeviceObject->MountedOrSelfDevice = FatxDeviceObject;

	return STATUS_SUCCESS;
}

NTSTATUS XBOXAPI FatxIrpCreate(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	PIO_STACK_LOCATION IrpStackPointer = IoGetCurrentIrpStackLocation(Irp);
	POBJECT_STRING RemainingName = IrpStackPointer->Parameters.Create.RemainingName;

	PFAT_VOLUME_EXTENSION VolumeExtension = (PFAT_VOLUME_EXTENSION)DeviceObject->DeviceExtension;
	FatxVolumeLockExclusive(VolumeExtension);

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

	if (RemainingName->Length == 0) {
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
		if ((RemainingName->Length == 1) && (RemainingName->Buffer[0] == OB_PATH_DELIMITER)) {
			// Special case: open the root directory of the volume

			if (IrpStackPointer->Flags & SL_OPEN_TARGET_DIRECTORY) { // cannot rename the root directory
				return FatxCompleteRequest(Irp, STATUS_INVALID_PARAMETER, VolumeExtension);
			}

			HasBackslashAtEnd = TRUE;
		}
		else {
			if (RemainingName->Buffer[RemainingName->Length - 1] == OB_PATH_DELIMITER) {
				HasBackslashAtEnd = TRUE; // creating or opening a directory
			}

			OBJECT_STRING FirstName, LocalRemainingName, OriName = *RemainingName;
			while (true) {
				// Iterate until we validate all path names
				ObpParseName(&OriName, &FirstName, &LocalRemainingName);

				// NOTE: ObpParseName discards the backslash from a name, so LocalRemainingName must be checked separately
				if (LocalRemainingName.Length && (LocalRemainingName.Buffer[0] == OB_PATH_DELIMITER)) {
					// Another delimiter in the name is invalid
					return FatxCompleteRequest(Irp, STATUS_OBJECT_NAME_INVALID, VolumeExtension);
				}

				if (FatxIsNameValid(&FirstName) == FALSE) {
					return FatxCompleteRequest(Irp, STATUS_OBJECT_NAME_INVALID, VolumeExtension);
				}

				if (LocalRemainingName.Length == 0) {
					break;
				}

				OriName = LocalRemainingName;
			}
		}

		if (HasBackslashAtEnd && (CreateOptions & FILE_NON_DIRECTORY_FILE)) { // file must not be a directory
			return FatxCompleteRequest(Irp, STATUS_FILE_IS_A_DIRECTORY, VolumeExtension);
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
		ULONGLONG HostHandle = InterlockedIncrement64(&IoHostFileHandle);
		IoInfoBlock InfoBlock = SubmitIoRequestToHost(
			(HasBackslashAtEnd ? IoFlags::IsDirectory : 0) | (CreateOptions & FILE_NON_DIRECTORY_FILE ? IoFlags::MustNotBeADirectory : 0) |
			(CreateOptions & FILE_DIRECTORY_FILE ? IoFlags::MustBeADirectory : 0) | (Disposition & 7) | IoRequestType::Open,
			0,
			POBJECT_STRING(Irp->Tail.Overlay.DriverContext[0])->Length,
			HostHandle,
			(ULONG_PTR)(POBJECT_STRING(Irp->Tail.Overlay.DriverContext[0])->Buffer)
		);

		NTSTATUS Status = HostToNtStatus(InfoBlock.Status);
		if (Status == STATUS_SUCCESS) {
			// Save the host handle in the object so that we can find it later for other requests to this same file/directory
			*(PULONGLONG)IrpStackPointer->FileObject->FsContext2 = HostHandle;
		}
		else if (Status == STATUS_PENDING) {
			// Should not happen right now, because RetrieveIoRequestFromHost is always synchronous
			RIP_API_MSG("Asynchronous IO is not supported");
		}

		Irp->IoStatus.Information = InfoBlock.Info;
		return FatxCompleteRequest(Irp, InfoBlock.Status, VolumeExtension);
	}
}

NTSTATUS XBOXAPI FatxIrpQueryVolumeInformation(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	PFAT_VOLUME_EXTENSION VolumeExtension = (PFAT_VOLUME_EXTENSION)DeviceObject->DeviceExtension;
	PIO_STACK_LOCATION IrpStackPointer = IoGetCurrentIrpStackLocation(Irp);

	FatxVolumeLockShared(VolumeExtension);

	if (VolumeExtension->Flags & FATX_VOLUME_DISMOUNTED) {
		return FatxCompleteRequest(Irp, STATUS_VOLUME_DISMOUNTED, VolumeExtension);
	}

	memset(Irp->UserBuffer, 0, IrpStackPointer->Parameters.QueryVolume.Length);

	ULONG BytesWritten;
	NTSTATUS Status = STATUS_SUCCESS;
	switch (IrpStackPointer->Parameters.QueryVolume.FsInformationClass)
	{
	case FileFsVolumeInformation:
		PFILE_FS_VOLUME_INFORMATION(Irp->UserBuffer)->VolumeSerialNumber = VolumeExtension->VolumeID;
		BytesWritten = offsetof(FILE_FS_VOLUME_INFORMATION, VolumeLabel);
		break;

	case FileFsSizeInformation: {
		PFILE_FS_SIZE_INFORMATION SizeInformation = (PFILE_FS_SIZE_INFORMATION)Irp->UserBuffer;
		SizeInformation->TotalAllocationUnits.QuadPart = VolumeExtension->NumberOfClusters;
		SizeInformation->AvailableAllocationUnits.QuadPart = VolumeExtension->NumberOfClustersAvailable;
		SizeInformation->SectorsPerAllocationUnit = VolumeExtension->BytesPerCluster >> VolumeExtension->SectorShift;
		SizeInformation->BytesPerSector = VolumeExtension->CacheExtension.SectorSize;
		BytesWritten = sizeof(FILE_FS_SIZE_INFORMATION);
	}
	break;

	case FileFsDeviceInformation: {
		PFILE_FS_DEVICE_INFORMATION DeviceInformation = (PFILE_FS_DEVICE_INFORMATION)Irp->UserBuffer;
		DeviceInformation->DeviceType = VolumeExtension->CacheExtension.TargetDeviceObject->DeviceType;
		DeviceInformation->Characteristics = 0;
		BytesWritten = sizeof(FILE_FS_DEVICE_INFORMATION);
	}
	break;

	case FileFsAttributeInformation: {
		PFILE_FS_ATTRIBUTE_INFORMATION AttributeInformation = (PFILE_FS_ATTRIBUTE_INFORMATION)Irp->UserBuffer;
		AttributeInformation->FileSystemAttributes = 0;
		AttributeInformation->MaximumComponentNameLength = FATX_MAX_FILE_NAME_LENGTH;
		AttributeInformation->FileSystemNameLength = 4; // strlen("FATX")

		// NOTE: this writes the ANSI string "FATX" to FileSystemName if the buffer is large enough. The string is written as if ANSI, and not UNICODE like the WCHAR type
		// of FileSystemName might suggest. This was confirmed by looking at this code from a dump of an original kernel
		const ULONG OffsetOfFileSystemName = offsetof(FILE_FS_ATTRIBUTE_INFORMATION, FileSystemName);
		if (IrpStackPointer->Parameters.QueryVolume.Length < (OffsetOfFileSystemName + 4)) {
			BytesWritten = OffsetOfFileSystemName;
			Status = STATUS_BUFFER_OVERFLOW;
		}
		else {
			strncpy((PCHAR)&AttributeInformation->FileSystemName, "FATX", 4);
			BytesWritten = OffsetOfFileSystemName + 4;
		}
	}
	break;

	default:
		BytesWritten = 0;
		Status = STATUS_INVALID_PARAMETER;
	}

	Irp->IoStatus.Information = BytesWritten;
	return FatxCompleteRequest(Irp, Status, VolumeExtension);
}
