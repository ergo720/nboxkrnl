/*
 * ergo720                Copyright (c) 2023
 */

#include "nt.hpp"
#include "..\ob\obp.hpp"
#include "..\rtl\rtl.hpp"
#include <string.h>


EXPORTNUM(187) NTSTATUS XBOXAPI NtClose
(
	HANDLE Handle
)
{
	KIRQL OldIrql = ObLock();

	if (PVOID Object = ObpDestroyObjectHandle(Handle); Object != NULL_HANDLE) {
		POBJECT_HEADER Obj = GetObjHeader(Object);
		LONG HandleCount = Obj->HandleCount;
		--Obj->HandleCount;

		if (Obj->Type->CloseProcedure) {
			ObUnlock(OldIrql);
			Obj->Type->CloseProcedure(Object, HandleCount);
			OldIrql = ObLock();
		}

		ObUnlock(OldIrql);

		if ((Obj->HandleCount == 0) && (Obj->Flags & OB_FLAG_ATTACHED_OBJECT) && !(Obj->Flags & OB_FLAG_PERMANENT_OBJECT)) {
			RIP_API_MSG("Closing handles with attached objects is not supported yet");
		}

		ObfDereferenceObject(Object);

		return STATUS_SUCCESS;
	}

	ObUnlock(OldIrql);

	return STATUS_INVALID_HANDLE;
}

EXPORTNUM(188) NTSTATUS XBOXAPI NtCreateDirectoryObject
(
	PHANDLE DirectoryHandle,
	POBJECT_ATTRIBUTES ObjectAttributes
)
{
	*DirectoryHandle = NULL_HANDLE;
	PVOID Object;
	NTSTATUS Status = ObCreateObject(&ObDirectoryObjectType, ObjectAttributes, sizeof(OBJECT_DIRECTORY), &Object);

	if (NT_SUCCESS(Status)) {
		memset(Object, 0, sizeof(OBJECT_DIRECTORY));
		Status = ObInsertObject(Object, ObjectAttributes, 0, DirectoryHandle);
	}

	return Status;
}

EXPORTNUM(203) NTSTATUS XBOXAPI NtOpenSymbolicLinkObject
(
	PHANDLE LinkHandle,
	POBJECT_ATTRIBUTES ObjectAttributes
)
{
	return ObOpenObjectByName(ObjectAttributes, &ObSymbolicLinkObjectType, nullptr, LinkHandle);
}

EXPORTNUM(233) NTSTATUS XBOXAPI NtWaitForSingleObject
(
	HANDLE Handle,
	BOOLEAN Alertable,
	PLARGE_INTEGER Timeout
)
{
	return NtWaitForSingleObjectEx(Handle, KernelMode, Alertable, Timeout);
}

EXPORTNUM(234) NTSTATUS XBOXAPI NtWaitForSingleObjectEx
(
	HANDLE Handle,
	KPROCESSOR_MODE WaitMode,
	BOOLEAN Alertable,
	PLARGE_INTEGER Timeout
)
{
	PVOID Object;
	NTSTATUS Status = ObReferenceObjectByHandle(Handle, nullptr, &Object);

	if (NT_SUCCESS(Status)) {
		// The following has to extract the member DISPATCHER_HEADER::Header of the object to be waited on. If Handle refers to an object that cannot be waited on,
		// then OBJECT_TYPE::DefaultObject holds the address of ObpDefaultObject, which is a global KEVENT always signalled, and thus the wait will be satisfied
		// immediately. Notice that, because the kernel has a fixed load address of 0x80010000, then &ObpDefaultObject will be interpreted as a negative number,
		// allowing us to distinguish the can't wait/can wait kinds of objects apart.

		POBJECT_HEADER Obj = GetObjHeader(Object); // get Ob header (NOT the same as DISPATCHER_HEADER)
		PVOID ObjectToWaitOn = Obj->Type->DefaultObject;

		if ((LONG_PTR)ObjectToWaitOn >= 0) {
			ObjectToWaitOn = (PCHAR)Object + (ULONG_PTR)ObjectToWaitOn; // DefaultObject is the offset of DISPATCHER_HEADER::Header
		}

		// KeWaitForSingleObject will raise an exception if the Handle is a mutant and its limit is exceeded
		__try {
			Status = KeWaitForSingleObject(ObjectToWaitOn, UserRequest, WaitMode, Alertable, Timeout);
		}
		__except (EXCEPTION_EXECUTE_HANDLER) {
			Status = GetExceptionCode();
		}

		ObfDereferenceObject(Object);
	}

	return Status;
}
