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
