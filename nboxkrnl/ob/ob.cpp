/*
 * ergo720                Copyright (c) 2023
 */

#include "ob.hpp"
#include "obp.hpp"
#include "..\rtl\rtl.hpp"
#include "..\ex\ex.hpp"


BOOLEAN ObInitSystem()
{
	ObpObjectHandleTable.HandleCount = 0;
	ObpObjectHandleTable.FirstFreeTableEntry = -1;
	ObpObjectHandleTable.NextHandleNeedingPool = 0;
	ObpObjectHandleTable.RootTable = nullptr;
	for (unsigned i = 0; i < OB_TABLES_PER_SEGMENT; ++i) {
		ObpObjectHandleTable.BuiltinRootTable[i] = nullptr;
	}

	// Create a new table and set the zero handle as zero
	// This way, if somebody passes the NULL handle (which is invalid), we will also read it back as invalid too
	// Doing this now allows us to avoid having to check for the NULL handle every time in ObpAllocateNewHandleTable
	if (ObpAllocateNewHandleTable() == FALSE) {
		return FALSE;
	}

	ObpObjectHandleTable.RootTable[0][0] = NULL_HANDLE;
	ObpObjectHandleTable.FirstFreeTableEntry = 4;

	return TRUE;
}

EXPORTNUM(239) NTSTATUS XBOXAPI ObCreateObject
(
	POBJECT_TYPE ObjectType,
	POBJECT_ATTRIBUTES ObjectAttributes,
	ULONG ObjectBodySize,
	PVOID *Object
)
{
	if ((ObjectAttributes == nullptr) || (ObjectAttributes->ObjectName == nullptr)) {
		POBJECT_HEADER Obj = (POBJECT_HEADER)ObjectType->AllocateProcedure(ObjectBodySize + sizeof(OBJECT_HEADER) - sizeof(OBJECT_HEADER::Body), ObjectType->PoolTag);

		if (Obj == nullptr) {
			return STATUS_INSUFFICIENT_RESOURCES;
		}

		Obj->HandleCount = 0;
		Obj->PointerCount = 1;
		Obj->Type = ObjectType;
		Obj->Flags = 0;

		*Object = &Obj->Body;

		return STATUS_SUCCESS;
	}

	RIP_API_MSG("creating named objects is not supported yet");
}

EXPORTNUM(241) NTSTATUS XBOXAPI ObInsertObject
(
	PVOID Object,
	POBJECT_ATTRIBUTES ObjectAttributes,
	ULONG ObjectPointerBias,
	PHANDLE ReturnedHandle
)
{
	KIRQL OldIrql = ObLock();

	*ReturnedHandle = NULL_HANDLE;
	POBJECT_DIRECTORY Directory;

	if ((ObjectAttributes == nullptr) || (ObjectAttributes->ObjectName == nullptr)) {
		Directory = nullptr;
	}
	else {
		RIP_API_MSG("inserting named objects is not supported yet");
	}

	HANDLE Handle = ObpCreateHandleForObject(Object);
	if (Handle == NULL_HANDLE) {
		ObUnlock(OldIrql);
		ObfDereferenceObject(Object);
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	// After this point, it can no longer fail or else ObfDereferenceObject will drop the object PointerCount to zero and delete the object
	POBJECT_HEADER Obj = GetObjHeader(Object);
	Obj->PointerCount += (ObjectPointerBias + 1);

	if (ObjectAttributes && ObjectAttributes->Attributes & OBJ_PERMANENT) {
		Obj->Flags |= OB_FLAG_PERMANENT_OBJECT;
	}

	*ReturnedHandle = Handle;

	ObUnlock(OldIrql);
	ObfDereferenceObject(Object);

	return STATUS_SUCCESS;
}

EXPORTNUM(250) VOID FASTCALL ObfDereferenceObject
(
	PVOID Object
)
{
	POBJECT_HEADER Obj = GetObjHeader(Object);
	if (InterlockedDecrement(&Obj->PointerCount) == 0) {
		if (Obj->Type->DeleteProcedure) {
			Obj->Type->DeleteProcedure(Object); // performs object-specific cleanup
		}

		if (Obj->Flags & OB_FLAG_NAMED_OBJECT) {
			RIP_API_MSG("destroying named objects is not supported yet");
		}
		else {
			Obj->Type->FreeProcedure(Obj); // releases the memory used for the object
		}
	}
}

EXPORTNUM(251) VOID FASTCALL ObfReferenceObject
(
	PVOID Object
)
{
	InterlockedIncrement(&GetObjHeader(Object)->PointerCount);
}
