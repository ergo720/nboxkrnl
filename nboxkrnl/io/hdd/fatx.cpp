/*
 * ergo720                Copyright (c) 2023
 */

#include "hdd.hpp"
#include "fsc.hpp"
#include "obp.hpp"
#include "dbg.hpp"
#include <string.h>
#include <assert.h>


#define FATX_PATH_NAME_LENGTH 250
#define FATX_NAME_LENGTH 32
#define FATX_ONLINE_DATA_LENGTH 2048
#define FATX_RESERVED_LENGTH 1968
#define FATX_SIGNATURE 'XTAF'
#define FATX_CLUSTER_FREE 0

#define FATX_TIMESTAMP_SECONDS2_SHIFT 0
#define FATX_TIMESTAMP_MINUTES_SHIFT  5
#define FATX_TIMESTAMP_HOURS_SHIFT    11
#define FATX_TIMESTAMP_DAY_SHIFT      16
#define FATX_TIMESTAMP_MONTH_SHIFT    21
#define FATX_TIMESTAMP_YEAR_SHIFT     25
#define FATX_TIMESTAMP_SECONDS2_MASK  0x1F
#define FATX_TIMESTAMP_MINUTES_MASK   0x3F
#define FATX_TIMESTAMP_HOURS_MASK     0x1F
#define FATX_TIMESTAMP_DAY_MASK       0x1F
#define FATX_TIMESTAMP_MONTH_MASK     0x0F
#define FATX_TIMESTAMP_YEAR_MASK      0x7F

#define FATX_DIRECTORY_FILE             FILE_DIRECTORY_FILE // = 0x00000001
#define FATX_DELETE_ON_CLOSE            FILE_DELETE_ON_CLOSE // = 0x00001000
#define FATX_VOLUME_FILE                0x80000000
#define FATX_VALID_FILE_ATTRIBUES       (FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_ARCHIVE)
// File permission check masks, used by nxbx and here only for documentation
#define FATX_VALID_DIRECTORY_ACCESS     (DELETE | READ_CONTROL | WRITE_OWNER | \
										WRITE_DAC | SYNCHRONIZE | ACCESS_SYSTEM_SECURITY | FILE_WRITE_DATA | \
										FILE_READ_EA | FILE_WRITE_EA | FILE_READ_ATTRIBUTES | \
										FILE_WRITE_ATTRIBUTES | FILE_LIST_DIRECTORY | FILE_TRAVERSE | \
										FILE_DELETE_CHILD | FILE_APPEND_DATA)
#define FATX_VALID_FILE_ACCESS          (DELETE | READ_CONTROL | WRITE_OWNER | \
										WRITE_DAC | SYNCHRONIZE | ACCESS_SYSTEM_SECURITY | FILE_READ_DATA | \
										FILE_WRITE_DATA | FILE_READ_EA | FILE_WRITE_EA | FILE_READ_ATTRIBUTES | \
										FILE_WRITE_ATTRIBUTES | FILE_EXECUTE | FILE_DELETE_CHILD | \
										FILE_APPEND_DATA)
#define FATX_ACCESS_IMPLIES_WRITE       (DELETE | READ_CONTROL | WRITE_OWNER | \
										WRITE_DAC | SYNCHRONIZE | ACCESS_SYSTEM_SECURITY | FILE_READ_DATA | \
										FILE_READ_EA | FILE_WRITE_EA | FILE_READ_ATTRIBUTES | \
										FILE_WRITE_ATTRIBUTES | FILE_EXECUTE | FILE_LIST_DIRECTORY | \
										FILE_TRAVERSE)
#define FATX_CREATE_IMPLIES_WRITE       FILE_DELETE_ON_CLOSE


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

static_assert(sizeof(FATX_SUPERBLOCK) == PAGE_SIZE);

static NTSTATUS XBOXAPI FatxIrpCreate(PDEVICE_OBJECT DeviceObject, PIRP Irp);
static NTSTATUS XBOXAPI FatxIrpClose(PDEVICE_OBJECT DeviceObject, PIRP Irp);
static NTSTATUS XBOXAPI FatxIrpRead(PDEVICE_OBJECT DeviceObject, PIRP Irp);
static NTSTATUS XBOXAPI FatxIrpWrite(PDEVICE_OBJECT DeviceObject, PIRP Irp);
static NTSTATUS XBOXAPI FatxIrpQueryInformation(PDEVICE_OBJECT DeviceObject, PIRP Irp);
static NTSTATUS XBOXAPI FatxIrpQueryVolumeInformation(PDEVICE_OBJECT DeviceObject, PIRP Irp);
static NTSTATUS XBOXAPI FatxIrpDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp);
static NTSTATUS XBOXAPI FatxIrpCleanup(PDEVICE_OBJECT DeviceObject, PIRP Irp);

static DRIVER_OBJECT FatxDriverObject = {
	nullptr,                            // DriverStartIo
	nullptr,                            // DriverDeleteDevice
	nullptr,                            // DriverDismountVolume
	{
		FatxIrpCreate,                  // IRP_MJ_CREATE
		FatxIrpClose,                   // IRP_MJ_CLOSE
		FatxIrpRead,                    // IRP_MJ_READ
		FatxIrpWrite,                   // IRP_MJ_WRITE
		FatxIrpQueryInformation,        // IRP_MJ_QUERY_INFORMATION
		IoInvalidDeviceRequest,         // IRP_MJ_SET_INFORMATION
		IoInvalidDeviceRequest,         // IRP_MJ_FLUSH_BUFFERS
		FatxIrpQueryVolumeInformation,  // IRP_MJ_QUERY_VOLUME_INFORMATION
		IoInvalidDeviceRequest,         // IRP_MJ_DIRECTORY_CONTROL
		IoInvalidDeviceRequest,         // IRP_MJ_FILE_SYSTEM_CONTROL
		FatxIrpDeviceControl,           // IRP_MJ_DEVICE_CONTROL
		IoInvalidDeviceRequest,         // IRP_MJ_INTERNAL_DEVICE_CONTROL
		IoInvalidDeviceRequest,         // IRP_MJ_SHUTDOWN
		FatxIrpCleanup,                 // IRP_MJ_CLEANUP
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

static BOOLEAN FatxPackTimestamp(PLARGE_INTEGER Time, PULONG FatxTimestamp)
{
	LARGE_INTEGER LocalTime{ .QuadPart = Time->QuadPart + (2 * 1000 * 1000 * 10 - 1) };
	TIME_FIELDS TimeField;
	RtlTimeToTimeFields(&LocalTime, &TimeField);

	*FatxTimestamp = ((ULONG)TimeField.Year - 2000) << FATX_TIMESTAMP_YEAR_SHIFT; // fatx tracks the year from 2000
	*FatxTimestamp |= ((ULONG)TimeField.Month << FATX_TIMESTAMP_MONTH_SHIFT);
	*FatxTimestamp |= ((ULONG)TimeField.Day << FATX_TIMESTAMP_DAY_SHIFT);
	*FatxTimestamp |= ((ULONG)TimeField.Hour << FATX_TIMESTAMP_HOURS_SHIFT);
	*FatxTimestamp |= ((ULONG)TimeField.Minute << FATX_TIMESTAMP_MINUTES_SHIFT);
	*FatxTimestamp |= ((ULONG)(TimeField.Second >> 1) << FATX_TIMESTAMP_SECONDS2_SHIFT);

	return (TimeField.Year >= 2000) && (TimeField.Year <= 2000 + 127); // the years will overflow after 2127 because it's 7 bits wide
}

static LARGE_INTEGER FatxUnpackTimestamp(ULONG FatxTimestamp)
{
	TIME_FIELDS TimeField;

	TimeField.Year = ((FatxTimestamp >> FATX_TIMESTAMP_YEAR_SHIFT) & FATX_TIMESTAMP_YEAR_MASK) + 2000; // fatx tracks the year from 2000
	TimeField.Month = (FatxTimestamp >> FATX_TIMESTAMP_MONTH_SHIFT) & FATX_TIMESTAMP_MONTH_MASK;
	TimeField.Day = (FatxTimestamp >> FATX_TIMESTAMP_DAY_SHIFT) & FATX_TIMESTAMP_DAY_MASK;
	TimeField.Hour = (FatxTimestamp >> FATX_TIMESTAMP_HOURS_SHIFT) & FATX_TIMESTAMP_HOURS_MASK;
	TimeField.Minute = (FatxTimestamp >> FATX_TIMESTAMP_MINUTES_SHIFT) & FATX_TIMESTAMP_MINUTES_MASK;
	TimeField.Second = ((FatxTimestamp >> FATX_TIMESTAMP_SECONDS2_SHIFT) & FATX_TIMESTAMP_SECONDS2_MASK) * 2;
	TimeField.Millisecond = 0;

	LARGE_INTEGER LocalTime;
	if (!RtlTimeFieldsToTime(&TimeField, &LocalTime)) {
		LocalTime.QuadPart = 0;
	}

	return LocalTime;
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

static PFATX_FILE_INFO FatxFindOpenFile(PFAT_VOLUME_EXTENSION VolumeExtension, POBJECT_STRING Name)
{
	PFATX_FILE_INFO FileInfo = nullptr;
	PLIST_ENTRY Entry = VolumeExtension->OpenFileList.Blink;
	while (Entry != &VolumeExtension->OpenFileList) {
		PFATX_FILE_INFO CurrFileInfo = CONTAINING_RECORD(Entry, FATX_FILE_INFO, ListEntry);
		OBJECT_STRING CurrString;
		CurrString.Buffer = CurrFileInfo->FileName;
		CurrString.Length = CurrFileInfo->FileNameLength;
		if (RtlEqualString(Name, &CurrString, TRUE)) {
			// Place the found element at the tail of the list
			RemoveEntryList(Entry);
			InsertTailList(&VolumeExtension->OpenFileList, Entry);
			FileInfo = CurrFileInfo;
			break;
		}

		Entry = Entry->Blink;
	}

	return FileInfo;
}

static VOID FatxInsertFile(PFAT_VOLUME_EXTENSION VolumeExtension, PFATX_FILE_INFO FileInfo)
{
	InsertTailList(&VolumeExtension->OpenFileList, &FileInfo->ListEntry);
}

static VOID FatxRemoveFile(PFAT_VOLUME_EXTENSION VolumeExtension, PFATX_FILE_INFO FileInfo)
{
	RemoveEntryList(&FileInfo->ListEntry);
}

static NTSTATUS FatxSetupVolumeExtension(PFAT_VOLUME_EXTENSION VolumeExtension, PPARTITION_INFORMATION PartitionInformation)
{
	PFATX_SUPERBLOCK Superblock;
	ULONG HostHandle = PARTITION0_HANDLE + PartitionInformation->PartitionNumber;
	VolumeExtension->CacheExtension.HostHandle = HostHandle;
	VolumeExtension->CacheExtension.DeviceType = DEV_PARTITION0 + PartitionInformation->PartitionNumber;
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

	ULONG ClusterShift;
	VolumeExtension->BytesPerCluster = Superblock->ClusterSize << VolumeExtension->SectorShift;
	RtlpBitScanForward(&ClusterShift, VolumeExtension->BytesPerCluster);
	VolumeExtension->ClusterShift = UCHAR(ClusterShift);
	VolumeExtension->VolumeID = Superblock->VolumeID;
	VolumeExtension->NumberOfClusters = ULONG((PartitionInformation->PartitionLength.QuadPart - sizeof(FATX_SUPERBLOCK)) >> VolumeExtension->ClusterShift);
	VolumeExtension->NumberOfClustersAvailable = 0;
	VolumeExtension->VolumeInfo.FileNameLength = 0;
	VolumeExtension->VolumeInfo.HostHandle = HostHandle;
	VolumeExtension->VolumeInfo.Flags = FATX_VOLUME_FILE;

	FscUnmapElementPage(Superblock);

	// Calculate how much free space we have in this partition
	ULONG FatLength;
	BOOLEAN IsFatx16;
	if ((PartitionInformation->PartitionNumber >= 2) && (PartitionInformation->PartitionNumber <= 5)) {
		FatLength = VolumeExtension->NumberOfClusters << 1;
		IsFatx16 = TRUE;
	} else {
		FatLength = VolumeExtension->NumberOfClusters << 2;
		IsFatx16 = FALSE;
	}

	ULONG FatBytesLeft = FatLength, FatOffset = sizeof(FATX_SUPERBLOCK);
	while (FatBytesLeft > 0) {
		PUCHAR FatBuffer;
		ULONG FatBytesToMap = FatBytesLeft > PAGE_SIZE ? PAGE_SIZE : FatBytesLeft;
		if (NTSTATUS Status = FscMapElementPage(&VolumeExtension->CacheExtension, FatOffset, (PVOID *)&FatBuffer, FALSE); !NT_SUCCESS(Status)) {
			return Status;
		}
		if (IsFatx16) {
			PUSHORT FatPage = (PUSHORT)FatBuffer;
			for (ULONG i = 0; i < FatBytesToMap; i += 2) {
				if (FatPage[i >> 1] == FATX_CLUSTER_FREE) {
					VolumeExtension->NumberOfClustersAvailable++;
				}
			}
		} else {
			PULONG FatPage = (PULONG)FatBuffer;
			for (ULONG i = 0; i < FatBytesToMap; i += 4) {
				if (FatPage[i >> 2] == FATX_CLUSTER_FREE) {
					VolumeExtension->NumberOfClustersAvailable++;
				}
			}
		}
		FatBytesLeft -= FatBytesToMap;
		FatOffset += FatBytesToMap;
		FscUnmapElementPage(FatBuffer);
	}

	return STATUS_SUCCESS;
}

static VOID FatxDeleteVolume(PDEVICE_OBJECT DeviceObject)
{
	PFAT_VOLUME_EXTENSION VolumeExtension = (PFAT_VOLUME_EXTENSION)DeviceObject->DeviceExtension;
	FscFlushPartitionElements(&VolumeExtension->CacheExtension);

	// NOTE: we never delete the HDD TargetDeviceObject
	PDEVICE_OBJECT HddDeviceObject = VolumeExtension->CacheExtension.TargetDeviceObject;
	ULONG DeviceObjectHasName = DeviceObject->Flags & DO_DEVICE_HAS_NAME;
	assert(VolumeExtension->Flags & FATX_VOLUME_DISMOUNTED);
	assert(VolumeExtension->CacheExtension.TargetDeviceObject);

	// Set DeletePending flag so that new open requests in IoParseDevice will now fail
	IoDeleteDevice(DeviceObject);

	KIRQL OldIrql = KeRaiseIrqlToDpcLevel();
	HddDeviceObject->MountedOrSelfDevice = nullptr;
	KfLowerIrql(OldIrql);

	// Dereferece DeviceObject because IoCreateDevice creates the object with a ref counter biased by one. Only do this if the device doesn't have a name, because IoDeleteDevice
	// internally calls ObMakeTemporaryObject for named devices, which dereferences the DeviceObject already
	if (!DeviceObjectHasName) {
		ObfDereferenceObject(DeviceObject);
	}
}

static NTSTATUS XBOXAPI FatxCompletionRoutine(DEVICE_OBJECT *DeviceObject, IRP *Irp, PVOID Context)
{
	// This is called from IofCompleteRequest while holding the fatx volume lock. Note that IrpStackPointer points again to the PIO_STACK_LOCATION of the fatx driver
	// because it calls IoGetCurrentIrpStackLocation after having incremented the IrpStackPointer

	if (Irp->PendingReturned) {
		IoMarkIrpPending(Irp);
	}

	// Read and write requests are completed directly in the fatx driver, without having to call a lower level driver. The only exception to this is the MU driver,
	// because that's part of the game itself, and not of the kernel
	PFAT_VOLUME_EXTENSION VolumeExtension = (PFAT_VOLUME_EXTENSION)DeviceObject->DeviceExtension;
	if (VolumeExtension->CacheExtension.TargetDeviceObject->DeviceType == FILE_DEVICE_MEMORY_UNIT) {
		RIP_API_MSG("Unimplemented memory unit support");
	}

	PIO_STACK_LOCATION IrpStackPointer = IoGetCurrentIrpStackLocation(Irp);
	assert((IrpStackPointer->MajorFunction != IRP_MJ_READ) && (IrpStackPointer->MajorFunction != IRP_MJ_WRITE));

	FatxVolumeUnlock(VolumeExtension);

	return STATUS_SUCCESS;
}

NTSTATUS FatxCreateVolume(PDEVICE_OBJECT DeviceObject)
{
	// NOTE: This is called while holding DeviceObject->DeviceLock

	DWORD BytesPerSector;
	PARTITION_INFORMATION PartitionInformation;
	
	switch (DeviceObject->DeviceType)
	{
	default:
	case FILE_DEVICE_MEMORY_UNIT: // TODO
	case FILE_DEVICE_MEDIA_BOARD: // TODO
		RIP_API_FMT("Unimplemented DeviceType %u", DeviceObject->DeviceType);

	case FILE_DEVICE_DISK:
		BytesPerSector = HDD_SECTOR_SIZE;
		PartitionInformation = PIDE_DISK_EXTENSION(DeviceObject->DeviceExtension)->PartitionInformation;
		break;
	}

	PDEVICE_OBJECT FatxDeviceObject;
	NTSTATUS Status = IoCreateDevice(&FatxDriverObject, sizeof(FAT_VOLUME_EXTENSION), nullptr, FILE_DEVICE_DISK_FILE_SYSTEM, FALSE, &FatxDeviceObject);
	if (!NT_SUCCESS(Status)) {
		return Status;
	}

	FatxDeviceObject->StackSize = FatxDeviceObject->StackSize + DeviceObject->StackSize;
	FatxDeviceObject->SectorSize = BytesPerSector;

	if (FatxDeviceObject->AlignmentRequirement < DeviceObject->AlignmentRequirement) {
		FatxDeviceObject->AlignmentRequirement = DeviceObject->AlignmentRequirement;
	}

	if (DeviceObject->Flags & DO_SCATTER_GATHER_IO) {
		FatxDeviceObject->Flags |= DO_SCATTER_GATHER_IO;
	}

	ULONG SectorShift;
	PFAT_VOLUME_EXTENSION VolumeExtension = (PFAT_VOLUME_EXTENSION)FatxDeviceObject->DeviceExtension;
	VolumeExtension->CacheExtension.TargetDeviceObject = DeviceObject;
	VolumeExtension->CacheExtension.SectorSize = FatxDeviceObject->SectorSize;
	RtlpBitScanForward(&SectorShift, BytesPerSector);
	VolumeExtension->SectorShift = UCHAR(SectorShift);
	InitializeListHead(&VolumeExtension->OpenFileList);
	ExInitializeReadWriteLock(&VolumeExtension->VolumeMutex);

	if (Status = FatxSetupVolumeExtension(VolumeExtension, &PartitionInformation); !NT_SUCCESS(Status)) {
		VolumeExtension->Flags |= FATX_VOLUME_DISMOUNTED;
		FatxDeleteVolume(FatxDeviceObject);
		return Status;
	}

	FatxDeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

	DeviceObject->MountedOrSelfDevice = FatxDeviceObject;

	return STATUS_SUCCESS;
}

static NTSTATUS XBOXAPI FatxIrpCreate(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	PFAT_VOLUME_EXTENSION VolumeExtension = (PFAT_VOLUME_EXTENSION)DeviceObject->DeviceExtension;
	FatxVolumeLockExclusive(VolumeExtension);

	PIO_STACK_LOCATION IrpStackPointer = IoGetCurrentIrpStackLocation(Irp);
	POBJECT_STRING RemainingName = IrpStackPointer->Parameters.Create.RemainingName;
	PFILE_OBJECT FileObject = IrpStackPointer->FileObject;
	PFILE_OBJECT RelatedFileObject = FileObject->RelatedFileObject;
	ACCESS_MASK DesiredAccess = IrpStackPointer->Parameters.Create.DesiredAccess;
	UCHAR Attributes = IrpStackPointer->Parameters.Create.FileAttributes & FATX_VALID_FILE_ATTRIBUES;
	USHORT ShareAccess = IrpStackPointer->Parameters.Create.ShareAccess;
	ULONG Disposition = (IrpStackPointer->Parameters.Create.Options >> 24) & 0xFF;
	ULONG CreateOptions = IrpStackPointer->Parameters.Create.Options & 0xFFFFFF;
	ULONG InitialSize = Irp->Overlay.AllocationSize.LowPart;
	POBJECT_ATTRIBUTES ObjectAttributes = POBJECT_ATTRIBUTES(Irp->Tail.Overlay.DriverContext[0]);

	if (VolumeExtension->Flags & FATX_VOLUME_DISMOUNTED) {
		return FatxCompleteRequest(Irp, STATUS_VOLUME_DISMOUNTED, VolumeExtension);
	}

	if (CreateOptions & FILE_OPEN_BY_FILE_ID) { // not supported on FAT
		return FatxCompleteRequest(Irp, STATUS_NOT_IMPLEMENTED, VolumeExtension);
	}

	if (Irp->Overlay.AllocationSize.HighPart) { // 4 GiB file size limit on FAT
		return FatxCompleteRequest(Irp, STATUS_INVALID_PARAMETER, VolumeExtension);
	}

	if (ObjectAttributes->ObjectName->Length > FATX_PATH_NAME_LENGTH) { // 250 characters path limit on fatx
		return FatxCompleteRequest(Irp, STATUS_OBJECT_NAME_INVALID, VolumeExtension);
	}

	PFATX_FILE_INFO FileInfo;
	OBJECT_STRING FileName;
	CHAR RootName = OB_PATH_DELIMITER;
	BOOLEAN HasBackslashAtEnd = FALSE;
	if (RelatedFileObject) {
		FileInfo = (PFATX_FILE_INFO)RelatedFileObject->FsContext2;

		if (!(FileInfo->Flags & FATX_DIRECTORY_FILE)) {
			return FatxCompleteRequest(Irp, STATUS_INVALID_PARAMETER, VolumeExtension);
		}

		if (FileInfo->Flags & FATX_DELETE_ON_CLOSE) {
			return FatxCompleteRequest(Irp, STATUS_DELETE_PENDING, VolumeExtension);
		}

		if (RemainingName->Length == 0) {
			HasBackslashAtEnd = TRUE;
			FileName.Buffer = FileInfo->FileName;
			FileName.Length = FileInfo->FileNameLength;
			if ((FileName.Length == 1) && (FileName.Buffer[0] == OB_PATH_DELIMITER)) {
				if (IrpStackPointer->Flags & SL_OPEN_TARGET_DIRECTORY) { // cannot rename the root directory
					return FatxCompleteRequest(Irp, STATUS_INVALID_PARAMETER, VolumeExtension);
				}

				if ((Disposition != FILE_OPEN) && (Disposition != FILE_OPEN_IF)) { // the root directory can only be opened
					return FatxCompleteRequest(Irp, STATUS_OBJECT_NAME_COLLISION, VolumeExtension);
				}
			}
			goto ByPassPathCheck;
		}
		goto OpenFile;
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

		PFATX_FILE_INFO FileInfo = &VolumeExtension->VolumeInfo;
		if (FileInfo->ShareAccess.OpenCount == 0) {
			// The requested volume is already open, but without any users to it, set the sharing permissions
			IoSetShareAccess(DesiredAccess, ShareAccess, FileObject, &FileInfo->ShareAccess);
		}
		else {
			// The requested volume is already open, check the share permissions to see if we can open it again
			if (NTSTATUS Status = IoCheckShareAccess(DesiredAccess, ShareAccess, FileObject, &FileInfo->ShareAccess, TRUE); !NT_SUCCESS(Status)) {
				return FatxCompleteRequest(Irp, Status, VolumeExtension);
			}
		}

		// No host I/O required for opening a volume
		FileInfo->RefCounter++;
		FileObject->FsContext2 = FileInfo;
		FatxInsertFile(VolumeExtension, FileInfo);
		Irp->IoStatus.Information = FILE_OPENED;
		return FatxCompleteRequest(Irp, STATUS_SUCCESS, VolumeExtension);
	}

	if ((RemainingName->Length == 1) && (RemainingName->Buffer[0] == OB_PATH_DELIMITER)) {
		// Special case: open the root directory of the volume

		if (IrpStackPointer->Flags & SL_OPEN_TARGET_DIRECTORY) { // cannot rename the root directory
			return FatxCompleteRequest(Irp, STATUS_INVALID_PARAMETER, VolumeExtension);
		}

		if ((Disposition != FILE_OPEN) && (Disposition != FILE_OPEN_IF)) { // the root directory can only be opened
			return FatxCompleteRequest(Irp, STATUS_OBJECT_NAME_COLLISION, VolumeExtension);
		}

		HasBackslashAtEnd = TRUE;
		FileName.Buffer = &RootName;
		FileName.Length = 1;
		FileInfo = FatxFindOpenFile(VolumeExtension, &FileName);
	}
	else {
		OpenFile:
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

			if (FileInfo = FatxFindOpenFile(VolumeExtension, &FirstName); FileInfo && (FileInfo->Flags & FATX_DELETE_ON_CLOSE)) {
				return FatxCompleteRequest(Irp, STATUS_DELETE_PENDING, VolumeExtension);
			}

			if (LocalRemainingName.Length == 0) {
				break;
			}

			OriName = LocalRemainingName;
		}
		FileName = FirstName;
	}
	ByPassPathCheck:

	CHAR FullPathBuffer[FATX_PATH_NAME_LENGTH * 2];
	OBJECT_STRING FullPath = *ObjectAttributes->ObjectName;
	if ((FullPath.Length < 7) || (strncmp(FullPath.Buffer, "\\Device", 7))) {
		// If the path doesn't start with "\\Device", then there's a sym link in the path. Note that it's not enough to check for RelatedFileObject, because the sym link
		// might not point to a file object (and thus RelatedFileObject would not be set). The sym link created by the cdrom device triggers this case, since the target
		// is the device object "CdRom0" instead of a file object

		USHORT ObjectAttributesLength = ObjectAttributes->ObjectName->Length;
		for (USHORT i = 0; i < FullPath.Length; ++i) {
			if (FullPath.Buffer[i] == OB_PATH_DELIMITER) {
				// Set ObjectName->Length of ObjectAttributes to the sym link name, so that ObpReferenceObjectByName doesn't fail below when it attempts to resolve it
				ObjectAttributes->ObjectName->Length = i;
				break;
			}
		}

		POBJECT_SYMBOLIC_LINK SymbolicLink;
		NTSTATUS Status = ObpReferenceObjectByName(ObjectAttributes, &ObSymbolicLinkObjectType, nullptr, (PVOID *)&SymbolicLink);
		assert(SymbolicLink && NT_SUCCESS(Status));
		USHORT SymbolicLinkLength = GetObjDirInfoHeader(SymbolicLink)->Name.Length, Offset = 0;
		assert(ObjectAttributes->ObjectName->Length == SymbolicLinkLength);
		ObjectAttributes->ObjectName->Length = ObjectAttributesLength;
		USHORT ResolvedSymbolicLinkLength = SymbolicLink->LinkTarget.Length;

		if (SymbolicLinkLength != FullPath.Length) {
			assert(FullPath.Length > SymbolicLinkLength);
			if ((SymbolicLink->LinkTarget.Buffer[ResolvedSymbolicLinkLength - 1] == OB_PATH_DELIMITER) &&
				(FullPath.Buffer[SymbolicLinkLength] == OB_PATH_DELIMITER)) {
				Offset = 1;
			}
			// Copy the resolved sym link name, and then the remaining part of the path too
			strncpy(FullPathBuffer, SymbolicLink->LinkTarget.Buffer, ResolvedSymbolicLinkLength);
			USHORT RemainingPathLength = FullPath.Length - SymbolicLinkLength - Offset;
			strncpy(&FullPathBuffer[ResolvedSymbolicLinkLength], &FullPath.Buffer[SymbolicLinkLength + Offset], RemainingPathLength);
			Offset = RemainingPathLength;
		}
		else {
			// FullPath consists only of the sym link
			strncpy(FullPathBuffer, SymbolicLink->LinkTarget.Buffer, ResolvedSymbolicLinkLength);
		}
		FullPath.Buffer = FullPathBuffer;
		FullPath.Length = ResolvedSymbolicLinkLength + Offset;
		ObfDereferenceObject(SymbolicLink);
	}

	// The optional creation of FileInfo must be the last thing to happen before SubmitIoRequestToHost. This, because if the IO fails, then IoParseDevice sets FileObject->DeviceObject
	// to a nullptr, which causes it to invoke IopDeleteFile via ObfDereferenceObject. Because DeviceObject is nullptr, FatxIrpClose won't be called, thus leaking the host handle and
	// the memory for FileInfo
	BOOLEAN UpdatedShareAccess = FALSE, FileInfoCreated = FALSE;
	LARGE_INTEGER CurrentTime;
	ULONG FatxTimeStamp;
	KeQuerySystemTime(&CurrentTime);
	FatxPackTimestamp(&CurrentTime, &FatxTimeStamp);
	if (FileInfo) {
		if (FileInfo->Flags & FATX_DIRECTORY_FILE) {
			if (CreateOptions & FILE_NON_DIRECTORY_FILE) {
				return FatxCompleteRequest(Irp, STATUS_FILE_IS_A_DIRECTORY, VolumeExtension);
			}
		}
		else {
			if (HasBackslashAtEnd || (CreateOptions & FILE_DIRECTORY_FILE)) {
				return FatxCompleteRequest(Irp, STATUS_NOT_A_DIRECTORY, VolumeExtension);
			}
		}
		UpdatedShareAccess = TRUE;
		if (FileInfo->ShareAccess.OpenCount == 0) {
			// The requested file is already open, but without any users to it, set the sharing permissions
			IoSetShareAccess(DesiredAccess, ShareAccess, FileObject, &FileInfo->ShareAccess);
		}
		else {
			// The requested file is already open, check the share permissions to see if we can open it again
			if (NTSTATUS Status = IoCheckShareAccess(DesiredAccess, ShareAccess, FileObject, &FileInfo->ShareAccess, TRUE); !NT_SUCCESS(Status)) {
				return FatxCompleteRequest(Irp, Status, VolumeExtension);
			}
		}
		++FileInfo->RefCounter;
	}
	else {
		FileInfo = (PFATX_FILE_INFO)ExAllocatePool(sizeof(FATX_FILE_INFO));
		if (FileInfo == nullptr) {
			return FatxCompleteRequest(Irp, STATUS_INSUFFICIENT_RESOURCES, VolumeExtension);
		}
		FileInfoCreated = TRUE;
		FileInfo->HostHandle = (ULONG)&FileInfo;
		FileInfo->FileNameLength = (UCHAR)FileName.Length;
		FileInfo->FileSize = HasBackslashAtEnd ? 0 : InitialSize;
		FileInfo->Flags = CreateOptions & (FILE_DELETE_ON_CLOSE | FILE_DIRECTORY_FILE);
		FileInfo->RefCounter = 1;
		strncpy(FileInfo->FileName, FileName.Buffer, FileName.Length);
		FatxInsertFile(VolumeExtension, FileInfo);
		IoSetShareAccess(DesiredAccess, ShareAccess, FileObject, &FileInfo->ShareAccess);
	}

	FileObject->FsContext2 = FileInfo;
	Attributes = CreateOptions & FILE_DIRECTORY_FILE ? (Attributes | FILE_ATTRIBUTE_DIRECTORY) : (Attributes & ~FILE_ATTRIBUTE_DIRECTORY);

	// Finally submit the I/O request to the host to do the actual work
	// NOTE: we cannot use the xbox handle as the host file handle, because the xbox handle is created by OB only after this I/O request succeeds. This new handle
	// should then be deleted when the file object goes away with IopCloseFile and/or IopDeleteFile
	IoInfoBlockOc InfoBlock = SubmitIoRequestToHost(
		(CreateOptions & FILE_NON_DIRECTORY_FILE ? IoFlags::MustNotBeADirectory : 0) |
		(CreateOptions & FILE_DIRECTORY_FILE ? IoFlags::MustBeADirectory : 0) |
		DEV_TYPE(VolumeExtension->CacheExtension.DeviceType) |
		(Disposition & 7) | IoRequestType::Open,
		InitialSize,
		HasBackslashAtEnd ? FullPath.Length - 1 : FullPath.Length,
		FileInfo->HostHandle,
		(ULONG_PTR)FullPath.Buffer,
		Attributes,
		FatxTimeStamp,
		DesiredAccess,
		CreateOptions
	);

	NTSTATUS Status = HostToNtStatus(InfoBlock.Header.Status);
	if (!NT_SUCCESS(Status)) {
		if (UpdatedShareAccess) {
			IoRemoveShareAccess(FileObject, &FileInfo->ShareAccess);
		}
		if (FileInfoCreated) {
			FatxRemoveFile(VolumeExtension, FileInfo);
			ExFreePool(FileInfo);
			FileObject->FsContext2 = nullptr;
		}
	}
	else if (Status == STATUS_PENDING) {
		// Should not happen right now, because RetrieveIoRequestFromHost is always synchronous
		RIP_API_MSG("Asynchronous IO is not supported");
	}
	else {
		++VolumeExtension->FileObjectCount;
		VolumeExtension->NumberOfClustersAvailable = InfoBlock.Fatx.FreeClusters;
		Irp->IoStatus.Information = InfoBlock.Header.Info;
		FileInfo->FileSize = InfoBlock.FileSize;
		FileInfo->CreationTime = FatxTimeStamp;
		FileInfo->LastAccessTime = FatxTimeStamp;
		FileInfo->LastWriteTime = FatxTimeStamp;
	}

	return FatxCompleteRequest(Irp, Status, VolumeExtension);
}

static NTSTATUS XBOXAPI FatxIrpClose(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	PFAT_VOLUME_EXTENSION VolumeExtension = (PFAT_VOLUME_EXTENSION)DeviceObject->DeviceExtension;
	FatxVolumeLockExclusive(VolumeExtension);

	PIO_STACK_LOCATION IrpStackPointer = IoGetCurrentIrpStackLocation(Irp);
	PFILE_OBJECT FileObject = IrpStackPointer->FileObject;
	PFATX_FILE_INFO FileInfo = (PFATX_FILE_INFO)FileObject->FsContext2;

	// NOTE: it's not viable to check for FileInfo->ShareAccess.OpenCount here, because it's possible to use a file handle with no access rights granted, which would
	// mean that OpenCount is zero and yet the handle is still in use
	if (--FileInfo->RefCounter == 0) {
		if (FileInfo->Flags & FATX_VOLUME_FILE) {
			FatxRemoveFile(VolumeExtension, FileInfo);
		}
		else {
			SubmitIoRequestToHost(
				DEV_TYPE(VolumeExtension->CacheExtension.DeviceType) | IoRequestType::Close,
				FileInfo->HostHandle
			);
			FatxRemoveFile(VolumeExtension, FileInfo);
			ExFreePool(FileInfo);
		}
		FileObject->FsContext2 = nullptr;
	}

	if ((--VolumeExtension->FileObjectCount == 0) && (VolumeExtension->Flags & FATX_VOLUME_DISMOUNTED)) {
		FatxVolumeUnlock(VolumeExtension);
		FatxDeleteVolume(DeviceObject);
	}
	else {
		FatxVolumeUnlock(VolumeExtension);
	}

	Irp->IoStatus.Status = STATUS_SUCCESS;
	IofCompleteRequest(Irp, PRIORITY_BOOST_IO);

	return STATUS_SUCCESS;
}

static NTSTATUS XBOXAPI FatxIrpRead(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	PFAT_VOLUME_EXTENSION VolumeExtension = (PFAT_VOLUME_EXTENSION)DeviceObject->DeviceExtension;
	FatxVolumeLockShared(VolumeExtension);

	PIO_STACK_LOCATION IrpStackPointer = IoGetCurrentIrpStackLocation(Irp);
	PFILE_OBJECT FileObject = IrpStackPointer->FileObject;
	ULONG Length = IrpStackPointer->Parameters.Read.Length;
	LARGE_INTEGER FileOffset = IrpStackPointer->Parameters.Read.ByteOffset;
	PVOID Buffer = Irp->UserBuffer;
	PFATX_FILE_INFO FileInfo = (PFATX_FILE_INFO)FileObject->FsContext2;

	if (VolumeExtension->Flags & FATX_VOLUME_DISMOUNTED) {
		return FatxCompleteRequest(Irp, STATUS_VOLUME_DISMOUNTED, VolumeExtension);
	}

	// Don't allow to operate on the file after all its handles are closed
	if (FileObject->Flags & FO_CLEANUP_COMPLETE) {
		return FatxCompleteRequest(Irp, STATUS_FILE_CLOSED, VolumeExtension);
	}

	// Cannot read from a directory
	if (FileInfo->Flags & FATX_DIRECTORY_FILE) {
		return FatxCompleteRequest(Irp, STATUS_INVALID_DEVICE_REQUEST, VolumeExtension);
	}

	if (FileObject->Flags & FO_NO_INTERMEDIATE_BUFFERING) {
		// NOTE: for the HDD, SectorSize is 512
		if ((FileOffset.LowPart & (DeviceObject->SectorSize - 1)) || (Length & (DeviceObject->SectorSize - 1))) {
			return FatxCompleteRequest(Irp, STATUS_INVALID_PARAMETER, VolumeExtension);
		}
	}

	if (Length == 0) {
		Irp->IoStatus.Information = 0;
		return FatxCompleteRequest(Irp, STATUS_SUCCESS, VolumeExtension);
	}

	if (!(FileInfo->Flags & FATX_VOLUME_FILE)) {
		// Cannot read past the end of the file
		if (FileOffset.HighPart || (FileOffset.LowPart >= FileInfo->FileSize)) {
			return FatxCompleteRequest(Irp, STATUS_END_OF_FILE, VolumeExtension);
		}

		if ((FileOffset.LowPart + Length) > FileInfo->FileSize) {
			// Reduce the number of bytes transferred to avoid reading past the end of the file
			Length = FileInfo->FileSize - FileOffset.LowPart;
		}
	}

	LARGE_INTEGER CurrentTime;
	ULONG FatxTimeStamp;
	KeQuerySystemTime(&CurrentTime);
	FatxPackTimestamp(&CurrentTime, &FatxTimeStamp);

	IoInfoBlock InfoBlock = SubmitIoRequestToHost(
		DEV_TYPE(VolumeExtension->CacheExtension.DeviceType) | IoRequestType::Read,
		FileOffset.LowPart,
		Length,
		(ULONG_PTR)Buffer,
		FileInfo->HostHandle,
		FatxTimeStamp
	);

	NTSTATUS Status = HostToNtStatus(InfoBlock.Status);
	if (Status == STATUS_PENDING) {
		// Should not happen right now, because RetrieveIoRequestFromHost is always synchronous
		RIP_API_MSG("Asynchronous IO is not supported");
	}
	else if (NT_SUCCESS(Status)) {
		if (FileObject->Flags & FO_SYNCHRONOUS_IO) {
			// NOTE: despite the addition not being atomic, this is ok because on fatx the file size limit is 4 GiB, which means the high dword will always be zero with no carry to add to it
			FileObject->CurrentByteOffset.QuadPart += InfoBlock.Info;
		}
		FileInfo->LastAccessTime = FatxTimeStamp;
	}

	Irp->IoStatus.Information = InfoBlock.Info;
	return FatxCompleteRequest(Irp, Status, VolumeExtension);
}

static NTSTATUS XBOXAPI FatxIrpWrite(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	PFAT_VOLUME_EXTENSION VolumeExtension = (PFAT_VOLUME_EXTENSION)DeviceObject->DeviceExtension;
	FatxVolumeLockExclusive(VolumeExtension);

	PIO_STACK_LOCATION IrpStackPointer = IoGetCurrentIrpStackLocation(Irp);
	PFILE_OBJECT FileObject = IrpStackPointer->FileObject;
	ULONG Length = IrpStackPointer->Parameters.Write.Length;
	LARGE_INTEGER FileOffset = IrpStackPointer->Parameters.Write.ByteOffset;
	PVOID Buffer = Irp->UserBuffer;
	PFATX_FILE_INFO FileInfo = (PFATX_FILE_INFO)FileObject->FsContext2;

	if (VolumeExtension->Flags & FATX_VOLUME_DISMOUNTED) {
		return FatxCompleteRequest(Irp, STATUS_VOLUME_DISMOUNTED, VolumeExtension);
	}

	// Don't allow to operate on the file after all its handles are closed
	if (FileObject->Flags & FO_CLEANUP_COMPLETE) {
		return FatxCompleteRequest(Irp, STATUS_FILE_CLOSED, VolumeExtension);
	}

	// Cannot write to a directory
	if (FileInfo->Flags & FATX_DIRECTORY_FILE) {
		return FatxCompleteRequest(Irp, STATUS_INVALID_DEVICE_REQUEST, VolumeExtension);
	}

	if (FileObject->Flags & FO_NO_INTERMEDIATE_BUFFERING) {
		// NOTE: for the HDD, SectorSize is 512
		if ((FileOffset.LowPart & (DeviceObject->SectorSize - 1)) || (Length & (DeviceObject->SectorSize - 1))) {
			return FatxCompleteRequest(Irp, STATUS_INVALID_PARAMETER, VolumeExtension);
		}
	}

	if (Length == 0) {
		Irp->IoStatus.Information = 0;
		return FatxCompleteRequest(Irp, STATUS_SUCCESS, VolumeExtension);
	}

	if (FileOffset.QuadPart == -1) {
		FileOffset.QuadPart = FileInfo->FileSize;
	}

	if (!(FileInfo->Flags & FATX_VOLUME_FILE)) {
		// 4 GiB file size limit on FAT
		if (LARGE_INTEGER NewOffset{ .QuadPart = FileOffset.QuadPart + Length }; NewOffset.HighPart || (NewOffset.LowPart <= FileOffset.LowPart)) {
			return FatxCompleteRequest(Irp, STATUS_DISK_FULL, VolumeExtension);
		}
	}

	LARGE_INTEGER CurrentTime;
	ULONG FatxTimeStamp;
	KeQuerySystemTime(&CurrentTime);
	FatxPackTimestamp(&CurrentTime, &FatxTimeStamp);

	IoInfoBlock InfoBlock = SubmitIoRequestToHost(
		DEV_TYPE(VolumeExtension->CacheExtension.DeviceType) | IoRequestType::Write,
		FileOffset.LowPart,
		Length,
		(ULONG_PTR)Buffer,
		FileInfo->HostHandle,
		FatxTimeStamp
	);

	NTSTATUS Status = HostToNtStatus(InfoBlock.Status);
	if (Status == STATUS_PENDING) {
		// Should not happen right now, because RetrieveIoRequestFromHost is always synchronous
		RIP_API_MSG("Asynchronous IO is not supported");
	}
	else if (NT_SUCCESS(Status)) {
		if ((FileObject->CurrentByteOffset.LowPart + InfoBlock.Info) > FileInfo->FileSize) {
			FileInfo->FileSize = FileObject->CurrentByteOffset.LowPart + InfoBlock.Info;
		}
		if (FileObject->Flags & FO_SYNCHRONOUS_IO) {
			FileObject->CurrentByteOffset.QuadPart += InfoBlock.Info;
		}
		FileInfo->LastAccessTime = FatxTimeStamp;
		FileInfo->LastWriteTime = FatxTimeStamp;
	}

	Irp->IoStatus.Information = InfoBlock.Info;
	return FatxCompleteRequest(Irp, Status, VolumeExtension);
}

static NTSTATUS XBOXAPI FatxIrpQueryInformation(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	PFAT_VOLUME_EXTENSION VolumeExtension = (PFAT_VOLUME_EXTENSION)DeviceObject->DeviceExtension;
	FatxVolumeLockShared(VolumeExtension);

	PIO_STACK_LOCATION IrpStackPointer = IoGetCurrentIrpStackLocation(Irp);
	PFILE_OBJECT FileObject = IrpStackPointer->FileObject;
	PFATX_FILE_INFO FileInfo = (PFATX_FILE_INFO)FileObject->FsContext2;

	if (VolumeExtension->Flags & FATX_VOLUME_DISMOUNTED) {
		return FatxCompleteRequest(Irp, STATUS_VOLUME_DISMOUNTED, VolumeExtension);
	}

	// Don't allow to operate on the file after all its handles are closed
	if (FileObject->Flags & FO_CLEANUP_COMPLETE) {
		return FatxCompleteRequest(Irp, STATUS_FILE_CLOSED, VolumeExtension);
	}

	if ((FileInfo->Flags & FATX_VOLUME_FILE) && (IrpStackPointer->Parameters.QueryFile.FileInformationClass != FilePositionInformation)) {
		return FatxCompleteRequest(Irp, STATUS_INVALID_PARAMETER, VolumeExtension);
	}

	memset(Irp->UserBuffer, 0, IrpStackPointer->Parameters.QueryFile.Length);

	ULONG BytesWritten;
	NTSTATUS Status = STATUS_SUCCESS;
	switch (IrpStackPointer->Parameters.QueryFile.FileInformationClass)
	{
	case FileInternalInformation:
		PFILE_INTERNAL_INFORMATION(Irp->UserBuffer)->IndexNumber.HighPart = ULONG_PTR(VolumeExtension);
		PFILE_INTERNAL_INFORMATION(Irp->UserBuffer)->IndexNumber.LowPart = ULONG_PTR(FileInfo);
		BytesWritten = sizeof(FILE_INTERNAL_INFORMATION);
		break;

	case FilePositionInformation:
		assert(!(FileObject->Flags & FO_SYNCHRONOUS_IO));
		PFILE_POSITION_INFORMATION(Irp->UserBuffer)->CurrentByteOffset = FileObject->CurrentByteOffset;
		BytesWritten = sizeof(FILE_POSITION_INFORMATION);
		break;

	case FileNetworkOpenInformation: {
		PFILE_NETWORK_OPEN_INFORMATION NetworkOpenInformation = (PFILE_NETWORK_OPEN_INFORMATION)Irp->UserBuffer;
		NetworkOpenInformation->CreationTime = FatxUnpackTimestamp(FileInfo->CreationTime);
		NetworkOpenInformation->LastAccessTime = FatxUnpackTimestamp(FileInfo->LastAccessTime);
		NetworkOpenInformation->LastWriteTime = FatxUnpackTimestamp(FileInfo->LastWriteTime);
		NetworkOpenInformation->ChangeTime = NetworkOpenInformation->LastWriteTime;
		if (FileInfo->Flags & FILE_DIRECTORY_FILE) {
			NetworkOpenInformation->FileAttributes = FILE_ATTRIBUTE_DIRECTORY;
			NetworkOpenInformation->AllocationSize.QuadPart = 0;
			NetworkOpenInformation->EndOfFile.QuadPart = 0;
		}
		else {
			NetworkOpenInformation->FileAttributes = FILE_ATTRIBUTE_NORMAL;
			NetworkOpenInformation->AllocationSize.QuadPart = (ULONGLONG)FileInfo->FileSize;
			NetworkOpenInformation->EndOfFile.QuadPart = (ULONGLONG)FileInfo->FileSize;
		}
		BytesWritten = sizeof(FILE_NETWORK_OPEN_INFORMATION);
	}
	break;

	case FileModeInformation: {
		PFILE_MODE_INFORMATION ModeInformation = (PFILE_MODE_INFORMATION)Irp->UserBuffer;
		ModeInformation->Mode = 0;
		if (FileObject->Flags & FO_SEQUENTIAL_ONLY) {
			ModeInformation->Mode |= FILE_SEQUENTIAL_ONLY;
		}
		if (FileObject->Flags & FO_NO_INTERMEDIATE_BUFFERING) {
			ModeInformation->Mode |= FILE_NO_INTERMEDIATE_BUFFERING;
		}
		if (FileObject->Flags & FO_SYNCHRONOUS_IO) {
			if (FileObject->Flags & FO_ALERTABLE_IO) {
				ModeInformation->Mode |= FILE_SYNCHRONOUS_IO_ALERT;
			}
			else {
				ModeInformation->Mode |= FILE_SYNCHRONOUS_IO_NONALERT;
			}
		}
		BytesWritten = sizeof(FILE_MODE_INFORMATION);
	}
	break;

	case FileAlignmentInformation:
		PFILE_ALIGNMENT_INFORMATION(Irp->UserBuffer)->AlignmentRequirement = DeviceObject->AlignmentRequirement;
		BytesWritten = sizeof(FILE_ALIGNMENT_INFORMATION);
		break;

	default:
		BytesWritten = 0;
		Status = STATUS_INVALID_PARAMETER;
		break;
	}

	Irp->IoStatus.Information = BytesWritten;
	return FatxCompleteRequest(Irp, Status, VolumeExtension);
}

static NTSTATUS XBOXAPI FatxIrpQueryVolumeInformation(PDEVICE_OBJECT DeviceObject, PIRP Irp)
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

static NTSTATUS XBOXAPI FatxIrpDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	PFAT_VOLUME_EXTENSION VolumeExtension = (PFAT_VOLUME_EXTENSION)DeviceObject->DeviceExtension;
	PIO_STACK_LOCATION IrpStackPointer = IoGetCurrentIrpStackLocation(Irp);

	FatxVolumeLockShared(VolumeExtension);

	if (VolumeExtension->Flags & FATX_VOLUME_DISMOUNTED) {
		return FatxCompleteRequest(Irp, STATUS_VOLUME_DISMOUNTED, VolumeExtension);
	}

	PDEVICE_OBJECT TargetDeviceObject = VolumeExtension->CacheExtension.TargetDeviceObject;
	if (TargetDeviceObject->DeviceType != FILE_DEVICE_DISK) {
		RIP_API_FMT("Unimplemented DeviceType %u", DeviceObject->DeviceType);
	}

	IoCopyCurrentIrpStackLocationToNext(Irp);
	IoSetCompletionRoutine(Irp, FatxCompletionRoutine, nullptr, TRUE, TRUE, TRUE);

	NTSTATUS Status = IofCallDriver(TargetDeviceObject, Irp); // invokes HddIrpDeviceControl for HDD

	KeLeaveCriticalRegion();

	return Status;
}

static NTSTATUS XBOXAPI FatxIrpCleanup(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	// This is called when the last open handle to a file is closed by NtClose

	PFAT_VOLUME_EXTENSION VolumeExtension = (PFAT_VOLUME_EXTENSION)DeviceObject->DeviceExtension;

	FatxVolumeLockExclusive(VolumeExtension);

	PIO_STACK_LOCATION IrpStackPointer = IoGetCurrentIrpStackLocation(Irp);
	PFILE_OBJECT FileObject = IrpStackPointer->FileObject;
	PFATX_FILE_INFO FileInfo = (PFATX_FILE_INFO)FileObject->FsContext2;
	IoRemoveShareAccess(FileObject, &FileInfo->ShareAccess);

	if (!(VolumeExtension->Flags & FATX_VOLUME_DISMOUNTED) && (FileInfo->ShareAccess.OpenCount == 0) && (FileInfo->Flags & FATX_DELETE_ON_CLOSE)) {
		IoInfoBlock InfoBlock = SubmitIoRequestToHost(
			DEV_TYPE(VolumeExtension->CacheExtension.DeviceType) | IoRequestType::Remove,
			FileInfo->HostHandle
		);
	}

	FileObject->Flags |= FO_CLEANUP_COMPLETE;

	return FatxCompleteRequest(Irp, STATUS_SUCCESS, VolumeExtension);
}
