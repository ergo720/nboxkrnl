/*
 * ergo720                Copyright (c) 2024
 */

#include "raw.hpp"
#include "mm.hpp"
#include "rtl.hpp"
#include "../hdd/hdd.hpp"
#include <assert.h>


static NTSTATUS XBOXAPI RawIrpCreate(PDEVICE_OBJECT DeviceObject, PIRP Irp);
static NTSTATUS XBOXAPI RawIrpClose(PDEVICE_OBJECT DeviceObject, PIRP Irp);
static NTSTATUS XBOXAPI RawIrpRead(PDEVICE_OBJECT DeviceObject, PIRP Irp);
static NTSTATUS XBOXAPI RawIrpWrite(PDEVICE_OBJECT DeviceObject, PIRP Irp);
static NTSTATUS XBOXAPI RawIrpDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp);
static NTSTATUS XBOXAPI RawIrpCleanup(PDEVICE_OBJECT DeviceObject, PIRP Irp);

static DRIVER_OBJECT RawDriverObject = {
	nullptr,                            // DriverStartIo
	nullptr,                            // DriverDeleteDevice
	nullptr,                            // DriverDismountVolume
	{
		RawIrpCreate,                   // IRP_MJ_CREATE
		RawIrpClose,                    // IRP_MJ_CLOSE
		RawIrpRead,                     // IRP_MJ_READ
		RawIrpWrite,                    // IRP_MJ_WRITE
		IoInvalidDeviceRequest,         // IRP_MJ_QUERY_INFORMATION
		IoInvalidDeviceRequest,         // IRP_MJ_SET_INFORMATION
		IoInvalidDeviceRequest,         // IRP_MJ_FLUSH_BUFFERS
		IoInvalidDeviceRequest       ,  // IRP_MJ_QUERY_VOLUME_INFORMATION
		IoInvalidDeviceRequest,         // IRP_MJ_DIRECTORY_CONTROL
		IoInvalidDeviceRequest,         // IRP_MJ_FILE_SYSTEM_CONTROL
		RawIrpDeviceControl,            // IRP_MJ_DEVICE_CONTROL
		IoInvalidDeviceRequest,         // IRP_MJ_INTERNAL_DEVICE_CONTROL
		IoInvalidDeviceRequest,         // IRP_MJ_SHUTDOWN
		RawIrpCleanup,                  // IRP_MJ_CLEANUP
	}
};


static VOID RawVolumeLockExclusive(PRAW_VOLUME_EXTENSION VolumeExtension)
{
	KeEnterCriticalRegion();
	ExAcquireReadWriteLockExclusive(&VolumeExtension->VolumeMutex);
}

static VOID RawVolumeLockShared(PRAW_VOLUME_EXTENSION VolumeExtension)
{
	KeEnterCriticalRegion();
	ExAcquireReadWriteLockShared(&VolumeExtension->VolumeMutex);
}

static VOID RawVolumeUnlock(PRAW_VOLUME_EXTENSION VolumeExtension)
{
	ExReleaseReadWriteLock(&VolumeExtension->VolumeMutex);
	KeLeaveCriticalRegion();
}

static NTSTATUS RawCompleteRequest(PIRP Irp, NTSTATUS Status, PRAW_VOLUME_EXTENSION VolumeExtension)
{
	RawVolumeUnlock(VolumeExtension);
	Irp->IoStatus.Status = Status;
	IofCompleteRequest(Irp, PRIORITY_BOOST_IO);
	return Status;
}

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
		RIP_API_FMT("Unimplemented DeviceType %u", DeviceObject->DeviceType);

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

static NTSTATUS XBOXAPI RawCompletionRoutine(DEVICE_OBJECT *DeviceObject, IRP *Irp, PVOID Context)
{
	// This is called from IofCompleteRequest while holding the raw volume lock. Note that IrpStackPointer points again to the PIO_STACK_LOCATION of the raw driver
	// because it calls IoGetCurrentIrpStackLocation after having incremented the IrpStackPointer

	if (Irp->PendingReturned) {
		IoMarkIrpPending(Irp);
	}

	// Read and write requests are completed directly in the raw driver, without having to call a lower level driver
	PIO_STACK_LOCATION IrpStackPointer = IoGetCurrentIrpStackLocation(Irp);
	assert((IrpStackPointer->MajorFunction != IRP_MJ_READ) && (IrpStackPointer->MajorFunction != IRP_MJ_READ));

	RawVolumeUnlock((PRAW_VOLUME_EXTENSION)DeviceObject->DeviceExtension);

	return STATUS_SUCCESS;
}

static NTSTATUS XBOXAPI RawIrpCreate(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	PRAW_VOLUME_EXTENSION VolumeExtension = (PRAW_VOLUME_EXTENSION)DeviceObject->DeviceExtension;
	RawVolumeLockExclusive(VolumeExtension);

	PIO_STACK_LOCATION IrpStackPointer = IoGetCurrentIrpStackLocation(Irp);
	POBJECT_STRING RemainingName = IrpStackPointer->Parameters.Create.RemainingName;
	PFILE_OBJECT FileObject = IrpStackPointer->FileObject;
	PFILE_OBJECT RelatedFileObject = FileObject->RelatedFileObject;
	ACCESS_MASK DesiredAccess = IrpStackPointer->Parameters.Create.DesiredAccess;
	USHORT ShareAccess = IrpStackPointer->Parameters.Create.ShareAccess;
	ULONG Disposition = (IrpStackPointer->Parameters.Create.Options >> 24) & 0xFF;
	ULONG CreateOptions = IrpStackPointer->Parameters.Create.Options & 0xFFFFFF;

	// Files and directories don't exist on a raw fs, so all relative operations are not supported
	if (VolumeExtension->Dismounted) {
		return RawCompleteRequest(Irp, STATUS_VOLUME_DISMOUNTED, VolumeExtension);
	}

	if (RelatedFileObject) {
		return RawCompleteRequest(Irp, STATUS_INVALID_PARAMETER, VolumeExtension);
	}

	if (RemainingName->Length) {
		return RawCompleteRequest(Irp, STATUS_OBJECT_PATH_NOT_FOUND, VolumeExtension);
	}

	if ((Disposition != FILE_OPEN) && (Disposition != FILE_OPEN_IF)) {
		return RawCompleteRequest(Irp, STATUS_ACCESS_DENIED, VolumeExtension);
	}

	if (CreateOptions & FILE_DIRECTORY_FILE) {
		return RawCompleteRequest(Irp, STATUS_NOT_A_DIRECTORY, VolumeExtension);
	}

	if (VolumeExtension->ShareAccess.OpenCount == 0) {
		// The requested volume is already open, but without any users to it, set the sharing permissions
		IoSetShareAccess(DesiredAccess, ShareAccess, FileObject, &VolumeExtension->ShareAccess);
	}
	else {
		// The requested volume is already open, check the share permissions to see if we can open it again
		if (NTSTATUS Status = IoCheckShareAccess(DesiredAccess, ShareAccess, FileObject, &VolumeExtension->ShareAccess, TRUE); !NT_SUCCESS(Status)) {
			return RawCompleteRequest(Irp, Status, VolumeExtension);
		}
	}

	// No host I/O required because we can use the special device handles to read raw bytes from the devices
	FileObject->Flags |= FO_NO_INTERMEDIATE_BUFFERING;
	Irp->IoStatus.Information = FILE_OPENED;

	return RawCompleteRequest(Irp, STATUS_SUCCESS, VolumeExtension);
}

static NTSTATUS XBOXAPI RawIrpClose(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	PRAW_VOLUME_EXTENSION VolumeExtension = (PRAW_VOLUME_EXTENSION)DeviceObject->DeviceExtension;
	RawVolumeLockExclusive(VolumeExtension);

	// We never close the device host handle, so there's no host I/O to do here

	if ((VolumeExtension->ShareAccess.OpenCount == 0) && VolumeExtension->Dismounted) {
		RawVolumeUnlock(VolumeExtension);
		RawDeleteVolume(DeviceObject);
	}
	else {
		RawVolumeUnlock(VolumeExtension);
	}

	Irp->IoStatus.Status = STATUS_SUCCESS;
	IofCompleteRequest(Irp, PRIORITY_BOOST_IO);

	return STATUS_SUCCESS;
}

static NTSTATUS XBOXAPI RawIrpRead(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	PRAW_VOLUME_EXTENSION VolumeExtension = (PRAW_VOLUME_EXTENSION)DeviceObject->DeviceExtension;
	RawVolumeLockShared(VolumeExtension);

	PIO_STACK_LOCATION IrpStackPointer = IoGetCurrentIrpStackLocation(Irp);
	PFILE_OBJECT FileObject = IrpStackPointer->FileObject;
	ULONG Length = IrpStackPointer->Parameters.Read.Length;
	LARGE_INTEGER FileOffset = IrpStackPointer->Parameters.Read.ByteOffset;
	PVOID Buffer = Irp->UserBuffer;
	PDEVICE_OBJECT TargetDeviceObject = VolumeExtension->TargetDeviceObject;

	switch (TargetDeviceObject->DeviceType)
	{
	default:
	case FILE_DEVICE_MEDIA_BOARD: // TODO
		RIP_API_FMT("Unimplemented DeviceType %u", TargetDeviceObject->DeviceType);

	case FILE_DEVICE_DISK:
		break;
	}

	if (VolumeExtension->Dismounted) {
		return RawCompleteRequest(Irp, STATUS_VOLUME_DISMOUNTED, VolumeExtension);
	}

	// Don't allow to operate on the device after all its handles are closed
	if (FileObject->Flags & FO_CLEANUP_COMPLETE) {
		return RawCompleteRequest(Irp, STATUS_FILE_CLOSED, VolumeExtension);
	}

	if (FileObject->Flags & FO_NO_INTERMEDIATE_BUFFERING) {
		// NOTE: for the HDD, SectorSize is 512
		if ((FileOffset.LowPart & (DeviceObject->SectorSize - 1)) || (Length & (DeviceObject->SectorSize - 1))) {
			return RawCompleteRequest(Irp, STATUS_INVALID_PARAMETER, VolumeExtension);
		}
	}

	if (Length == 0) {
		Irp->IoStatus.Information = 0;
		return RawCompleteRequest(Irp, STATUS_SUCCESS, VolumeExtension);
	}

	DWORD PartitionNumber = PIDE_DISK_EXTENSION(TargetDeviceObject->DeviceExtension)->PartitionInformation.PartitionNumber;

	IoInfoBlock InfoBlock = SubmitIoRequestToHost(
		DEV_TYPE(DEV_PARTITION0 + PartitionNumber) | IoRequestType::Read,
		FileOffset.LowPart,
		Length,
		(ULONG_PTR)Buffer,
		DEV_PARTITION0 + PartitionNumber
	);

	NTSTATUS Status = HostToNtStatus(InfoBlock.Status);
	if (Status == STATUS_PENDING) {
		// Should not happen right now, because RetrieveIoRequestFromHost is always synchronous
		RIP_API_MSG("Asynchronous IO is not supported");
	}
	else if (NT_SUCCESS(Status)) {
		if (FileObject->Flags & FO_SYNCHRONOUS_IO) {
			// NOTE: FileObject->CurrentByteOffset.QuadPart must be updated atomically because RawIrpRead acquires a shared lock, which means it can be concurrently updated
			atomic_add64(&FileObject->CurrentByteOffset.QuadPart, InfoBlock.Info);
		}
	}

	Irp->IoStatus.Information = InfoBlock.Info;
	return RawCompleteRequest(Irp, Status, VolumeExtension);
}

static NTSTATUS XBOXAPI RawIrpWrite(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	PRAW_VOLUME_EXTENSION VolumeExtension = (PRAW_VOLUME_EXTENSION)DeviceObject->DeviceExtension;
	RawVolumeLockExclusive(VolumeExtension);

	PIO_STACK_LOCATION IrpStackPointer = IoGetCurrentIrpStackLocation(Irp);
	PFILE_OBJECT FileObject = IrpStackPointer->FileObject;
	ULONG Length = IrpStackPointer->Parameters.Write.Length;
	LARGE_INTEGER FileOffset = IrpStackPointer->Parameters.Write.ByteOffset;
	PVOID Buffer = Irp->UserBuffer;
	PDEVICE_OBJECT TargetDeviceObject = VolumeExtension->TargetDeviceObject;

	switch (TargetDeviceObject->DeviceType)
	{
	default:
	case FILE_DEVICE_MEDIA_BOARD: // TODO
		RIP_API_FMT("Unimplemented DeviceType %u", TargetDeviceObject->DeviceType);

	case FILE_DEVICE_DISK:
		break;
	}

	if (VolumeExtension->Dismounted) {
		return RawCompleteRequest(Irp, STATUS_VOLUME_DISMOUNTED, VolumeExtension);
	}

	// Don't allow to operate on the device after all its handles are closed
	if (FileObject->Flags & FO_CLEANUP_COMPLETE) {
		return RawCompleteRequest(Irp, STATUS_FILE_CLOSED, VolumeExtension);
	}

	// Cannot write at the end of a device
	if (FileOffset.QuadPart == -1) {
		return RawCompleteRequest(Irp, STATUS_INVALID_PARAMETER, VolumeExtension);
	}

	if (FileObject->Flags & FO_NO_INTERMEDIATE_BUFFERING) {
		// NOTE: for the HDD, SectorSize is 512
		if ((FileOffset.LowPart & (DeviceObject->SectorSize - 1)) || (Length & (DeviceObject->SectorSize - 1))) {
			return RawCompleteRequest(Irp, STATUS_INVALID_PARAMETER, VolumeExtension);
		}
	}

	if (Length == 0) {
		Irp->IoStatus.Information = 0;
		return RawCompleteRequest(Irp, STATUS_SUCCESS, VolumeExtension);
	}

	DWORD PartitionNumber = PIDE_DISK_EXTENSION(TargetDeviceObject->DeviceExtension)->PartitionInformation.PartitionNumber;

	IoInfoBlock InfoBlock = SubmitIoRequestToHost(
		DEV_TYPE(DEV_PARTITION0 + PartitionNumber) | IoRequestType::Write,
		FileOffset.LowPart,
		Length,
		(ULONG_PTR)Buffer,
		DEV_PARTITION0 + PartitionNumber
	);

	NTSTATUS Status = HostToNtStatus(InfoBlock.Status);
	if (Status == STATUS_PENDING) {
		// Should not happen right now, because RetrieveIoRequestFromHost is always synchronous
		RIP_API_MSG("Asynchronous IO is not supported");
	}
	else if (NT_SUCCESS(Status)) {
		if (FileObject->Flags & FO_SYNCHRONOUS_IO) {
			FileObject->CurrentByteOffset.QuadPart += InfoBlock.Info;
		}
	}

	Irp->IoStatus.Information = InfoBlock.Info;
	return RawCompleteRequest(Irp, Status, VolumeExtension);
}

NTSTATUS XBOXAPI RawIrpDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	PRAW_VOLUME_EXTENSION VolumeExtension = (PRAW_VOLUME_EXTENSION)DeviceObject->DeviceExtension;
	RawVolumeLockShared(VolumeExtension);

	if (VolumeExtension->Dismounted) {
		return RawCompleteRequest(Irp, STATUS_VOLUME_DISMOUNTED, VolumeExtension);
	}

	PDEVICE_OBJECT TargetDeviceObject = VolumeExtension->TargetDeviceObject;

	if (TargetDeviceObject->DeviceType == FILE_DEVICE_MEDIA_BOARD) { // TODO
		RIP_API_FMT("Unimplemented DeviceType %u", TargetDeviceObject->DeviceType);
	}

	IoCopyCurrentIrpStackLocationToNext(Irp);
	IoSetCompletionRoutine(Irp, RawCompletionRoutine, nullptr, TRUE, TRUE, TRUE);

	NTSTATUS Status = IofCallDriver(VolumeExtension->TargetDeviceObject, Irp); // invokes HddIrpDeviceControl for HDD

	KeLeaveCriticalRegion();

	return Status;
}

static NTSTATUS XBOXAPI RawIrpCleanup(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	// This is called when the last open handle to a file is closed by NtClose

	PRAW_VOLUME_EXTENSION VolumeExtension = (PRAW_VOLUME_EXTENSION)DeviceObject->DeviceExtension;
	RawVolumeLockExclusive(VolumeExtension);

	PIO_STACK_LOCATION IrpStackPointer = IoGetCurrentIrpStackLocation(Irp);
	PFILE_OBJECT FileObject = IrpStackPointer->FileObject;
	IoRemoveShareAccess(FileObject, &VolumeExtension->ShareAccess);

	// Obviously, we can't delete a device, so there's no host I/O to do here
	FileObject->Flags |= FO_CLEANUP_COMPLETE;

	return RawCompleteRequest(Irp, STATUS_SUCCESS, VolumeExtension);
}
