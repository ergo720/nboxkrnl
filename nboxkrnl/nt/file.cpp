/*
 * ergo720                Copyright (c) 2023
 */

#include "nt.hpp"
#include "..\rtl\rtl.hpp"


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

	// Disabled because it can't work since IO is not storing the file permission accesses anywhere (yet)
#if ENABLE_FILE_PERMISSION_CHECKS
	if ((IopQueryFsOperationAccess[FileInformationClass] & FILE_READ_DATA) && !FileObject->ReadAccess) {
		ObfDereferenceObject(FileObject);
		return STATUS_ACCESS_DENIED;
}
#endif

	BOOLEAN IsSynchronousIo;
	if (FileObject->Flags & FO_SYNCHRONOUS_IO) {
		IopAcquireSynchronousFileLock(FileObject);
		IsSynchronousIo = TRUE;
	}
	else {
		IsSynchronousIo = FALSE;
		RIP_API_MSG("Asynchronous IO is not supported");
	}

	FileObject->Event.Header.SignalState = 0;
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
	Irp->Flags |= IRP_DEFER_IO_COMPLETION;

	PIO_STACK_LOCATION IrpStackPointer = IoGetNextIrpStackLocation(Irp);
	IrpStackPointer->MajorFunction = IRP_MJ_QUERY_VOLUME_INFORMATION;
	IrpStackPointer->FileObject = FileObject;
	IrpStackPointer->Parameters.QueryVolume.Length = Length;
	IrpStackPointer->Parameters.QueryVolume.FsInformationClass = FileInformationClass;

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
