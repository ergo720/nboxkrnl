/*
 * ergo720                Copyright (c) 2023
 */

#include "iop.hpp"
#include "hdd\fatx.hpp"
#include "..\rtl\rtl.hpp"


NTSTATUS IopMountDevice(PDEVICE_OBJECT DeviceObject)
{
	// Thread safety: acquire a device-specific lock
	KeWaitForSingleObject(&DeviceObject->DeviceLock, Executive, KernelMode, FALSE, nullptr);

	NTSTATUS Status = STATUS_SUCCESS;
	if (DeviceObject->MountedOrSelfDevice == nullptr) {
		if (DeviceObject->Flags & DO_RAW_MOUNT_ONLY) {
			RIP_API_MSG("Mounting raw devices is not supported");
		}
		else {
			switch (DeviceObject->DeviceType)
			{
			case FILE_DEVICE_CD_ROM:
				RIP_API_MSG("Mounting CD/DVDs is not supported");

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

VOID XBOXAPI IopCloseFile(PVOID Object, ULONG SystemHandleCount)
{
	RIP_UNIMPLEMENTED();
}

VOID XBOXAPI IopDeleteFile(PVOID Object)
{
	RIP_UNIMPLEMENTED();
}

NTSTATUS XBOXAPI IopParseFile(PVOID ParseObject, POBJECT_TYPE ObjectType, ULONG Attributes, POBJECT_STRING CompleteName, POBJECT_STRING RemainingName,
	PVOID Context, PVOID *Object)
{
	RIP_UNIMPLEMENTED();
}

VOID XBOXAPI IopCompleteRequest(PKAPC Apc, PKNORMAL_ROUTINE *NormalRoutine, PVOID *NormalContext, PVOID *SystemArgument1, PVOID *SystemArgument2)
{
	RIP_UNIMPLEMENTED();
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
