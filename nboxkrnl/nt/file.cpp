/*
 * ergo720                Copyright (c) 2023
 */

#include "nt.hpp"
#include "ex.hpp"
#include "rtl.hpp"


EXPORTNUM(190) NTSTATUS XBOXAPI NtCreateFile
(
	PHANDLE FileHandle,
	ACCESS_MASK DesiredAccess,
	POBJECT_ATTRIBUTES ObjectAttributes,
	PIO_STATUS_BLOCK IoStatusBlock,
	PLARGE_INTEGER AllocationSize,
	ULONG FileAttributes,
	ULONG ShareAccess,
	ULONG CreateDisposition,
	ULONG CreateOptions
)
{
	return IoCreateFile(FileHandle, DesiredAccess, ObjectAttributes, IoStatusBlock, AllocationSize, FileAttributes, ShareAccess, CreateDisposition, CreateOptions, 0);
}

EXPORTNUM(202) NTSTATUS XBOXAPI NtOpenFile
(
	PHANDLE FileHandle,
	ACCESS_MASK DesiredAccess,
	POBJECT_ATTRIBUTES ObjectAttributes,
	PIO_STATUS_BLOCK IoStatusBlock,
	ULONG ShareAccess,
	ULONG OpenOptions
)
{
	return IoCreateFile(FileHandle, DesiredAccess, ObjectAttributes, IoStatusBlock, nullptr, 0, ShareAccess, FILE_OPEN, OpenOptions, 0);
}

EXPORTNUM(211) NTSTATUS XBOXAPI NtQueryInformationFile
(
	HANDLE FileHandle,
	PIO_STATUS_BLOCK IoStatusBlock,
	PVOID FileInformation,
	ULONG Length,
	FILE_INFORMATION_CLASS FileInformationClass
)
{
	if (((ULONG)FileInformationClass >= FileMaximumInformation) || (IopQueryOperationLength[FileInformationClass] == 0)) {
		return STATUS_INVALID_INFO_CLASS;
	}

	if (Length < (ULONG)IopQueryOperationLength[FileInformationClass]) {
		return STATUS_INFO_LENGTH_MISMATCH;
	}

	PFILE_OBJECT FileObject;
	NTSTATUS Status = ObReferenceObjectByHandle(FileHandle, &IoFileObjectType, (PVOID *)&FileObject);
	if (!NT_SUCCESS(Status)) {
		return Status;
	}

	if ((IopQueryOperationAccess[FileInformationClass] & FILE_READ_DATA) && !FileObject->ReadAccess) {
		ObfDereferenceObject(FileObject);
		return STATUS_ACCESS_DENIED;
	}

	BOOLEAN IsSynchronousIo;
	if (FileObject->Flags & FO_SYNCHRONOUS_IO) {
		IopAcquireSynchronousFileLock(FileObject);

		if (FileInformationClass == FilePositionInformation) {
			PFILE_POSITION_INFORMATION(FileInformation)->CurrentByteOffset = FileObject->CurrentByteOffset;
			IoStatusBlock->Information = sizeof(FILE_POSITION_INFORMATION);
			IoStatusBlock->Status = STATUS_SUCCESS;
			IopReleaseSynchronousFileLock(FileObject);
			ObfDereferenceObject(FileObject);
			return STATUS_SUCCESS;
		}
		IsSynchronousIo = TRUE;
	}
	else {
		IsSynchronousIo = FALSE;
		RIP_API_MSG("Asynchronous IO is not supported");
	}

	PDEVICE_OBJECT DeviceObject = FileObject->DeviceObject;
	PIRP Irp = IoAllocateIrp(DeviceObject->StackSize);
	if (!Irp) {
		return IopCleanupFailedIrpAllocation(FileObject, nullptr);
	}
	Irp->Tail.Overlay.OriginalFileObject = FileObject;
	Irp->Tail.Overlay.Thread = (PETHREAD)KeGetCurrentThread();
	Irp->UserBuffer = FileInformation;
	Irp->Flags = IRP_DEFER_IO_COMPLETION;
	if (IsSynchronousIo) {
		Irp->UserIosb = IoStatusBlock;
	}
	else {
		Irp->Flags |= IRP_SYNCHRONOUS_API;
	}

	PIO_STACK_LOCATION IrpStackPointer = IoGetNextIrpStackLocation(Irp);
	IrpStackPointer->MajorFunction = IRP_MJ_QUERY_INFORMATION;
	IrpStackPointer->FileObject = FileObject;
	IrpStackPointer->Parameters.QueryFile.Length = Length;
	IrpStackPointer->Parameters.QueryFile.FileInformationClass = FileInformationClass;

	FileObject->Event.Header.SignalState = 0;

	IopQueueThreadIrp(Irp);
	Status = IofCallDriver(DeviceObject, Irp);

	if (!NT_ERROR(Status)) {
		IoStatusBlock->Information = Irp->IoStatus.Information;
		IoStatusBlock->Status = Irp->IoStatus.Status;

		if (FileObject->Synchronize && ((FileObject->Flags & FO_SYNCHRONOUS_IO) || (Irp->Flags & IRP_SYNCHRONOUS_API))) {
			KeSetEvent(&FileObject->Event, 0, FALSE);
		}
	}

	KIRQL OldIrql = KfRaiseIrql(APC_LEVEL);
	IopDequeueThreadIrp(Irp);
	IoFreeIrp(Irp);
	KfLowerIrql(OldIrql);
	if (IsSynchronousIo) {
		IopReleaseSynchronousFileLock(FileObject);
	}
	ObfDereferenceObject(FileObject);

	return Status;
}

EXPORTNUM(218) NTSTATUS XBOXAPI NtQueryVolumeInformationFile
(
	HANDLE FileHandle,
	PIO_STATUS_BLOCK IoStatusBlock,
	PFILE_FS_SIZE_INFORMATION FileInformation,
	ULONG Length,
	FS_INFORMATION_CLASS FileInformationClass
)
{
	if (((ULONG)FileInformationClass >= FileFsMaximumInformation) || (IopValidFsInformationQueries[FileInformationClass] == 0)) {
		return STATUS_INVALID_INFO_CLASS;
	}

	if (Length < (ULONG)IopValidFsInformationQueries[FileInformationClass]) {
		return STATUS_INFO_LENGTH_MISMATCH;
	}

	PFILE_OBJECT FileObject;
	NTSTATUS Status = ObReferenceObjectByHandle(FileHandle, &IoFileObjectType, (PVOID *)&FileObject);
	if (!NT_SUCCESS(Status)) {
		return Status;
	}

	if ((IopQueryFsOperationAccess[FileInformationClass] & FILE_READ_DATA) && !FileObject->ReadAccess) {
		ObfDereferenceObject(FileObject);
		return STATUS_ACCESS_DENIED;
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
		return IopCleanupFailedIrpAllocation(FileObject, nullptr);
	}
	Irp->Tail.Overlay.OriginalFileObject = FileObject;
	Irp->Tail.Overlay.Thread = (PETHREAD)KeGetCurrentThread();
	Irp->UserBuffer = FileInformation;
	if (IsSynchronousIo) {
		Irp->UserIosb = IoStatusBlock;
	}
	Irp->Flags |= (IRP_SYNCHRONOUS_API | IRP_DEFER_IO_COMPLETION);

	PIO_STACK_LOCATION IrpStackPointer = IoGetNextIrpStackLocation(Irp);
	IrpStackPointer->MajorFunction = IRP_MJ_QUERY_VOLUME_INFORMATION;
	IrpStackPointer->FileObject = FileObject;
	IrpStackPointer->Parameters.QueryVolume.Length = Length;
	IrpStackPointer->Parameters.QueryVolume.FsInformationClass = FileInformationClass;

	FileObject->Event.Header.SignalState = 0;

	return IopSynchronousService(DeviceObject, Irp, FileObject, TRUE, IsSynchronousIo);
}

EXPORTNUM(219) DLLEXPORT NTSTATUS XBOXAPI NtReadFile
(
	HANDLE FileHandle,
	HANDLE Event,
	PIO_APC_ROUTINE ApcRoutine,
	PVOID ApcContext,
	PIO_STATUS_BLOCK IoStatusBlock,
	PVOID Buffer,
	ULONG Length,
	PLARGE_INTEGER ByteOffset
)
{
	PFILE_OBJECT FileObject;
	NTSTATUS Status = ObReferenceObjectByHandle(FileHandle, &IoFileObjectType, (PVOID *)&FileObject);
	if (!NT_SUCCESS(Status)) {
		return Status;
	}

	if (!FileObject->ReadAccess) {
		ObfDereferenceObject(FileObject);
		return STATUS_ACCESS_DENIED;
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

	LARGE_INTEGER FileOffset;
	if ((FileObject->Flags & FO_SYNCHRONOUS_IO) && (!ByteOffset || ((ByteOffset->HighPart == -1) && (ByteOffset->LowPart == FILE_USE_FILE_POINTER_POSITION)))) {
		FileOffset.QuadPart = FileObject->CurrentByteOffset.QuadPart;
	}
	else {
		if (!ByteOffset || (ByteOffset->QuadPart < 0)) {
			ObfDereferenceObject(FileObject);
			if (Event) {
				ObfDereferenceObject(Event);
			}
			return STATUS_INVALID_PARAMETER;
		}
		FileOffset.QuadPart = ByteOffset->QuadPart;
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
	Irp->UserBuffer = Buffer;
	Irp->UserEvent = UserEvent;
	if (IsSynchronousIo) {
		Irp->UserIosb = IoStatusBlock;
	}
	Irp->Flags |= (IRP_READ_OPERATION | IRP_DEFER_IO_COMPLETION);

	PIO_STACK_LOCATION IrpStackPointer = IoGetNextIrpStackLocation(Irp);
	IrpStackPointer->MajorFunction = IRP_MJ_READ;
	IrpStackPointer->FileObject = FileObject;
	IrpStackPointer->Parameters.Read.Length = Length;
	IrpStackPointer->Parameters.Read.ByteOffset = FileOffset;

	FileObject->Event.Header.SignalState = 0;

	return IopSynchronousService(DeviceObject, Irp, FileObject, TRUE, IsSynchronousIo);
}

EXPORTNUM(232) DLLEXPORT VOID XBOXAPI NtUserIoApcDispatcher
(
	PVOID ApcContext,
	PIO_STATUS_BLOCK IoStatusBlock,
	ULONG Reserved
)
{
	RIP_UNIMPLEMENTED();
}

EXPORTNUM(236) NTSTATUS XBOXAPI NtWriteFile
(
	HANDLE FileHandle,
	HANDLE Event,
	PIO_APC_ROUTINE ApcRoutine,
	PVOID ApcContext,
	PIO_STATUS_BLOCK IoStatusBlock,
	PVOID Buffer,
	ULONG Length,
	PLARGE_INTEGER ByteOffset
)
{
	PFILE_OBJECT FileObject;
	NTSTATUS Status = ObReferenceObjectByHandle(FileHandle, &IoFileObjectType, (PVOID *)&FileObject);
	if (!NT_SUCCESS(Status)) {
		return Status;
	}

	if (!FileObject->WriteAccess) {
		ObfDereferenceObject(FileObject);
		return STATUS_ACCESS_DENIED;
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

	LARGE_INTEGER FileOffset;
	if ((FileObject->Flags & FO_APPEND_ONLY) || (ByteOffset && (ByteOffset->HighPart == -1) && (ByteOffset->LowPart == FILE_WRITE_TO_END_OF_FILE))) {
		FileOffset.QuadPart = -1;
	}
	else if ((FileObject->Flags & FO_SYNCHRONOUS_IO) && (!ByteOffset || ((ByteOffset->HighPart == -1) && (ByteOffset->LowPart == FILE_USE_FILE_POINTER_POSITION)))) {
		FileOffset.QuadPart = FileObject->CurrentByteOffset.QuadPart;
	}
	else {
		if (!ByteOffset || (ByteOffset->QuadPart < 0)) {
			ObfDereferenceObject(FileObject);
			if (Event) {
				ObfDereferenceObject(Event);
			}
			return STATUS_INVALID_PARAMETER;
		}
		FileOffset.QuadPart = ByteOffset->QuadPart;
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
	Irp->UserBuffer = Buffer;
	Irp->UserEvent = UserEvent;
	if (IsSynchronousIo) {
		Irp->UserIosb = IoStatusBlock;
	}
	Irp->Flags |= (IRP_WRITE_OPERATION | IRP_DEFER_IO_COMPLETION);

	PIO_STACK_LOCATION IrpStackPointer = IoGetNextIrpStackLocation(Irp);
	IrpStackPointer->MajorFunction = IRP_MJ_WRITE;
	IrpStackPointer->FileObject = FileObject;
	IrpStackPointer->Parameters.Write.Length = Length;
	IrpStackPointer->Parameters.Write.ByteOffset = FileOffset;

	FileObject->Event.Header.SignalState = 0;

	return IopSynchronousService(DeviceObject, Irp, FileObject, TRUE, IsSynchronousIo);
}
