/*
 * ergo720                Copyright (c) 2023
 */

#include "iop.hpp"
#include "hdd\fatx.hpp"
#include "cdrom\xdvdfs.hpp"
#include "raw/raw.hpp"
#include "nt.hpp"
#include "string.h"


const UCHAR IopValidFsInformationQueries[] = {
  0,
  sizeof(FILE_FS_VOLUME_INFORMATION),    // 1 FileFsVolumeInformation
  0,                                     // 2 FileFsLabelInformation
  sizeof(FILE_FS_SIZE_INFORMATION),      // 3 FileFsSizeInformation
  sizeof(FILE_FS_DEVICE_INFORMATION),    // 4 FileFsDeviceInformation
  sizeof(FILE_FS_ATTRIBUTE_INFORMATION), // 5 FileFsAttributeInformation
  sizeof(FILE_FS_CONTROL_INFORMATION),   // 6 FileFsControlInformation
  sizeof(FILE_FS_FULL_SIZE_INFORMATION), // 7 FileFsFullSizeInformation
  sizeof(FILE_FS_OBJECTID_INFORMATION),  // 8 FileFsObjectIdInformation
  0xff                                   // 9 FileFsMaximumInformation
};

const ULONG IopQueryFsOperationAccess[] = {
   0,
   0,              // 1 FileFsVolumeInformation [any access to file or volume]
   0,              // 2 FileFsLabelInformation [query is invalid]
   0,              // 3 FileFsSizeInformation [any access to file or volume]
   0,              // 4 FileFsDeviceInformation [any access to file or volume]
   0,              // 5 FileFsAttributeInformation [any access to file or vol]
   FILE_READ_DATA, // 6 FileFsControlInformation [vol read access]
   0,              // 7 FileFsFullSizeInformation [any access to file or volume]
   0,              // 8 FileFsObjectIdInformation [any access to file or volume]
   0xffffffff      // 9 FileFsMaximumInformation
};

const UCHAR IopQueryOperationLength[] = {
	0,
	0,
	0,
	0,
	sizeof(FILE_BASIC_INFORMATION),
	sizeof(FILE_STANDARD_INFORMATION),
	sizeof(FILE_INTERNAL_INFORMATION),
	sizeof(FILE_EA_INFORMATION),
	0,
	sizeof(FILE_NAME_INFORMATION),
	0,
	0,
	0,
	0,
	sizeof(FILE_POSITION_INFORMATION),
	0,
	sizeof(FILE_MODE_INFORMATION),
	sizeof(FILE_ALIGNMENT_INFORMATION),
	sizeof(FILE_ALL_INFORMATION),
	0,
	0,
	sizeof(FILE_NAME_INFORMATION),
	sizeof(FILE_STREAM_INFORMATION),
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	sizeof(FILE_NETWORK_OPEN_INFORMATION),
	sizeof(FILE_ATTRIBUTE_TAG_INFORMATION),
	0,
	0xFF
};

const ULONG IopQueryOperationAccess[] = {
	0,
	0,
	0,
	0,
	FILE_READ_ATTRIBUTES,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	FILE_READ_EA,
	0,
	0,
	FILE_READ_ATTRIBUTES,
	0,
	0,
	0,
	0,
	FILE_READ_ATTRIBUTES,
	FILE_READ_ATTRIBUTES,
	FILE_READ_ATTRIBUTES,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	FILE_READ_ATTRIBUTES,
	FILE_READ_ATTRIBUTES,
	0,
	0XFFFFFFFF
};

static VOID XBOXAPI IopRundownRoutine(PKAPC Apc)
{
	IoFreeIrp(CONTAINING_RECORD(Apc, IRP, Tail.Apc));
}

static VOID XBOXAPI IopCompletionRoutine(PKAPC Apc, PKNORMAL_ROUTINE *NormalRoutine, PVOID *NormalContext, PVOID *SystemArgument1, PVOID *SystemArgument2)
{
	IopRundownRoutine(Apc);
}

NTSTATUS IopMountDevice(PDEVICE_OBJECT DeviceObject, BOOLEAN AllowRawAccess)
{
	// Thread safety: acquire a device-specific lock
	KeWaitForSingleObject(&DeviceObject->DeviceLock, Executive, KernelMode, FALSE, nullptr);

	NTSTATUS Status = STATUS_SUCCESS;
	if (DeviceObject->MountedOrSelfDevice == nullptr) {
		if ((DeviceObject->Flags & DO_RAW_MOUNT_ONLY) && AllowRawAccess) {
			// This should only happen for the partition zero of the HDD or the media board
			Status = RawCreateVolume(DeviceObject);
		}
		else {
			switch (DeviceObject->DeviceType)
			{
			case FILE_DEVICE_CD_ROM:
				// FIXME: this could also be a regular CD/DVD with UDF filesystem instead of XDVDFS
				Status = XdvdfsCreateVolume(DeviceObject);
				break;

			case FILE_DEVICE_DISK:
				Status = FatxCreateVolume(DeviceObject);
				break;

			case FILE_DEVICE_MEMORY_UNIT:
				RIP_API_MSG("Mounting MUs is not supported");

			case FILE_DEVICE_MEDIA_BOARD:
				RIP_API_MSG("Mounting the media board is not supported");

			default:
				Status = STATUS_UNRECOGNIZED_VOLUME;
			}
		}
	}

	KeSetEvent(&DeviceObject->DeviceLock, 0, FALSE);

	return Status;
}

VOID IopDereferenceDeviceObject(PDEVICE_OBJECT DeviceObject)
{
	KIRQL OldIrql = IoLock();

	--DeviceObject->ReferenceCount;

	if ((DeviceObject->ReferenceCount == 0) && (DeviceObject->DeletePending)) {
		IoUnlock(OldIrql);
		if (DeviceObject->DriverObject->DriverDeleteDevice) {
			DeviceObject->DriverObject->DriverDeleteDevice(DeviceObject);
		}
		else {
			ObfDereferenceObject(DeviceObject);
		}
	}
	else {
		IoUnlock(OldIrql);
	}
}

VOID IopQueueThreadIrp(PIRP Irp)
{
	KIRQL OldIrql = KfRaiseIrql(APC_LEVEL);
	InsertHeadList(&Irp->Tail.Overlay.Thread->IrpList, &Irp->ThreadListEntry);
	KfLowerIrql(OldIrql);
}

VOID IopDequeueThreadIrp(PIRP Irp)
{
	RemoveEntryList(&Irp->ThreadListEntry);
	InitializeListHead(&Irp->ThreadListEntry);
}

VOID ZeroIrpStackLocation(PIO_STACK_LOCATION IrpStackPointer)
{
	IrpStackPointer->MinorFunction = 0;
	IrpStackPointer->Flags = 0;
	IrpStackPointer->Control = 0;
	IrpStackPointer->Parameters.Others.Argument1 = 0;
	IrpStackPointer->Parameters.Others.Argument2 = 0;
	IrpStackPointer->Parameters.Others.Argument3 = 0;
	IrpStackPointer->Parameters.Others.Argument4 = 0;
	IrpStackPointer->FileObject = nullptr;
}

VOID IoMarkIrpPending(PIRP Irp)
{
	IoGetCurrentIrpStackLocation(Irp)->Control |= SL_PENDING_RETURNED;
}

PIO_STACK_LOCATION IoGetCurrentIrpStackLocation(PIRP Irp)
{
	return Irp->Tail.Overlay.CurrentStackLocation;
}

PIO_STACK_LOCATION IoGetNextIrpStackLocation(PIRP Irp)
{
	return Irp->Tail.Overlay.CurrentStackLocation - 1;
}

VOID IoCopyCurrentIrpStackLocationToNext(PIRP Irp)
{
	PIO_STACK_LOCATION IrpStackPointer = IoGetCurrentIrpStackLocation(Irp);
	PIO_STACK_LOCATION NextIrpStackPointer = IoGetNextIrpStackLocation(Irp);
	memcpy(NextIrpStackPointer, IrpStackPointer, offsetof(IO_STACK_LOCATION, CompletionRoutine));
	NextIrpStackPointer->Control = 0;
}

VOID IoSetCompletionRoutine(PIRP Irp, PIO_COMPLETION_ROUTINE CompletionRoutine, PVOID Context, BOOLEAN InvokeOnSuccess, BOOLEAN InvokeOnError, BOOLEAN InvokeOnCancel)
{
	PIO_STACK_LOCATION NextIrpStackPointer = IoGetNextIrpStackLocation(Irp);
	NextIrpStackPointer->CompletionRoutine = CompletionRoutine;
	NextIrpStackPointer->Context = Context;
	NextIrpStackPointer->Control = 0;
	if (InvokeOnSuccess) {
		NextIrpStackPointer->Control = SL_INVOKE_ON_SUCCESS;
	}
	if (InvokeOnError) {
		NextIrpStackPointer->Control |= SL_INVOKE_ON_ERROR;
	}
	if (InvokeOnCancel) {
		NextIrpStackPointer->Control |= SL_INVOKE_ON_CANCEL;
	}
}

VOID XBOXAPI IopCloseFile(PVOID Object, ULONG SystemHandleCount)
{
	if (SystemHandleCount == 1) {
		PFILE_OBJECT FileObject = (PFILE_OBJECT)Object;
		FileObject->Flags |= FO_HANDLE_CREATED;

		if (FileObject->Flags & FO_SYNCHRONOUS_IO) {
			IopAcquireSynchronousFileLock(FileObject);
		}

		FileObject->Event.Header.SignalState = 0;
		PDEVICE_OBJECT DeviceObject = FileObject->DeviceObject;
		PIRP Irp = IoAllocateIrpNoFail(DeviceObject->StackSize);
		Irp->Tail.Overlay.OriginalFileObject = FileObject;
		Irp->Tail.Overlay.Thread = (PETHREAD)KeGetCurrentThread();
		Irp->UserIosb = &Irp->IoStatus;
		Irp->Flags = IRP_SYNCHRONOUS_API | IRP_CLOSE_OPERATION;

		PIO_STACK_LOCATION IrpStackPointer = IoGetNextIrpStackLocation(Irp);
		IrpStackPointer->MajorFunction = IRP_MJ_CLEANUP;
		IrpStackPointer->FileObject = FileObject;

		IopQueueThreadIrp(Irp);

		IofCallDriver(DeviceObject, Irp);

		KIRQL OldIrql = KfRaiseIrql(APC_LEVEL);
		IopDequeueThreadIrp(Irp);
		KfLowerIrql(OldIrql);

		IoFreeIrp(Irp);

		if (FileObject->Flags & FO_SYNCHRONOUS_IO) {
			IopReleaseSynchronousFileLock(FileObject);
		}
	}
}

VOID XBOXAPI IopDeleteFile(PVOID Object)
{
	PFILE_OBJECT FileObject = (PFILE_OBJECT)Object;

	// The file won't have a DeviceObject only when a create/open request to it failed (IoParseDevice sets DeviceObject to nullptr and then calls ObfDereferenceObject
	// which triggers this function)
	if (FileObject->DeviceObject) {
		PDEVICE_OBJECT DeviceObject = FileObject->DeviceObject;

		// This happens when OB fails a create/open request in IoCreateFile
		if (!(FileObject->Flags & FO_HANDLE_CREATED)) {
			IopCloseFile(Object, 1);
		}

		if (FileObject->Flags & FO_SYNCHRONOUS_IO) {
			IopAcquireSynchronousFileLock(FileObject);
		}

		IO_STATUS_BLOCK IoStatus;
		PIRP Irp = IoAllocateIrpNoFail(DeviceObject->StackSize);
		PIO_STACK_LOCATION IrpStackPointer = IoGetNextIrpStackLocation(Irp);
		IrpStackPointer->MajorFunction = IRP_MJ_CLOSE;
		IrpStackPointer->FileObject = FileObject;
		Irp->UserIosb = &IoStatus;
		Irp->Tail.Overlay.OriginalFileObject = FileObject;
		Irp->Tail.Overlay.Thread = (PETHREAD)KeGetCurrentThread();
		Irp->Flags = IRP_CLOSE_OPERATION | IRP_SYNCHRONOUS_API;

		IopQueueThreadIrp(Irp);
		IofCallDriver(DeviceObject, Irp);

		KIRQL OldIrql = KfRaiseIrql(APC_LEVEL);
		IopDequeueThreadIrp(Irp);
		KfLowerIrql(OldIrql);

		IoFreeIrp(Irp);

		if (FileObject->CompletionContext) {
			ObfDereferenceObject(FileObject->CompletionContext->Port);
			ExFreePool(FileObject->CompletionContext);
		}

		IopDereferenceDeviceObject(DeviceObject);
	}
}

NTSTATUS XBOXAPI IopParseFile(PVOID ParseObject, POBJECT_TYPE ObjectType, ULONG Attributes, POBJECT_STRING CompleteName, POBJECT_STRING RemainingName,
	PVOID Context, PVOID *Object)
{
	// This function is supposed to be called for create request from NtCreateFile, so the context is an OPEN_PACKET
	POPEN_PACKET OpenPacket = (POPEN_PACKET)Context;

	// Sanity check: ensure that the request is really create
	if ((OpenPacket == nullptr) || ((OpenPacket->Type != IO_TYPE_OPEN_PACKET) || (OpenPacket->Size != sizeof(OPEN_PACKET)))) {
		return STATUS_OBJECT_TYPE_MISMATCH;
	}

	// IopParseFile is the ParseProcedure of IoFileObjectType, so ParseObject must be a PFILE_OBJECT
	PFILE_OBJECT FileObject = (PFILE_OBJECT)ParseObject;
	OpenPacket->RelatedFileObject = FileObject;

	// FileObject->DeviceObject is the device that "owns" the FileObject (e.g. for a file on the HDD, it will be a FatxDeviceObject)
	return IoParseDevice(FileObject->DeviceObject, ObjectType, Attributes, CompleteName, RemainingName, Context, Object);
}

VOID XBOXAPI IopCompleteRequest(PKAPC Apc, PKNORMAL_ROUTINE *NormalRoutine, PVOID *NormalContext, PVOID *SystemArgument1, PVOID *SystemArgument2)
{
	PIRP Irp = CONTAINING_RECORD(Apc, IRP, Tail.Apc);
	PFILE_OBJECT FileObject = (PFILE_OBJECT)*SystemArgument1;

	if (!NT_ERROR(Irp->IoStatus.Status)) {
		Irp->UserIosb->Information = Irp->IoStatus.Information;
		Irp->UserIosb->Status = Irp->IoStatus.Status;

		// If the original IO request supplied an event, signal it now. Also signal the FileObject's event too since the IO is completed now
		if (Irp->UserEvent) {
			KeSetEvent(Irp->UserEvent, 0, FALSE);
			ObfDereferenceObject(Irp->UserEvent);
			if (FileObject) {
				FileObject->FinalStatus = Irp->IoStatus.Status;
			}
		}
		else if (FileObject && FileObject->Synchronize && ((FileObject->Flags & FO_SYNCHRONOUS_IO) || (Irp->Flags & IRP_SYNCHRONOUS_API))) {
			KeSetEvent(&FileObject->Event, 0, FALSE);
			FileObject->FinalStatus = Irp->IoStatus.Status;
		}

		IopDequeueThreadIrp(Irp);

		// If the original IO request supplied an APC, execute it now
		if (Irp->Overlay.AsynchronousParameters.UserApcRoutine) {
			KeInitializeApc(&Irp->Tail.Apc,
				KeGetCurrentThread(),
				IopCompletionRoutine,
				(PKRUNDOWN_ROUTINE)IopRundownRoutine,
				(PKNORMAL_ROUTINE)Irp->Overlay.AsynchronousParameters.UserApcRoutine,
				Irp->Overlay.AsynchronousParameters.UserApcRoutine == NtUserIoApcDispatcher ? UserMode : KernelMode,
				Irp->Overlay.AsynchronousParameters.UserApcContext);

			KeInsertQueueApc(&Irp->Tail.Apc, Irp->UserIosb, nullptr, 2);
		}
		else {
			IoFreeIrp(Irp);
		}
	}
	else {
		// If the IO failed, don't signal the event but only dereference it and then destroy the irp
		if (Irp->UserEvent) {
			ObfDereferenceObject(Irp->UserEvent);
		}

		IopDequeueThreadIrp(Irp);
		IoFreeIrp(Irp);
	}

	if (FileObject) {
		ObfDereferenceObject(FileObject);
	}
}

VOID IopDropIrp(PIRP Irp, PFILE_OBJECT FileObject)
{
	if (Irp->UserEvent && FileObject && !(Irp->Flags & IRP_SYNCHRONOUS_API)) {
		ObfDereferenceObject(Irp->UserEvent);
	}

	if (FileObject && !(Irp->Flags & IRP_CREATE_OPERATION)) {
		ObfDereferenceObject(FileObject);
	}

	IoFreeIrp(Irp);
}

NTSTATUS IopSynchronousService(PDEVICE_OBJECT DeviceObject, PIRP Irp, PFILE_OBJECT FileObject, BOOLEAN DeferredIoCompletion, BOOLEAN SynchronousIo)
{
	IopQueueThreadIrp(Irp);

	NTSTATUS Status = IofCallDriver(DeviceObject, Irp);

	if (DeferredIoCompletion) {
		if (Status != STATUS_PENDING) {
			PKNORMAL_ROUTINE NormalRoutine;
			PVOID NormalContext;

			KIRQL OldIrql = KfRaiseIrql(APC_LEVEL);
			IopCompleteRequest(&Irp->Tail.Apc, &NormalRoutine, &NormalContext, (PVOID *)&FileObject, &NormalContext);
			KfLowerIrql(OldIrql);
		}
	}

	if (SynchronousIo) {
		if (Status == STATUS_PENDING) {
			KeWaitForSingleObject(&FileObject->Event, Executive, KernelMode, FALSE, nullptr);
			Status = FileObject->FinalStatus;
		}

		IopReleaseSynchronousFileLock(FileObject);
	}

	return Status;
}

VOID IopAcquireSynchronousFileLock(PFILE_OBJECT FileObject)
{
	if (InterlockedIncrement(&FileObject->LockCount)) {
		KeWaitForSingleObject(&FileObject->Lock, Executive, KernelMode, FALSE, nullptr);
	}
}

VOID IopReleaseSynchronousFileLock(PFILE_OBJECT FileObject)
{
	if (InterlockedDecrement(&FileObject->LockCount) >= 0) {
		KeSetEvent(&FileObject->Lock, 0, FALSE);
	}
}

NTSTATUS IopCleanupFailedIrpAllocation(PFILE_OBJECT FileObject, PKEVENT EventObject)
{
	if (EventObject) {
		ObfDereferenceObject(EventObject);
	}

	if (FileObject->Flags & FO_SYNCHRONOUS_IO) {
		IopReleaseSynchronousFileLock(FileObject);
	}

	ObfDereferenceObject(FileObject);

	return STATUS_INSUFFICIENT_RESOURCES;
}

NTSTATUS XBOXAPI IopControlFile
(
	HANDLE FileHandle,
	HANDLE Event,
	PIO_APC_ROUTINE ApcRoutine,
	PVOID ApcContext,
	PIO_STATUS_BLOCK IoStatusBlock,
	ULONG IoControlCode,
	PVOID InputBuffer,
	ULONG InputBufferLength,
	PVOID OutputBuffer,
	ULONG OutputBufferLength,
	ULONG IrpType
)
{
	// TODO: does this need to lock the pages of the OutputBuffer?

	PFILE_OBJECT FileObject;
	NTSTATUS Status = ObReferenceObjectByHandle(FileHandle, &IoFileObjectType, (PVOID *)&FileObject);
	if (!NT_SUCCESS(Status)) {
		return Status;
	}

	if (FileObject->CompletionContext && ApcRoutine) {
		ObfDereferenceObject(FileObject);
		return STATUS_INVALID_PARAMETER;
	}

	PKEVENT UserEvent = nullptr;
	if (Event) {
		NTSTATUS Status = ObReferenceObjectByHandle(Event, &ExEventObjectType, (PVOID *)&UserEvent);
		if (!NT_SUCCESS(Status)) {
			ObfDereferenceObject(FileObject);
			return Status;
		}
		UserEvent->Header.SignalState = 0;
	}

	BOOLEAN IsSynchronousIo;
	if (FileObject->Flags & FO_SYNCHRONOUS_IO) {
		IopAcquireSynchronousFileLock(FileObject);
		IsSynchronousIo = TRUE;
	}
	else {
		IsSynchronousIo = FALSE;
		RIP_API_MSG("Asynchronous IO is not supported");
	}

	PDEVICE_OBJECT DeviceObject = FileObject->DeviceObject;

	PIRP Irp = IoAllocateIrp(DeviceObject->StackSize);
	if (!Irp) {
		return IopCleanupFailedIrpAllocation(FileObject, UserEvent);
	}
	Irp->Tail.Overlay.OriginalFileObject = FileObject;
	Irp->Tail.Overlay.Thread = (PETHREAD)KeGetCurrentThread();
	Irp->Overlay.AsynchronousParameters.UserApcRoutine = ApcRoutine;
	Irp->Overlay.AsynchronousParameters.UserApcContext = ApcContext;
	Irp->UserBuffer = OutputBuffer;
	Irp->UserEvent = UserEvent;
	Irp->UserIosb = IoStatusBlock;
	Irp->Flags = (IrpType == IRP_MJ_FILE_SYSTEM_CONTROL) ? IRP_DEFER_IO_COMPLETION : 0;

	PIO_STACK_LOCATION IrpStackPointer = IoGetNextIrpStackLocation(Irp);
	IrpStackPointer->MajorFunction = IrpType;
	IrpStackPointer->FileObject = FileObject;
	IrpStackPointer->Parameters.DeviceIoControl.OutputBufferLength = OutputBufferLength;
	IrpStackPointer->Parameters.DeviceIoControl.InputBufferLength = InputBufferLength;
	IrpStackPointer->Parameters.DeviceIoControl.IoControlCode = IoControlCode;
	IrpStackPointer->Parameters.DeviceIoControl.InputBuffer = InputBuffer;

	FileObject->Event.Header.SignalState = 0;

	return IopSynchronousService(DeviceObject, Irp, FileObject, (IrpType == IRP_MJ_FILE_SYSTEM_CONTROL), IsSynchronousIo);
}

static VOID XBOXAPI IopLogDeviceType(DEVICE_OBJECT *DeviceObject, PCHAR &DriverName)
{
	switch (DeviceObject->DeviceType)
	{
	case FILE_DEVICE_CD_ROM:
		DriverName = (PCHAR)"dvd drive";
		break;

	case FILE_DEVICE_CD_ROM_FILE_SYSTEM:
		DriverName = (PCHAR)"dvd drive fs";
		break;

	case FILE_DEVICE_DISK:
		DriverName = (PCHAR)"hard disk";
		break;

	case FILE_DEVICE_DISK_FILE_SYSTEM:
		DriverName = (PCHAR)"hard disk fs";
		break;

	case FILE_DEVICE_MEMORY_UNIT:
		DriverName = (PCHAR)"memory unit";
		break;

	case FILE_DEVICE_MEDIA_BOARD:
		DriverName = (PCHAR)"media board";
		break;

	default:
		DriverName = (PCHAR)"unknown";
	}
}

static VOID XBOXAPI IopLogIrpType(IRP *Irp, PCHAR &IrpType)
{
	PIO_STACK_LOCATION IrpStackPointer = IoGetCurrentIrpStackLocation(Irp);
	switch (IrpStackPointer->MajorFunction)
	{
	case IRP_MJ_CREATE:
		IrpType = (PCHAR)"create";
		break;

	case IRP_MJ_CLOSE:
		IrpType = (PCHAR)"close";
		break;

	case IRP_MJ_READ:
		IrpType = (PCHAR)"read";
		break;

	case IRP_MJ_WRITE:
		IrpType = (PCHAR)"write";
		break;

	case IRP_MJ_QUERY_INFORMATION:
		IrpType = (PCHAR)"query information";
		break;

	case IRP_MJ_SET_INFORMATION:
		IrpType = (PCHAR)"set information";
		break;

	case IRP_MJ_FLUSH_BUFFERS:
		IrpType = (PCHAR)"flush buffers";
		break;

	case IRP_MJ_QUERY_VOLUME_INFORMATION:
		IrpType = (PCHAR)"query volume information";
		break;

	case IRP_MJ_DIRECTORY_CONTROL:
		IrpType = (PCHAR)"directory control";
		break;

	case IRP_MJ_FILE_SYSTEM_CONTROL:
		IrpType = (PCHAR)"file system control";
		break;

	case IRP_MJ_DEVICE_CONTROL:
		IrpType = (PCHAR)"device control";
		break;

	case IRP_MJ_INTERNAL_DEVICE_CONTROL:
		IrpType = (PCHAR)"internal device control";
		break;

	case IRP_MJ_SHUTDOWN:
		IrpType = (PCHAR)"shutdown";
		break;

	case IRP_MJ_CLEANUP:
		IrpType = (PCHAR)"cleanup";
		break;

	default:
		IrpType = (PCHAR)"unknown";
	}
}

VOID XBOXAPI IopUnimplementedDriverStartIo(DEVICE_OBJECT *DeviceObject, IRP *Irp)
{
	PCHAR DriverName, IrpType;
	IopLogDeviceType(DeviceObject, DriverName);
	IopLogIrpType(Irp, IrpType);
	RIP_API_FMT("Unimplemented driver start io routine of type \"%s\" from \"%s\" driver", IrpType, DriverName);
}

VOID XBOXAPI IopUnimplementedDriverDeleteDevice(DEVICE_OBJECT *DeviceObject)
{
	PCHAR DriverName;
	IopLogDeviceType(DeviceObject, DriverName);
	RIP_API_FMT("Unimplemented driver delete device routine from \"%s\" driver", DriverName);
}

NTSTATUS XBOXAPI IopUnimplementedDriverDismountVolume(DEVICE_OBJECT *DeviceObject)
{
	PCHAR DriverName;
	IopLogDeviceType(DeviceObject, DriverName);
	RIP_API_FMT("Unimplemented driver dismount volume routine from \"%s\" driver", DriverName);
}

NTSTATUS XBOXAPI IopUnimplementedDeviceRequest(DEVICE_OBJECT *DeviceObject, IRP *Irp)
{
	PCHAR DriverName, IrpType;
	IopLogDeviceType(DeviceObject, DriverName);
	IopLogIrpType(Irp, IrpType);
	RIP_API_FMT("Unimplemented device request \"%s\" from \"%s\" driver", IrpType, DriverName);
}
