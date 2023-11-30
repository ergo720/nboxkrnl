/*
 * ergo720                Copyright (c) 2023
 */

#include "..\ex\ex.hpp"
#include "..\rtl\rtl.hpp"
#include "..\ob\obp.hpp"
#include "..\nt\nt.hpp"
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
	IoInfoBlock InfoBlock;
	IoRequest Packet;
	Packet.Id = InterlockedIncrement64(&IoRequestId);
	Packet.Type = IoRequestType::Read;
	Packet.HandleOrPath = EEPROM_HANDLE;
	Packet.Offset = 0;
	Packet.Size = sizeof(XBOX_EEPROM);
	Packet.HandleOrAddress = (ULONG_PTR)&CachedEeprom;
	SubmitIoRequestToHost(&Packet);
	RetrieveIoRequestFromHost(&InfoBlock, Packet.Id);

	if (InfoBlock.Status != Success) {
		return FALSE;
	}

	XboxFactoryGameRegion = CachedEeprom.EncryptedSettings.GameRegion;

	if (!HddInitDriver()) {
		return FALSE;
	}

	return TRUE;
}

EXPORTNUM(59) PVOID XBOXAPI IoAllocateIrp
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

	HANDLE Handle;
	NTSTATUS Status = ObOpenObjectByName(ObjectAttributes, nullptr, &OpenPacket, &Handle);

	RIP_API_MSG("incomplete");
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
	RIP_UNIMPLEMENTED();
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
	RIP_UNIMPLEMENTED();
}

NTSTATUS XBOXAPI IoParseDevice(PVOID ParseObject, POBJECT_TYPE ObjectType, ULONG Attributes, POBJECT_STRING Name, POBJECT_STRING RemainderName,
	PVOID Context, PVOID *Object)
{
	*Object = NULL_HANDLE;

	// This function is supposed to be called for create/open request from NtCreate/OpenFile, so the context is an OPEN_PACKET
	POPEN_PACKET OpenPacket = (POPEN_PACKET)Context;

	if ((OpenPacket == nullptr) && (RemainderName->Length == 0)) {
		ObfDereferenceObject(ParseObject);
		*Object = ParseObject;
		return STATUS_SUCCESS;
	}

	// Sanity check: ensure that the request is really open/create
	if ((OpenPacket == nullptr) || ((OpenPacket->Type != IO_TYPE_OPEN_PACKET) || (OpenPacket->Size != sizeof(OPEN_PACKET)))) {
		NTSTATUS Status = STATUS_OBJECT_TYPE_MISMATCH;
		if (OpenPacket) {
			OpenPacket->FinalStatus = Status;
		}
		return Status;
	}

	PDEVICE_OBJECT ParsedDeviceObject = (PDEVICE_OBJECT)ParseObject;

	if (OpenPacket->RelatedFileObject) {
		RIP_API_MSG("RelatedFileObject in OpenPacket is not supported");
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
		if (NTSTATUS Status = IopMountDevice(ParsedDeviceObject); !NT_SUCCESS(Status)) {
			OpenPacket->FinalStatus = Status;
			return Status;
		}
		OldIrql = IoLock();
		MountedDeviceObject = ParsedDeviceObject->MountedOrSelfDevice;
	}

	++MountedDeviceObject->ReferenceCount;
	IoUnlock(OldIrql);

	RtlMapGenericMask(&OpenPacket->DesiredAccess, &IoFileMapping);

	// Create the IRP to submit the I/O request
	PIRP Irp = (PIRP)IoAllocateIrp(MountedDeviceObject->StackSize);
	if (Irp == nullptr) {
		IopDereferenceDeviceObject(MountedDeviceObject);
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	IO_STATUS_BLOCK IoStatus;
	Irp->Tail.Overlay.Thread = (PETHREAD)KeGetCurrentThread();
	Irp->Flags = IRP_CREATE_OPERATION | IRP_SYNCHRONOUS_API;
	Irp->Overlay.AllocationSize = OpenPacket->AllocationSize;
	Irp->UserIosb = &IoStatus;

	PIO_STACK_LOCATION IrpStackPointer = Irp->Tail.Overlay.CurrentStackLocation - 1;
	IrpStackPointer->MajorFunction = IRP_MJ_CREATE;
	IrpStackPointer->Flags = (UCHAR)OpenPacket->Options;
	if ((Attributes & OBJ_CASE_INSENSITIVE) == 0) {
		IrpStackPointer->Flags |= SL_CASE_SENSITIVE;
	}
	
	IrpStackPointer->Parameters.Create.Options = (OpenPacket->Disposition << 24) | (OpenPacket->CreateOptions & 0x00ffffff);
	IrpStackPointer->Parameters.Create.FileAttributes = OpenPacket->FileAttributes;
	IrpStackPointer->Parameters.Create.ShareAccess = OpenPacket->ShareAccess;
	IrpStackPointer->Parameters.Create.DesiredAccess = OpenPacket->DesiredAccess;
	IrpStackPointer->Parameters.Create.RemainingName = RemainderName;

	OBJECT_ATTRIBUTES objectAttributes;
	InitializeObjectAttributes(&objectAttributes, Name, Attributes, nullptr);
	PFILE_OBJECT FileObject;
	if (NTSTATUS Status = ObCreateObject(&IoFileObjectType, &objectAttributes, sizeof(FILE_OBJECT), (PVOID *)&FileObject); !NT_SUCCESS(Status)) {
		IoFreeIrp(Irp);
		IopDereferenceDeviceObject(MountedDeviceObject);
		OpenPacket->FinalStatus = Status;
		return Status;
	}

	memset(FileObject, 0, sizeof(FILE_OBJECT));
	FileObject->Type = IO_TYPE_FILE;
	FileObject->RelatedFileObject = OpenPacket->RelatedFileObject;
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

	FileObject->Type = IO_TYPE_FILE;
	FileObject->RelatedFileObject = OpenPacket->RelatedFileObject;
	FileObject->DeviceObject = MountedDeviceObject;

	Irp->Tail.Overlay.DriverContext[0] = Name;
	Irp->Tail.Overlay.OriginalFileObject = FileObject;
	IrpStackPointer->FileObject = FileObject;

	KeInitializeEvent(&FileObject->Event, NotificationEvent, FALSE);
	OpenPacket->FileObject = FileObject;

	IopQueueThreadIrp(Irp);

	// Current irp setup:
	// stack location 1 -> fs/raw driver (fatx/xdvdfs/udf or raw disk)
	// stack location 0 -> device driver (hdd/mu/...) 
	IofCallDriver(MountedDeviceObject, Irp);

	RIP_API_MSG("incomplete");
}
