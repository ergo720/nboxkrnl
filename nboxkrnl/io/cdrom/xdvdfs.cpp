/*
 * ergo720                Copyright (c) 2024
 */

#include "cdrom.hpp"
#include "xdvdfs.hpp"
#include "obp.hpp"
#include <string.h>
#include <assert.h>


#define XDVDFS_DIRECTORY_FILE             FILE_DIRECTORY_FILE // = 0x00000001
#define XDVDFS_VOLUME_FILE                0x80000000

static NTSTATUS XBOXAPI XdvdfsIrpCreate(PDEVICE_OBJECT DeviceObject, PIRP Irp);
static NTSTATUS XBOXAPI XdvdfsIrpClose(PDEVICE_OBJECT DeviceObject, PIRP Irp);
static NTSTATUS XBOXAPI XdvdfsIrpRead(PDEVICE_OBJECT DeviceObject, PIRP Irp);
static NTSTATUS XBOXAPI XdvdfsIrpDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp);
static NTSTATUS XBOXAPI XdvdfsIrpCleanup(PDEVICE_OBJECT DeviceObject, PIRP Irp);

static DRIVER_OBJECT XdvdfsDriverObject = {
	nullptr,                            // DriverStartIo
	nullptr,                            // DriverDeleteDevice
	nullptr,                            // DriverDismountVolume
	{
		XdvdfsIrpCreate,                // IRP_MJ_CREATE
		XdvdfsIrpClose,                 // IRP_MJ_CLOSE
		XdvdfsIrpRead,                  // IRP_MJ_READ
		IoInvalidDeviceRequest,         // IRP_MJ_WRITE
		IoInvalidDeviceRequest,         // IRP_MJ_QUERY_INFORMATION
		IoInvalidDeviceRequest,         // IRP_MJ_SET_INFORMATION
		IoInvalidDeviceRequest,         // IRP_MJ_FLUSH_BUFFERS
		IoInvalidDeviceRequest,         // IRP_MJ_QUERY_VOLUME_INFORMATION
		IoInvalidDeviceRequest,         // IRP_MJ_DIRECTORY_CONTROL
		IoInvalidDeviceRequest,         // IRP_MJ_FILE_SYSTEM_CONTROL
		XdvdfsIrpDeviceControl,         // IRP_MJ_DEVICE_CONTROL
		IoInvalidDeviceRequest,         // IRP_MJ_INTERNAL_DEVICE_CONTROL
		IoInvalidDeviceRequest,         // IRP_MJ_SHUTDOWN
		XdvdfsIrpCleanup,               // IRP_MJ_CLEANUP
	}
};

static VOID XdvdfsVolumeLockExclusive(PXISO_VOLUME_EXTENSION VolumeExtension)
{
	KeEnterCriticalRegion();
	ExAcquireReadWriteLockExclusive(&VolumeExtension->VolumeMutex);
}

static VOID XdvdfsVolumeLockShared(PXISO_VOLUME_EXTENSION VolumeExtension)
{
	KeEnterCriticalRegion();
	ExAcquireReadWriteLockShared(&VolumeExtension->VolumeMutex);
}

static VOID XdvdfsVolumeUnlock(PXISO_VOLUME_EXTENSION VolumeExtension)
{
	ExReleaseReadWriteLock(&VolumeExtension->VolumeMutex);
	KeLeaveCriticalRegion();
}

static NTSTATUS XdvdfsCompleteRequest(PIRP Irp, NTSTATUS Status, PXISO_VOLUME_EXTENSION VolumeExtension)
{
	XdvdfsVolumeUnlock(VolumeExtension);
	Irp->IoStatus.Status = Status;
	IofCompleteRequest(Irp, PRIORITY_BOOST_IO);
	return Status;
}

static PXISO_FILE_INFO XdvdfsFindOpenFile(PXISO_VOLUME_EXTENSION VolumeExtension, POBJECT_STRING Name)
{
	PXISO_FILE_INFO FileInfo = nullptr;
	PLIST_ENTRY Entry = VolumeExtension->OpenFileList.Blink;
	while (Entry != &VolumeExtension->OpenFileList) {
		PXISO_FILE_INFO CurrFileInfo = CONTAINING_RECORD(Entry, XISO_FILE_INFO, ListEntry);
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

static VOID XdvdfsInsertFile(PXISO_VOLUME_EXTENSION VolumeExtension, PXISO_FILE_INFO FileInfo)
{
	InsertTailList(&VolumeExtension->OpenFileList, &FileInfo->ListEntry);
}

static VOID XdvdfsRemoveFile(PXISO_VOLUME_EXTENSION VolumeExtension, PXISO_FILE_INFO FileInfo)
{
	RemoveEntryList(&FileInfo->ListEntry);
}

static VOID XdvdfsDeleteVolume(PDEVICE_OBJECT DeviceObject)
{
	PXISO_VOLUME_EXTENSION VolumeExtension = (PXISO_VOLUME_EXTENSION)DeviceObject->DeviceExtension;

	// NOTE: we never delete the HDD TargetDeviceObject
	PDEVICE_OBJECT CdRomDeviceObject = VolumeExtension->TargetDeviceObject;
	ULONG DeviceObjectHasName = DeviceObject->Flags & DO_DEVICE_HAS_NAME;
	assert(VolumeExtension->Dismounted);
	assert(VolumeExtension->TargetDeviceObject);

	// Set DeletePending flag so that new open requests in IoParseDevice will now fail
	IoDeleteDevice(DeviceObject);

	KIRQL OldIrql = KeRaiseIrqlToDpcLevel();
	CdRomDeviceObject->MountedOrSelfDevice = nullptr;
	KfLowerIrql(OldIrql);

	// Dereference DeviceObject because IoCreateDevice creates the object with a ref counter biased by one. Only do this if the device doesn't have a name, because IoDeleteDevice
	// internally calls ObMakeTemporaryObject for named devices, which dereferences the DeviceObject already
	if (!DeviceObjectHasName) {
		ObfDereferenceObject(DeviceObject);
	}
}

NTSTATUS XdvdfsCreateVolume(PDEVICE_OBJECT DeviceObject)
{
	DISK_GEOMETRY DiskGeometry;
	DiskGeometry.Cylinders.QuadPart = CDROM_TOTAL_NUM_OF_SECTORS;
	DiskGeometry.MediaType = RemovableMedia;
	DiskGeometry.TracksPerCylinder = 1;
	DiskGeometry.SectorsPerTrack = 1;
	DiskGeometry.BytesPerSector = CDROM_SECTOR_SIZE;

	PDEVICE_OBJECT XdvdfsDeviceObject;
	NTSTATUS Status = IoCreateDevice(&XdvdfsDriverObject, sizeof(XISO_VOLUME_EXTENSION), nullptr, FILE_DEVICE_CD_ROM_FILE_SYSTEM, FALSE, &XdvdfsDeviceObject);
	if (!NT_SUCCESS(Status)) {
		return Status;
	}

	XdvdfsDeviceObject->StackSize = XdvdfsDeviceObject->StackSize + DeviceObject->StackSize;
	XdvdfsDeviceObject->SectorSize = DiskGeometry.BytesPerSector;

	if (XdvdfsDeviceObject->AlignmentRequirement < DeviceObject->AlignmentRequirement) {
		XdvdfsDeviceObject->AlignmentRequirement = DeviceObject->AlignmentRequirement;
	}

	if (DeviceObject->Flags & DO_SCATTER_GATHER_IO) {
		XdvdfsDeviceObject->Flags |= DO_SCATTER_GATHER_IO;
	}

	PXISO_VOLUME_EXTENSION VolumeExtension = (PXISO_VOLUME_EXTENSION)XdvdfsDeviceObject->DeviceExtension;

	VolumeExtension->TargetDeviceObject = DeviceObject;
	VolumeExtension->SectorSize = DiskGeometry.BytesPerSector;
	VolumeExtension->PartitionSectorCount = DiskGeometry.Cylinders.LowPart;
	VolumeExtension->PartitionLength.QuadPart = (ULONGLONG)DiskGeometry.Cylinders.LowPart << 11;
	VolumeExtension->VolumeInfo.FileNameLength = 0;
	VolumeExtension->VolumeInfo.HostHandle = CDROM_HANDLE;
	VolumeExtension->VolumeInfo.Flags = XDVDFS_VOLUME_FILE;
	InitializeListHead(&VolumeExtension->OpenFileList);
	ExInitializeReadWriteLock(&VolumeExtension->VolumeMutex);

	XdvdfsDeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

	DeviceObject->MountedOrSelfDevice = XdvdfsDeviceObject;

	return STATUS_SUCCESS;
}

static NTSTATUS XBOXAPI XdvdfsIrpCreate(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	PXISO_VOLUME_EXTENSION VolumeExtension = (PXISO_VOLUME_EXTENSION)DeviceObject->DeviceExtension;
	XdvdfsVolumeLockExclusive(VolumeExtension);

	PIO_STACK_LOCATION IrpStackPointer = IoGetCurrentIrpStackLocation(Irp);
	POBJECT_STRING RemainingName = IrpStackPointer->Parameters.Create.RemainingName;
	PFILE_OBJECT FileObject = IrpStackPointer->FileObject;
	PFILE_OBJECT RelatedFileObject = FileObject->RelatedFileObject;
	ACCESS_MASK DesiredAccess = IrpStackPointer->Parameters.Create.DesiredAccess;
	ULONG Disposition = (IrpStackPointer->Parameters.Create.Options >> 24) & 0xFF;
	ULONG CreateOptions = IrpStackPointer->Parameters.Create.Options & 0xFFFFFF;
	ULONG InitialSize = Irp->Overlay.AllocationSize.LowPart;

	if (VolumeExtension->Dismounted) {
		return XdvdfsCompleteRequest(Irp, STATUS_VOLUME_DISMOUNTED, VolumeExtension);
	}

	if (IrpStackPointer->Flags & SL_OPEN_TARGET_DIRECTORY) { // cannot rename directories (xiso is read-only media)
		return XdvdfsCompleteRequest(Irp, STATUS_ACCESS_DENIED, VolumeExtension);
	}

	if (DesiredAccess & (FILE_WRITE_ATTRIBUTES | FILE_WRITE_DATA | // don't allow any write access (xiso is read-only media)
		FILE_WRITE_EA | FILE_ADD_FILE | FILE_ADD_SUBDIRECTORY |
		FILE_APPEND_DATA | FILE_DELETE_CHILD | DELETE | WRITE_DAC)) {
		return XdvdfsCompleteRequest(Irp, STATUS_ACCESS_DENIED, VolumeExtension);
	}

	if (CreateOptions & FILE_OPEN_BY_FILE_ID) { // not supported on xiso
		return XdvdfsCompleteRequest(Irp, STATUS_NOT_IMPLEMENTED, VolumeExtension);
	}

	if ((Disposition != FILE_OPEN) && (Disposition != FILE_OPEN_IF)) { // only allow open operations (xiso is read-only media)
		return XdvdfsCompleteRequest(Irp, STATUS_ACCESS_DENIED, VolumeExtension);
	}

	PXISO_FILE_INFO FileInfo;
	OBJECT_STRING FileName;
	CHAR RootName = OB_PATH_DELIMITER;
	BOOLEAN HasBackslashAtEnd = FALSE;
	if (RelatedFileObject) {
		FileInfo = (PXISO_FILE_INFO)RelatedFileObject->FsContext2;

		if (!(FileInfo->Flags & XDVDFS_DIRECTORY_FILE)) {
			return XdvdfsCompleteRequest(Irp, STATUS_INVALID_PARAMETER, VolumeExtension);
		}

		if (RemainingName->Length == 0) {
			HasBackslashAtEnd = TRUE;
			FileName.Buffer = FileInfo->FileName;
			FileName.Length = FileInfo->FileNameLength;
			goto ByPassPathCheck;
		}
		goto OpenFile;
	}

	if (RemainingName->Length == 0) {
		// Special case: open the volume itself

		if (CreateOptions & FILE_DIRECTORY_FILE) { // volume is not a directory
			return XdvdfsCompleteRequest(Irp, STATUS_NOT_A_DIRECTORY, VolumeExtension);
		}

		SHARE_ACCESS ShareAccess;
		IoSetShareAccess(DesiredAccess, 0, FileObject, &ShareAccess);
		FileObject->Flags |= FO_NO_INTERMEDIATE_BUFFERING;

		// No host I/O required for opening a volume
		PXISO_FILE_INFO FileInfo = &VolumeExtension->VolumeInfo;
		FileInfo->RefCounter++;
		FileObject->FsContext2 = FileInfo;
		XdvdfsInsertFile(VolumeExtension, FileInfo);
		Irp->IoStatus.Information = FILE_OPENED;
		return XdvdfsCompleteRequest(Irp, STATUS_SUCCESS, VolumeExtension);
	}

	if ((RemainingName->Length == 1) && (RemainingName->Buffer[0] == OB_PATH_DELIMITER)) {
		// Special case: open the root directory of the volume

		HasBackslashAtEnd = TRUE;
		FileName.Buffer = &RootName;
		FileName.Length = 1;
		FileInfo = XdvdfsFindOpenFile(VolumeExtension, &FileName);
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
				return XdvdfsCompleteRequest(Irp, STATUS_OBJECT_NAME_INVALID, VolumeExtension);
			}

			if (LocalRemainingName.Length == 0) {
				break;
			}

			OriName = LocalRemainingName;
		}
		FileInfo = XdvdfsFindOpenFile(VolumeExtension, &FirstName);
		FileName = FirstName;
	}
ByPassPathCheck:

	POBJECT_ATTRIBUTES ObjectAttributes = POBJECT_ATTRIBUTES(Irp->Tail.Overlay.DriverContext[0]);
	OBJECT_STRING FullPath = *ObjectAttributes->ObjectName;
	BOOLEAN FullPathBufferAllocated = FALSE;
	PCHAR FullPathBuffer;
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
			// Allocate enough space for the resolved link and the remaining path name. e.g. D: -> \Device\CdRom0 and path is D:\default.xbe, then strlen(\Device\CdRom0) + strlen(default.xbe) + 1
			FullPathBuffer = (PCHAR)ExAllocatePool(ResolvedSymbolicLinkLength + RemainingName->Length + 1);
			if (FullPathBuffer == nullptr) {
				return XdvdfsCompleteRequest(Irp, STATUS_INSUFFICIENT_RESOURCES, VolumeExtension);
			}
			// Copy the resolved sym link name, and then the remaining part of the path too
			strncpy(FullPathBuffer, SymbolicLink->LinkTarget.Buffer, ResolvedSymbolicLinkLength);
			USHORT RemainingPathLength = FullPath.Length - SymbolicLinkLength - Offset;
			strncpy(&FullPathBuffer[ResolvedSymbolicLinkLength], &FullPath.Buffer[SymbolicLinkLength + Offset], RemainingPathLength);
			Offset = RemainingPathLength;
		}
		else {
			// FullPath consists only of the sym link, allocate enough space for the resolved link
			FullPathBuffer = (PCHAR)ExAllocatePool(ResolvedSymbolicLinkLength);
			if (FullPathBuffer == nullptr) {
				return XdvdfsCompleteRequest(Irp, STATUS_INSUFFICIENT_RESOURCES, VolumeExtension);
			}
			strncpy(FullPathBuffer, SymbolicLink->LinkTarget.Buffer, ResolvedSymbolicLinkLength);
		}
		FullPathBufferAllocated = TRUE;
		FullPath.Buffer = FullPathBuffer;
		FullPath.Length = ResolvedSymbolicLinkLength + Offset;
		ObfDereferenceObject(SymbolicLink);
	}

	// The optional creation of FileInfo must be the last thing to happen before SubmitIoRequestToHost. This, because if the IO fails, then IoParseDevice sets FileObject->DeviceObject
	// to a nullptr, which causes it to invoke IopDeleteFile via ObfDereferenceObject. Because DeviceObject is nullptr, XdvdfsIrpClose won't be called, thus leaking the host handle and
	// the memory for FileInfo
	BOOLEAN FileInfoCreated = FALSE;
	if (FileInfo) {
		if (FileInfo->Flags & XDVDFS_DIRECTORY_FILE) {
			if (CreateOptions & FILE_NON_DIRECTORY_FILE) {
				if (FullPathBufferAllocated) {
					ExFreePool(FullPathBuffer);
				}
				return XdvdfsCompleteRequest(Irp, STATUS_FILE_IS_A_DIRECTORY, VolumeExtension);
			}
		}
		else {
			if (HasBackslashAtEnd || (CreateOptions & FILE_DIRECTORY_FILE)) {
				if (FullPathBufferAllocated) {
					ExFreePool(FullPathBuffer);
				}
				return XdvdfsCompleteRequest(Irp, STATUS_NOT_A_DIRECTORY, VolumeExtension);
			}
		}
		++FileInfo->RefCounter;
	}
	else {
		FileInfo = (PXISO_FILE_INFO)ExAllocatePool(sizeof(XISO_FILE_INFO) + FileName.Length);
		if (FileInfo == nullptr) {
			if (FullPathBufferAllocated) {
				ExFreePool(FullPathBuffer);
			}
			return XdvdfsCompleteRequest(Irp, STATUS_INSUFFICIENT_RESOURCES, VolumeExtension);
		}
		FileInfoCreated = TRUE;
		FileInfo->HostHandle = (ULONG)&FileInfo;
		FileInfo->FileNameLength = FileName.Length;
		FileInfo->FileName = (PCHAR)(FileInfo + 1);
		FileInfo->FileSize = HasBackslashAtEnd ? 0 : InitialSize;
		FileInfo->Flags = CreateOptions & FILE_DIRECTORY_FILE;
		FileInfo->RefCounter = 1;
		strncpy(FileInfo->FileName, FileName.Buffer, FileName.Length);
		XdvdfsInsertFile(VolumeExtension, FileInfo);
	}

	SHARE_ACCESS ShareAccess;
	IoSetShareAccess(DesiredAccess, 0, FileObject, &ShareAccess);
	FileObject->FsContext2 = FileInfo;

	// Finally submit the I/O request to the host to do the actual work
	// NOTE: we cannot use the xbox handle as the host file handle, because the xbox handle is created by OB only after this I/O request succeeds. This new handle
	// should then be deleted when the file object goes away with IopCloseFile and/or IopDeleteFile
	IoInfoBlockOc InfoBlock = SubmitIoRequestToHost(
		(CreateOptions & FILE_NON_DIRECTORY_FILE ? IoFlags::MustNotBeADirectory : 0) |
		(CreateOptions & FILE_DIRECTORY_FILE ? IoFlags::MustBeADirectory : 0) | DEV_TYPE(DEV_CDROM) |
		(Disposition & 7) | IoRequestType::Open,
		InitialSize,
		HasBackslashAtEnd ? FullPath.Length - 1 : FullPath.Length,
		FileInfo->HostHandle,
		(ULONG_PTR)(FullPath.Buffer),
		0,
		0,
		DesiredAccess,
		CreateOptions
	);

	NTSTATUS Status = InfoBlock.Header.NtStatus;
	if (!NT_SUCCESS(Status)) {
		if (FileInfoCreated) {
			XdvdfsRemoveFile(VolumeExtension, FileInfo);
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
		Irp->IoStatus.Information = InfoBlock.Header.Info;
		FileInfo->FileSize = InfoBlock.FileSize;
		FileInfo->Timestamp.QuadPart = InfoBlock.XdvdfsTimestamp;
	}

	if (FullPathBufferAllocated) {
		ExFreePool(FullPathBuffer);
	}

	return XdvdfsCompleteRequest(Irp, Status, VolumeExtension);
}

static NTSTATUS XBOXAPI XdvdfsIrpClose(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	PXISO_VOLUME_EXTENSION VolumeExtension = (PXISO_VOLUME_EXTENSION)DeviceObject->DeviceExtension;
	XdvdfsVolumeLockExclusive(VolumeExtension);

	PIO_STACK_LOCATION IrpStackPointer = IoGetCurrentIrpStackLocation(Irp);
	PFILE_OBJECT FileObject = IrpStackPointer->FileObject;
	PXISO_FILE_INFO FileInfo = (PXISO_FILE_INFO)FileObject->FsContext2;

	if (--FileInfo->RefCounter == 0) {
		if (FileInfo->Flags & XDVDFS_VOLUME_FILE) {
			XdvdfsRemoveFile(VolumeExtension, FileInfo);
		}
		else {
			SubmitIoRequestToHost(
				DEV_TYPE(DEV_CDROM) | IoRequestType::Close,
				FileInfo->HostHandle
			);
			XdvdfsRemoveFile(VolumeExtension, FileInfo);
			ExFreePool(FileInfo);
		}
		FileObject->FsContext2 = nullptr;
	}

	if ((--VolumeExtension->FileObjectCount == 0) && (VolumeExtension->Dismounted)) {
		XdvdfsVolumeUnlock(VolumeExtension);
		XdvdfsDeleteVolume(DeviceObject);
	}
	else {
		XdvdfsVolumeUnlock(VolumeExtension);
	}

	Irp->IoStatus.Status = STATUS_SUCCESS;
	IofCompleteRequest(Irp, PRIORITY_BOOST_IO);

	return STATUS_SUCCESS;
}

static NTSTATUS XBOXAPI XdvdfsIrpRead(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	PXISO_VOLUME_EXTENSION VolumeExtension = (PXISO_VOLUME_EXTENSION)DeviceObject->DeviceExtension;
	XdvdfsVolumeLockShared(VolumeExtension);

	PIO_STACK_LOCATION IrpStackPointer = IoGetCurrentIrpStackLocation(Irp);
	PFILE_OBJECT FileObject = IrpStackPointer->FileObject;
	ULONG Length = IrpStackPointer->Parameters.Read.Length;
	LARGE_INTEGER FileOffset = IrpStackPointer->Parameters.Read.ByteOffset;
	PVOID Buffer = Irp->UserBuffer;
	PXISO_FILE_INFO FileInfo = (PXISO_FILE_INFO)FileObject->FsContext2;

	if (VolumeExtension->Dismounted) {
		return XdvdfsCompleteRequest(Irp, STATUS_VOLUME_DISMOUNTED, VolumeExtension);
	}

	// Don't allow to operate on the file after all its handles are closed
	if (FileObject->Flags & FO_CLEANUP_COMPLETE) {
		return XdvdfsCompleteRequest(Irp, STATUS_FILE_CLOSED, VolumeExtension);
	}

	// Cannot read from a directory
	if (FileInfo->Flags & XDVDFS_DIRECTORY_FILE) {
		return XdvdfsCompleteRequest(Irp, STATUS_INVALID_DEVICE_REQUEST, VolumeExtension);
	}

	if (FileObject->Flags & FO_NO_INTERMEDIATE_BUFFERING) {
		// NOTE: for the dvd drive, SectorSize is 2048
		if ((FileOffset.LowPart & (DeviceObject->SectorSize - 1)) || (Length & (DeviceObject->SectorSize - 1))) {
			return XdvdfsCompleteRequest(Irp, STATUS_INVALID_PARAMETER, VolumeExtension);
		}
	}

	if (Length == 0) {
		Irp->IoStatus.Information = 0;
		return XdvdfsCompleteRequest(Irp, STATUS_SUCCESS, VolumeExtension);
	}

	if (FileInfo->Flags & XDVDFS_VOLUME_FILE) {
		// Cannot read past the end of the file
		if (FileOffset.HighPart || (FileOffset.LowPart >= FileInfo->FileSize)) {
			return XdvdfsCompleteRequest(Irp, STATUS_END_OF_FILE, VolumeExtension);
		}

		if ((FileOffset.LowPart + Length) > FileInfo->FileSize) {
			// Reduce the number of bytes transferred to avoid reading past the end of the file
			Length = FileInfo->FileSize - FileOffset.LowPart;
		}
	}

	IoInfoBlock InfoBlock = SubmitIoRequestToHost(
		DEV_TYPE(DEV_CDROM) | IoRequestType::Read,
		FileOffset.LowPart,
		Length,
		(ULONG_PTR)Buffer,
		FileInfo->HostHandle
	);

	NTSTATUS Status = InfoBlock.NtStatus;
	if (Status == STATUS_PENDING) {
		// Should not happen right now, because RetrieveIoRequestFromHost is always synchronous
		RIP_API_MSG("Asynchronous IO is not supported");
	}
	else if (NT_SUCCESS(Status)) {
		if (FileObject->Flags & FO_SYNCHRONOUS_IO) {
			// NOTE: despite the addition not being atomic, this is ok because on xiso the file size limit is 4 GiB, which means the high dword will always be zero with no carry to add to it
			FileObject->CurrentByteOffset.QuadPart += InfoBlock.Info;
		}
	}

	Irp->IoStatus.Information = InfoBlock.Info;
	return XdvdfsCompleteRequest(Irp, Status, VolumeExtension);
}

static NTSTATUS XBOXAPI XdvdfsIrpDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	PXISO_VOLUME_EXTENSION VolumeExtension = (PXISO_VOLUME_EXTENSION)DeviceObject->DeviceExtension;
	XdvdfsVolumeLockShared(VolumeExtension);
	PIO_STACK_LOCATION IrpStackPointer = IoGetCurrentIrpStackLocation(Irp);

	if (VolumeExtension->Dismounted) {
		return XdvdfsCompleteRequest(Irp, STATUS_VOLUME_DISMOUNTED, VolumeExtension);
	}

	// This function is used to send various SCSI commands to the dvd drive. For now, we only support the dvd authentication command,
	// and rip on everything else

	NTSTATUS Status;
	switch (IrpStackPointer->Parameters.DeviceIoControl.IoControlCode)
	{
	case IOCTL_SCSI_PASS_THROUGH_DIRECT: {
		// This is used to authenticate an xbox dvd disc. Because nxbx doesn't implement the authentication mechanism, we unconditionally succeed here
		PSCSI_PASS_THROUGH_DIRECT PassThrough = (PSCSI_PASS_THROUGH_DIRECT)IrpStackPointer->Parameters.DeviceIoControl.InputBuffer;
		PDVDX2_AUTHENTICATION Authentication = (PDVDX2_AUTHENTICATION)PassThrough->DataBuffer;
		Authentication->AuthenticationPage.CDFValid = 1;
		Authentication->AuthenticationPage.PartitionArea = 1;
		Authentication->AuthenticationPage.Authentication = 1;
		Status = STATUS_SUCCESS;
	}
	break;

	default:
		RIP_API_FMT("Ripped on unimplemented DVD IOCTL 0x%X", IrpStackPointer->Parameters.DeviceIoControl.IoControlCode);
	}

	return XdvdfsCompleteRequest(Irp, Status, VolumeExtension);
}

static NTSTATUS XBOXAPI XdvdfsIrpCleanup(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	// We cannot delete a file because xiso is read-only media, so only set FO_CLEANUP_COMPLETE to make other irp request fail

	IoGetCurrentIrpStackLocation(Irp)->FileObject->Flags |= FO_CLEANUP_COMPLETE;
	Irp->IoStatus.Status = STATUS_INVALID_DEVICE_REQUEST;
	IofCompleteRequest(Irp, PRIORITY_BOOST_IO);
	return STATUS_INVALID_DEVICE_REQUEST;
}
