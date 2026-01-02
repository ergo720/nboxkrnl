/*
 * ergo720                Copyright (c) 2023
 */

#include "ex.hpp"
#include "rtl.hpp"
#include "obp.hpp"
#include "nt.hpp"
#include "halp.hpp"
#include "cdrom\cdrom.hpp"
#include "hdd\hdd.hpp"
#include <string.h>


EXPORTNUM(70) OBJECT_TYPE IoDeviceObjectType = {
	ExAllocatePoolWithTag,
	ExFreePool,
	nullptr,
	nullptr,
	IoParseDevice,
	&ObpDefaultObject,
	'iveD'
};

EXPORTNUM(71) OBJECT_TYPE IoFileObjectType = {
	ExAllocatePoolWithTag,
	ExFreePool,
	IopCloseFile,
	IopDeleteFile,
	IopParseFile,
	(PVOID)offsetof(FILE_OBJECT, Event.Header),
	'eliF'
};

BOOLEAN IoInitSystem()
{
	for (unsigned i = 0; i < 8; ++i) {
		if (!NT_SUCCESS(HalpReadSMBusBlock(EEPROM_READ_ADDR, 32 * i, 32, (PBYTE)&CachedEeprom + i * 32))) {
			return FALSE;
		}
	}

	XboxFactoryGameRegion = CachedEeprom.EncryptedSettings.GameRegion;

	if (!HddInitDriver()) {
		return FALSE;
	}

	if (!CdromInitDriver()) {
		return FALSE;
	}

	return TRUE;
}

EXPORTNUM(59) PIRP XBOXAPI IoAllocateIrp
(
	CCHAR StackSize
)
{
	USHORT PacketSize = USHORT(sizeof(IRP) + StackSize * sizeof(IO_STACK_LOCATION));
	PIRP Irp = (PIRP)ExAllocatePoolWithTag(PacketSize, ' prI');
	if (Irp == nullptr) {
		return Irp;
	}

	IoInitializeIrp(Irp, PacketSize, StackSize);

	return Irp;
}

EXPORTNUM(63) NTSTATUS XBOXAPI IoCheckShareAccess
(
	ACCESS_MASK DesiredAccess,
	ULONG DesiredShareAccess,
	PFILE_OBJECT FileObject,
	PSHARE_ACCESS ShareAccess,
	BOOLEAN Update
)
{
	FileObject->ReadAccess = !!(DesiredAccess & (FILE_EXECUTE | FILE_READ_DATA));
	FileObject->WriteAccess = !!(DesiredAccess & (FILE_WRITE_DATA | FILE_APPEND_DATA));
	FileObject->DeleteAccess = !!(DesiredAccess & DELETE);
	FileObject->Synchronize = !!(DesiredAccess & SYNCHRONIZE);

	if (FileObject->ReadAccess || FileObject->WriteAccess || FileObject->DeleteAccess) {
		FileObject->SharedRead = !!(DesiredShareAccess & FILE_SHARE_READ);
		FileObject->SharedWrite = !!(DesiredShareAccess & FILE_SHARE_WRITE);
		FileObject->SharedDelete = !!(DesiredShareAccess & FILE_SHARE_DELETE);

		UCHAR OpenCount = ShareAccess->OpenCount;
		if ((OpenCount == 255) ||
			(FileObject->ReadAccess && (ShareAccess->SharedRead < OpenCount)) ||
			(FileObject->WriteAccess && (ShareAccess->SharedWrite < OpenCount)) ||
			(FileObject->DeleteAccess && (ShareAccess->SharedDelete < OpenCount)) ||
			((ShareAccess->ReadAccess != 0) && !FileObject->SharedRead) ||
			((ShareAccess->WriteAccess != 0) && !FileObject->SharedWrite) ||
			((ShareAccess->DeleteAccess != 0) && !FileObject->SharedDelete)
			) {
			return STATUS_SHARING_VIOLATION;
		}
		else if (Update) {
			++ShareAccess->OpenCount;
			ShareAccess->ReadAccess += FileObject->ReadAccess;
			ShareAccess->WriteAccess += FileObject->WriteAccess;
			ShareAccess->DeleteAccess += FileObject->DeleteAccess;
			ShareAccess->SharedRead += FileObject->SharedRead;
			ShareAccess->SharedWrite += FileObject->SharedWrite;
			ShareAccess->SharedDelete += FileObject->SharedDelete;
		}
	}

	return STATUS_SUCCESS;
}

EXPORTNUM(65) NTSTATUS XBOXAPI IoCreateDevice
(
	PDRIVER_OBJECT DriverObject,
	ULONG DeviceExtensionSize,
	PSTRING DeviceName,
	ULONG DeviceType,
	BOOLEAN Exclusive,
	PDEVICE_OBJECT *DeviceObject
)
{
	*DeviceObject = nullptr;

	ULONG Attributes = 0;
	if (DeviceName) {
		Attributes |= OBJ_PERMANENT;
	}
	if (Exclusive) {
		Attributes |= OBJ_EXCLUSIVE;
	}

	OBJECT_ATTRIBUTES ObjectAttributes;
	InitializeObjectAttributes(&ObjectAttributes, DeviceName, Attributes, nullptr);
	ULONG ObjectSize = sizeof(DEVICE_OBJECT) + DeviceExtensionSize;
	PDEVICE_OBJECT CreatedDeviceObject;
	NTSTATUS Status = ObCreateObject(&IoDeviceObjectType, &ObjectAttributes, ObjectSize, (PVOID *)&CreatedDeviceObject);

	if (!NT_SUCCESS(Status)) {
		return Status;
	}

	memset(CreatedDeviceObject, 0, ObjectSize);
	CreatedDeviceObject->Type = IO_TYPE_DEVICE;
	CreatedDeviceObject->Size = (USHORT)ObjectSize;
	CreatedDeviceObject->DeviceType = (UCHAR)DeviceType;

	if ((DeviceType == FILE_DEVICE_DISK) || (DeviceType == FILE_DEVICE_MEMORY_UNIT) || (DeviceType == FILE_DEVICE_CD_ROM) || (DeviceType == FILE_DEVICE_MEDIA_BOARD)) {
		KeInitializeEvent(&CreatedDeviceObject->DeviceLock, SynchronizationEvent, TRUE);
		CreatedDeviceObject->MountedOrSelfDevice = nullptr;
	}
	else {
		CreatedDeviceObject->MountedOrSelfDevice = CreatedDeviceObject;
	}

	CreatedDeviceObject->AlignmentRequirement = 0;
	CreatedDeviceObject->Flags = DO_DEVICE_INITIALIZING;
	if (DeviceName) {
		CreatedDeviceObject->Flags |= DO_DEVICE_HAS_NAME;
	}
	if (Exclusive) {
		CreatedDeviceObject->Flags |= DO_EXCLUSIVE;
	}
	CreatedDeviceObject->DeviceExtension = DeviceExtensionSize ? CreatedDeviceObject + 1 : nullptr;
	CreatedDeviceObject->StackSize = 1;
	KeInitializeDeviceQueue(&CreatedDeviceObject->DeviceQueue);

	HANDLE Handle;
	Status = ObInsertObject(CreatedDeviceObject, &ObjectAttributes, 1, &Handle);
	if (NT_SUCCESS(Status)) {
		*DeviceObject = CreatedDeviceObject;
		CreatedDeviceObject->DriverObject = DriverObject;
		NtClose(Handle);
	}

	return Status;
}

EXPORTNUM(66) NTSTATUS XBOXAPI IoCreateFile
(
	PHANDLE FileHandle,
	ACCESS_MASK DesiredAccess,
	POBJECT_ATTRIBUTES ObjectAttributes,
	PIO_STATUS_BLOCK IoStatusBlock,
	PLARGE_INTEGER AllocationSize,
	ULONG FileAttributes,
	ULONG ShareAccess,
	ULONG Disposition,
	ULONG CreateOptions,
	ULONG Options
)
{
	if (Options & IO_CHECK_CREATE_PARAMETERS) {
		/* Validate parameters */

		if (FileAttributes & ~FILE_ATTRIBUTE_VALID_FLAGS) {
			return STATUS_INVALID_PARAMETER;
		}

		if (ShareAccess & ~FILE_SHARE_VALID_FLAGS) {
			return STATUS_INVALID_PARAMETER;
		}

		if (Disposition > FILE_MAXIMUM_DISPOSITION) {
			return STATUS_INVALID_PARAMETER;
		}

		if (CreateOptions & ~FILE_VALID_OPTION_FLAGS) {
			return STATUS_INVALID_PARAMETER;
		}

		if ((CreateOptions & (FILE_SYNCHRONOUS_IO_ALERT | FILE_SYNCHRONOUS_IO_NONALERT)) && (!(DesiredAccess & SYNCHRONIZE))) {
			return STATUS_INVALID_PARAMETER;
		}

		if ((CreateOptions & FILE_DELETE_ON_CLOSE) && (!(DesiredAccess & DELETE))) {
			return STATUS_INVALID_PARAMETER;
		}

		if ((CreateOptions & (FILE_SYNCHRONOUS_IO_NONALERT | FILE_SYNCHRONOUS_IO_ALERT)) == (FILE_SYNCHRONOUS_IO_NONALERT | FILE_SYNCHRONOUS_IO_ALERT)){
			return STATUS_INVALID_PARAMETER;
		}

		if ((CreateOptions & FILE_DIRECTORY_FILE) && !(CreateOptions & FILE_NON_DIRECTORY_FILE) && (CreateOptions & ~(FILE_DIRECTORY_FILE |
				FILE_SYNCHRONOUS_IO_ALERT |
				FILE_SYNCHRONOUS_IO_NONALERT |
				FILE_WRITE_THROUGH |
				FILE_COMPLETE_IF_OPLOCKED |
				FILE_OPEN_FOR_BACKUP_INTENT |
				FILE_DELETE_ON_CLOSE |
				FILE_OPEN_FOR_FREE_SPACE_QUERY |
				FILE_OPEN_BY_FILE_ID |
				FILE_OPEN_REPARSE_POINT))
			|| ((Disposition != FILE_CREATE)
				&& (Disposition != FILE_OPEN)
				&& (Disposition != FILE_OPEN_IF))
			) {
			return STATUS_INVALID_PARAMETER;
		}

		if ((CreateOptions & FILE_COMPLETE_IF_OPLOCKED) && (CreateOptions & FILE_RESERVE_OPFILTER)) {
			return STATUS_INVALID_PARAMETER;
		}

		if ((CreateOptions & FILE_NO_INTERMEDIATE_BUFFERING) && (DesiredAccess & FILE_APPEND_DATA)) {
			return STATUS_INVALID_PARAMETER;
		}
	}

	LARGE_INTEGER CapturedAllocationSize;
	CapturedAllocationSize.QuadPart = AllocationSize ? AllocationSize->QuadPart : 0;

	OPEN_PACKET OpenPacket;
	OpenPacket.Type = IO_TYPE_OPEN_PACKET;
	OpenPacket.Size = sizeof(OPEN_PACKET);
	OpenPacket.ParseCheck = 0;
	OpenPacket.AllocationSize = CapturedAllocationSize;
	OpenPacket.CreateOptions = CreateOptions;
	OpenPacket.FileAttributes = (USHORT)FileAttributes;
	OpenPacket.ShareAccess = (USHORT)ShareAccess;
	OpenPacket.Disposition = Disposition;
	OpenPacket.QueryOnly = FALSE;
	OpenPacket.DeleteOnly = FALSE;
	OpenPacket.Options = Options;
	OpenPacket.RelatedFileObject = nullptr;
	OpenPacket.DesiredAccess = DesiredAccess;
	OpenPacket.FinalStatus = STATUS_SUCCESS;
	OpenPacket.FileObject = nullptr;
	OpenPacket.ObjectAttributes = ObjectAttributes;

	HANDLE Handle;
	NTSTATUS Status = ObOpenObjectByName(ObjectAttributes, nullptr, &OpenPacket, &Handle);

	BOOL SuccessfulIoParse = (BOOLEAN)(OpenPacket.ParseCheck == OPEN_PACKET_PATTERN);
	if (!NT_SUCCESS(Status) || !SuccessfulIoParse) {
		if (NT_SUCCESS(Status)) {
			NtClose(Handle);
			Status = STATUS_OBJECT_TYPE_MISMATCH;
		}

		if (!NT_SUCCESS(OpenPacket.FinalStatus)) {
			Status = OpenPacket.FinalStatus;

			if (NT_WARNING(Status)) {
				IoStatusBlock->Status = OpenPacket.FinalStatus;
				IoStatusBlock->Information = OpenPacket.Information;
			}
		}
		else if (OpenPacket.FileObject && !SuccessfulIoParse) {
			OpenPacket.FileObject->DeviceObject = nullptr;
			ObfDereferenceObject(OpenPacket.FileObject);
		}
	}
	else {
		OpenPacket.FileObject->Flags |= FO_HANDLE_CREATED;
		*FileHandle = Handle;
		IoStatusBlock->Information = OpenPacket.Information;
		IoStatusBlock->Status = OpenPacket.FinalStatus;
		Status = OpenPacket.FinalStatus;
	}

	if (SuccessfulIoParse && OpenPacket.FileObject) {
		ObfDereferenceObject(OpenPacket.FileObject);
	}

	return Status;
}

EXPORTNUM(67) NTSTATUS XBOXAPI IoCreateSymbolicLink
(
	PSTRING SymbolicLinkName,
	PSTRING DeviceName
)
{
	// Must be an absolute path because the search below always starts from the root directory
	if (!DeviceName->Length || (DeviceName->Buffer[0] != OB_PATH_DELIMITER)) {
		return STATUS_INVALID_PARAMETER;
	}

	KIRQL OldIrql = ObLock();

	// Sanity check: ensure that the target object exists
	NTSTATUS Status;
	PVOID FoundObject, TargetObject;
	POBJECT_DIRECTORY Directory = ObpRootDirectoryObject;
	OBJECT_STRING FirstName, RemainingName, OriName = *DeviceName;
	while (true) {
		ObpParseName(&OriName, &FirstName, &RemainingName);

		if (RemainingName.Length && (RemainingName.Buffer[0] == OB_PATH_DELIMITER)) {
			// Another delimiter in the name is invalid
			Status = STATUS_INVALID_PARAMETER;
			ObUnlock(OldIrql);
			break;
		}

		if (FoundObject = ObpFindObjectInDirectory(Directory, &FirstName, TRUE); FoundObject == NULL_HANDLE) {
			Status = STATUS_INVALID_PARAMETER;
			ObUnlock(OldIrql);
			break;
		}

		POBJECT_HEADER Obj = GetObjHeader(FoundObject);
		if (RemainingName.Length == 0) {
			Status = STATUS_SUCCESS;
			++Obj->PointerCount;
			TargetObject = FoundObject;
			ObUnlock(OldIrql);
			break;
		}

		if (Obj->Type != &ObDirectoryObjectType) {
			// NOTE: ParseProcedure is supposed to parse the remaining part of the name

			if (Obj->Type->ParseProcedure == nullptr) {
				Status = STATUS_INVALID_PARAMETER;
				ObUnlock(OldIrql);
				break;
			}

			++Obj->PointerCount;
			ObUnlock(OldIrql);

			Status = Obj->Type->ParseProcedure(FoundObject, nullptr, OBJ_CASE_INSENSITIVE, DeviceName, &RemainingName, nullptr, &TargetObject);

			if (Status == STATUS_OBJECT_TYPE_MISMATCH) {
				OBJECT_ATTRIBUTES ObjectAttributes;
				InitializeObjectAttributes(&ObjectAttributes, DeviceName, OBJ_CASE_INSENSITIVE, nullptr);

				HANDLE Handle;
				IO_STATUS_BLOCK IoStatusBlock;
				Status = NtOpenFile(&Handle, 0, &ObjectAttributes, &IoStatusBlock, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, FILE_DIRECTORY_FILE);

				if (NT_SUCCESS(Status)) {
					Status = ObReferenceObjectByHandle(Handle, &IoFileObjectType, &TargetObject);
					NtClose(Handle);
				}
			}

			ObfDereferenceObject(FoundObject);

			break;
		}

		// Reset Directory to the FoundObject, which we know it's a directory object if we reach here. Otherwise, the code will keep looking for the objects specified
		// in RemainingName in the root directory that was set when this loop first started
		Directory = (POBJECT_DIRECTORY)FoundObject;
		OriName = RemainingName;
	}

	// If the check succeeded, create the sym link object and insert it into OB
	if (!NT_SUCCESS(Status)) {
		return Status;
	}

	ULONG DeviceNameLength = DeviceName->Length;
	OBJECT_ATTRIBUTES ObjectAttributes;
	InitializeObjectAttributes(&ObjectAttributes, SymbolicLinkName, OBJ_PERMANENT | OBJ_CASE_INSENSITIVE, nullptr);

	POBJECT_SYMBOLIC_LINK SymbolicObject;
	Status = ObCreateObject(&ObSymbolicLinkObjectType, &ObjectAttributes, sizeof(OBJECT_SYMBOLIC_LINK) + DeviceNameLength, (PVOID *)&SymbolicObject);

	if (NT_SUCCESS(Status)) {
		PCHAR LinkTargetBuffer = (PCHAR)(SymbolicObject + 1);
		strncpy(LinkTargetBuffer, DeviceName->Buffer, DeviceNameLength);

		SymbolicObject->LinkTargetObject = TargetObject;
		SymbolicObject->LinkTarget.Buffer = LinkTargetBuffer;
		SymbolicObject->LinkTarget.Length = (USHORT)DeviceNameLength;
		SymbolicObject->LinkTarget.MaximumLength = (USHORT)DeviceNameLength;

		HANDLE Handle;
		Status = ObInsertObject(SymbolicObject, &ObjectAttributes, 0, &Handle);
		if (NT_SUCCESS(Status)) {
			NtClose(Handle);
		}
	}
	else {
		ObfDereferenceObject(TargetObject);
	}

	return Status;
}

EXPORTNUM(68) VOID XBOXAPI IoDeleteDevice
(
	PDEVICE_OBJECT DeviceObject
)
{
	// Named devices use the permanent flag, so remove it so that we can actually delete it
	if (DeviceObject->Flags & DO_DEVICE_HAS_NAME) {
		ObMakeTemporaryObject(DeviceObject);
	}

	// Mark the device s being deleted so that IoParseDevice doesn't allow the opening/creation of new files with the device
	KIRQL OldIrql = IoLock();
	DeviceObject->DeletePending = TRUE;

	// When ReferenceCount is zero, no more files are open for this DeviceObject
	if (DeviceObject->ReferenceCount == 0) {
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

EXPORTNUM(72) VOID XBOXAPI IoFreeIrp
(
	PIRP Irp
)
{
	ExFreePool(Irp);
}

EXPORTNUM(73) VOID XBOXAPI IoInitializeIrp
(
	PIRP Irp,
	USHORT PacketSize,
	CCHAR StackSize
)
{
	memset(Irp, 0, PacketSize);
	Irp->Type = (CSHORT)IO_TYPE_IRP;
	Irp->Size = PacketSize;
	Irp->StackCount = StackSize;
	Irp->CurrentLocation = StackSize + 1;
	InitializeListHead(&Irp->ThreadListEntry);
	Irp->Tail.Overlay.CurrentStackLocation = (PIO_STACK_LOCATION)((UCHAR *)Irp + sizeof(IRP) + (StackSize * sizeof(IO_STACK_LOCATION)));
}

EXPORTNUM(74) NTSTATUS XBOXAPI IoInvalidDeviceRequest
(
	PDEVICE_OBJECT DeviceObject,
	PIRP Irp
)
{
	Irp->IoStatus.Status = STATUS_INVALID_DEVICE_REQUEST;
	IofCompleteRequest(Irp, PRIORITY_BOOST_IO);
	return STATUS_INVALID_DEVICE_REQUEST;
}

EXPORTNUM(78) VOID XBOXAPI IoRemoveShareAccess
(
	PFILE_OBJECT FileObject,
	PSHARE_ACCESS ShareAccess
)
{
	if (FileObject->ReadAccess || FileObject->WriteAccess || FileObject->DeleteAccess) {
		--ShareAccess->OpenCount;

		if (FileObject->ReadAccess) {
			--ShareAccess->ReadAccess;
		}

		if (FileObject->WriteAccess) {
			--ShareAccess->WriteAccess;
		}

		if (FileObject->DeleteAccess) {
			--ShareAccess->DeleteAccess;
		}

		if (FileObject->SharedRead) {
			--ShareAccess->SharedRead;
		}

		if (FileObject->SharedWrite) {
			--ShareAccess->SharedWrite;
		}

		if (FileObject->SharedDelete) {
			--ShareAccess->SharedDelete;
		}
	}
}

EXPORTNUM(80) VOID XBOXAPI IoSetShareAccess
(
	ULONG DesiredAccess,
	ULONG DesiredShareAccess,
	PFILE_OBJECT FileObject,
	PSHARE_ACCESS ShareAccess
)
{
	FileObject->ReadAccess = !!(DesiredAccess & (FILE_EXECUTE | FILE_READ_DATA));
	FileObject->WriteAccess = !!(DesiredAccess & (FILE_WRITE_DATA | FILE_APPEND_DATA));
	FileObject->DeleteAccess = !!(DesiredAccess & DELETE);
	FileObject->Synchronize = !!(DesiredAccess & SYNCHRONIZE);

	if (FileObject->ReadAccess || FileObject->WriteAccess || FileObject->DeleteAccess) {
		FileObject->SharedRead = !!(DesiredShareAccess & FILE_SHARE_READ);
		FileObject->SharedWrite = !!(DesiredShareAccess & FILE_SHARE_WRITE);
		FileObject->SharedDelete = !!(DesiredShareAccess & FILE_SHARE_DELETE);

		ShareAccess->OpenCount = 1;
		ShareAccess->ReadAccess = FileObject->ReadAccess;
		ShareAccess->WriteAccess = FileObject->WriteAccess;
		ShareAccess->DeleteAccess = FileObject->DeleteAccess;
		ShareAccess->SharedRead = FileObject->SharedRead;
		ShareAccess->SharedWrite = FileObject->SharedWrite;
		ShareAccess->SharedDelete = FileObject->SharedDelete;

	}
	else {
		ShareAccess->OpenCount = 0;
		ShareAccess->ReadAccess = 0;
		ShareAccess->WriteAccess = 0;
		ShareAccess->DeleteAccess = 0;
		ShareAccess->SharedRead = 0;
		ShareAccess->SharedWrite = 0;
		ShareAccess->SharedDelete = 0;
	}
}

EXPORTNUM(86) NTSTATUS FASTCALL IofCallDriver
(
	PDEVICE_OBJECT DeviceObject,
	PIRP Irp
)
{
	// This passes the Irp to the next lower level driver in the chain and calls it

	--Irp->CurrentLocation;
	PIO_STACK_LOCATION IrpStackPointer = IoGetNextIrpStackLocation(Irp);
	Irp->Tail.Overlay.CurrentStackLocation = IrpStackPointer;
	IrpStackPointer->DeviceObject = DeviceObject;

	return DeviceObject->DriverObject->MajorFunction[IrpStackPointer->MajorFunction](DeviceObject, Irp);
}

EXPORTNUM(87) VOID FASTCALL IofCompleteRequest
(
	PIRP Irp,
	CCHAR PriorityBoost
)
{
	if (Irp->CurrentLocation > (Irp->StackCount + 1) || // StackCount + 1 because IoInitializeIrp sets CurrentLocation as StackCount + 1
		(Irp->Type != IO_TYPE_IRP)) {
		KeBugCheckEx(MULTIPLE_IRP_COMPLETE_REQUESTS, (ULONG_PTR)Irp, __LINE__, 0, 0);
	}

	// This pops all PIO_STACK_LOCATIONs to see if a driver wants to be notified about the IO that just completed by calling its CompletionRoutine. Note that the CompletionRoutine
	// of a driver is stored in the PIO_STACK_LOCATION of the next driver, not on its own PIO_STACK_LOCATION
	PIO_STACK_LOCATION IrpStackPointer = IoGetCurrentIrpStackLocation(Irp);
	for (++Irp->CurrentLocation, ++Irp->Tail.Overlay.CurrentStackLocation;
		Irp->CurrentLocation <= (CCHAR)(Irp->StackCount + 1);
		++IrpStackPointer, ++Irp->CurrentLocation, ++Irp->Tail.Overlay.CurrentStackLocation) {

		if (IrpStackPointer->Control & SL_MUST_COMPLETE) {
			RIP_API_MSG("SL_MUST_COMPLETE not implemented");
		}

		Irp->PendingReturned = (BOOLEAN)(IrpStackPointer->Control & SL_PENDING_RETURNED);

		if ((NT_SUCCESS(Irp->IoStatus.Status) && (IrpStackPointer->Control & SL_INVOKE_ON_SUCCESS)) ||
			(!NT_SUCCESS(Irp->IoStatus.Status) && (IrpStackPointer->Control & SL_INVOKE_ON_ERROR))) {

			ZeroIrpStackLocation(IrpStackPointer);

			NTSTATUS Status = IrpStackPointer->CompletionRoutine((PDEVICE_OBJECT)(Irp->CurrentLocation == (CCHAR)(Irp->StackCount + 1) ?
				nullptr : IoGetCurrentIrpStackLocation(Irp)->DeviceObject),
				Irp,
				IrpStackPointer->Context);

			if (Status == STATUS_MORE_PROCESSING_REQUIRED) {
				return;
			}
		}
		else {
			if (Irp->PendingReturned && (Irp->CurrentLocation <= Irp->StackCount)) {
				IoMarkIrpPending(Irp);
			}
			ZeroIrpStackLocation(IrpStackPointer);
		}
	}

	if (Irp->Flags & (IRP_MOUNT_COMPLETION)) {
		RIP_API_MSG("SL_MUST_COMPLETE not implemented");
	}

	if (Irp->Flags & IRP_CLOSE_OPERATION) {
		// This is a close request from NtClose when the last handle is closed. The caller will already dequeue the packet and free the IRP, and there's no user event for
		// this request. So basically, we only need to copy back the final IoStatus of the operation here
		*Irp->UserIosb = Irp->IoStatus;
		return;
	}

	if (Irp->Flags & IRP_UNLOCK_USER_BUFFER) {
		RIP_API_MSG("IRP_UNLOCK_USER_BUFFER not implemented");
	}
	else if (Irp->SegmentArray) {
		RIP_API_MSG("Irp->SegmentArray not implemented");
	}

	if (Irp->Flags & IRP_DEFER_IO_COMPLETION && !Irp->PendingReturned) {
		return;
	}

	PETHREAD Thread = Irp->Tail.Overlay.Thread;
	PFILE_OBJECT FileObject = Irp->Tail.Overlay.OriginalFileObject;

	// Finally, inform the thread that originally requested this IO about its completion
	if (!Irp->Cancel) {
		KeInitializeApc(&Irp->Tail.Apc,
			&Thread->Tcb,
			IopCompleteRequest,
			nullptr,
			nullptr,
			KernelMode,
			nullptr);

		KeInsertQueueApc(&Irp->Tail.Apc,
			FileObject,
			nullptr,
			PriorityBoost);
	}
	else {
		KIRQL OldIrql = KeRaiseIrqlToDpcLevel();
		Thread = Irp->Tail.Overlay.Thread;

		if (Thread) {
			KeInitializeApc(&Irp->Tail.Apc,
				&Thread->Tcb,
				IopCompleteRequest,
				nullptr,
				nullptr,
				KernelMode,
				nullptr);

			KeInsertQueueApc(&Irp->Tail.Apc,
				FileObject,
				nullptr,
				PriorityBoost);

			KfLowerIrql(OldIrql);
		}
		else {
			// The original thread is gone already, so just destroy this IRP
			KfLowerIrql(OldIrql);
			IopDropIrp(Irp, FileObject);
		}
	}
}

NTSTATUS XBOXAPI IoParseDevice(PVOID ParseObject, POBJECT_TYPE ObjectType, ULONG Attributes, POBJECT_STRING Name, POBJECT_STRING RemainingName,
	PVOID Context, PVOID *Object)
{
	*Object = NULL_HANDLE;

	// This function is supposed to be called for create/open request from NtCreate/OpenFile, so the context is an OPEN_PACKET
	POPEN_PACKET OpenPacket = (POPEN_PACKET)Context;

	if ((OpenPacket == nullptr) && (RemainingName->Length == 0)) {
		ObfReferenceObject(ParseObject);
		*Object = ParseObject;
		return STATUS_SUCCESS;
	}

	// Sanity check: ensure that the request is really open/create
	// INFO: the case OpenPacket == nullptr is triggered by IoCreateSymbolicLink when the target object is a HDD path and it invokes the ParseProcedure with a nullptr ParseContext
	if ((OpenPacket == nullptr) || ((OpenPacket->Type != IO_TYPE_OPEN_PACKET) || (OpenPacket->Size != sizeof(OPEN_PACKET)))) {
		NTSTATUS Status = STATUS_OBJECT_TYPE_MISMATCH;
		if (OpenPacket) {
			OpenPacket->FinalStatus = Status;
		}
		return Status;
	}

	PDEVICE_OBJECT ParsedDeviceObject = (PDEVICE_OBJECT)ParseObject;

	if (OpenPacket->RelatedFileObject) {
		ParsedDeviceObject = OpenPacket->RelatedFileObject->DeviceObject;
	}

	KIRQL OldIrql = IoLock();
	if (ParsedDeviceObject->DeletePending || (ParsedDeviceObject->Flags & DO_DEVICE_INITIALIZING)) {
		// Device is going away or being initialized, fail the request
		IoUnlock(OldIrql);
		OpenPacket->FinalStatus = STATUS_NO_SUCH_DEVICE;
		return OpenPacket->FinalStatus;
	}
	else if ((ParsedDeviceObject->Flags & DO_EXCLUSIVE) && ParsedDeviceObject->ReferenceCount) {
		// Device has exclusive access and somebody else already opened it, fail the request
		IoUnlock(OldIrql);
		OpenPacket->FinalStatus = STATUS_ACCESS_DENIED;
		return OpenPacket->FinalStatus;
	}

	PDEVICE_OBJECT MountedDeviceObject = ParsedDeviceObject->MountedOrSelfDevice;
	while (MountedDeviceObject == nullptr) {
		IoUnlock(OldIrql);
		// Raw device access can only be supported if the access is not relative to another file/dir and there isn't more path to process
		if (NTSTATUS Status = IopMountDevice(ParsedDeviceObject, (RemainingName->Length == 0) && !OpenPacket->RelatedFileObject); !NT_SUCCESS(Status)) {
			OpenPacket->FinalStatus = Status;
			return Status;
		}
		OldIrql = IoLock();
		MountedDeviceObject = ParsedDeviceObject->MountedOrSelfDevice;
	}

	// Increase ReferenceCount for every file that's opened/created with this MountedDeviceObject
	++MountedDeviceObject->ReferenceCount;
	IoUnlock(OldIrql);

	RtlMapGenericMask(&OpenPacket->DesiredAccess, &IoFileMapping);

	// Create the IRP to submit the I/O request
	PIRP Irp = IoAllocateIrp(MountedDeviceObject->StackSize);
	if (Irp == nullptr) {
		IopDereferenceDeviceObject(MountedDeviceObject);
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	IO_STATUS_BLOCK IoStatus;
	Irp->Tail.Overlay.Thread = (PETHREAD)KeGetCurrentThread();
	Irp->Flags = IRP_CREATE_OPERATION | IRP_SYNCHRONOUS_API | IRP_DEFER_IO_COMPLETION;
	Irp->Overlay.AllocationSize = OpenPacket->AllocationSize;
	Irp->UserIosb = &IoStatus;

	PIO_STACK_LOCATION IrpStackPointer = IoGetNextIrpStackLocation(Irp);
	IrpStackPointer->MajorFunction = IRP_MJ_CREATE;
	IrpStackPointer->Flags = (UCHAR)OpenPacket->Options;
	if ((Attributes & OBJ_CASE_INSENSITIVE) == 0) {
		IrpStackPointer->Flags |= SL_CASE_SENSITIVE;
	}
	
	IrpStackPointer->Parameters.Create.Options = (OpenPacket->Disposition << 24) | (OpenPacket->CreateOptions & 0x00ffffff);
	IrpStackPointer->Parameters.Create.FileAttributes = OpenPacket->FileAttributes;
	IrpStackPointer->Parameters.Create.ShareAccess = OpenPacket->ShareAccess;
	IrpStackPointer->Parameters.Create.DesiredAccess = OpenPacket->DesiredAccess;
	IrpStackPointer->Parameters.Create.RemainingName = RemainingName;

	BOOL FileObjectRequired = !(OpenPacket->QueryOnly || OpenPacket->DeleteOnly);
	PFILE_OBJECT FileObject;

	if (FileObjectRequired) {
		OBJECT_ATTRIBUTES objectAttributes;
		InitializeObjectAttributes(&objectAttributes, Name, Attributes, nullptr);

		if (NTSTATUS Status = ObCreateObject(&IoFileObjectType, &objectAttributes, sizeof(FILE_OBJECT), (PVOID *)&FileObject); !NT_SUCCESS(Status)) {
			IoFreeIrp(Irp);
			IopDereferenceDeviceObject(MountedDeviceObject);
			OpenPacket->FinalStatus = Status;
			return Status;
		}

		memset(FileObject, 0, sizeof(FILE_OBJECT));
		FileObject->Type = IO_TYPE_FILE;
		if (OpenPacket->CreateOptions & (FILE_SYNCHRONOUS_IO_ALERT | FILE_SYNCHRONOUS_IO_NONALERT)) {
			FileObject->Flags = FO_SYNCHRONOUS_IO;
			if (OpenPacket->CreateOptions & FILE_SYNCHRONOUS_IO_ALERT) {
				FileObject->Flags |= FO_ALERTABLE_IO;
			}
		}
		if (FileObject->Flags & FO_SYNCHRONOUS_IO) {
			FileObject->LockCount = -1;
			KeInitializeEvent(&FileObject->Lock, SynchronizationEvent, FALSE);
		}
		if ((OpenPacket->DesiredAccess & FILE_APPEND_DATA) && !(OpenPacket->DesiredAccess & FILE_WRITE_DATA)) {
			FileObject->Flags |= FO_APPEND_ONLY;
		}

		if (OpenPacket->CreateOptions & FILE_NO_INTERMEDIATE_BUFFERING) {
			FileObject->Flags |= FO_NO_INTERMEDIATE_BUFFERING;
		}

		if (OpenPacket->CreateOptions & FILE_SEQUENTIAL_ONLY) {
			FileObject->Flags |= FO_SEQUENTIAL_ONLY;
		}
	}
	else {
		RIP_API_MSG("QueryOnly and DeleteOnly flags in OpenPacket are not implemented");
	}

	FileObject->Type = IO_TYPE_FILE;
	FileObject->RelatedFileObject = OpenPacket->RelatedFileObject;
	FileObject->DeviceObject = MountedDeviceObject;

	Irp->Tail.Overlay.DriverContext[0] = OpenPacket->ObjectAttributes;
	Irp->Tail.Overlay.OriginalFileObject = FileObject;
	IrpStackPointer->FileObject = FileObject;

	KeInitializeEvent(&FileObject->Event, NotificationEvent, FALSE);
	OpenPacket->FileObject = FileObject;

	IopQueueThreadIrp(Irp);

	// Current irp setup:
	// stack location 1 -> fs/raw driver (fatx/xdvdfs/udf or raw disk)
	// stack location 0 -> device driver (hdd/mu/...) 
	NTSTATUS Status = IofCallDriver(MountedDeviceObject, Irp);

	if (Status == STATUS_PENDING) {
		KeWaitForSingleObject(&FileObject->Event, Executive, KernelMode, FALSE, nullptr);
		Status = IoStatus.Status;
	}
	else {
		KIRQL OldIrql = KfRaiseIrql(APC_LEVEL);

		IoStatus = Irp->IoStatus;
		Status = IoStatus.Status;
		FileObject->Event.Header.SignalState = 1;
		IopDequeueThreadIrp(Irp);
		IoFreeIrp(Irp);

		KfLowerIrql(OldIrql);
	}

	OpenPacket->Information = IoStatus.Information;

	if (!NT_SUCCESS(Status)) {
		FileObject->DeviceObject = nullptr;
		if (FileObjectRequired) {
			ObfDereferenceObject(FileObject);
		}
		OpenPacket->FileObject = nullptr;
		IopDereferenceDeviceObject(MountedDeviceObject);

		return OpenPacket->FinalStatus = Status;
	}

	if (FileObjectRequired) {
		*Object = FileObject;
		OpenPacket->ParseCheck = OPEN_PACKET_PATTERN;
		ObfReferenceObject(FileObject);

		return OpenPacket->FinalStatus = IoStatus.Status;
	}
	else {
		RIP_API_MSG("QueryOnly and DeleteOnly flags in OpenPacket are not implemented");
	}
}

PIRP IoAllocateIrpNoFail(CCHAR StackSize)
{
	while (true) {
		PIRP Irp = IoAllocateIrp(StackSize);
		if (Irp) {
			return Irp;
		}

		LARGE_INTEGER Timeout{ .QuadPart = -50 * 10000 };
		KeDelayExecutionThread(KernelMode, FALSE, &Timeout);
	}
}
