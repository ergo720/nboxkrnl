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

	ObpObjectHandleTable.RootTable[0][0] = (PVOID)0;
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

EXPORTNUM(251) VOID FASTCALL ObfReferenceObject
(
	PVOID Object
)
{
	InterlockedIncrement(&GetObjHeader(Object)->PointerCount);
}
