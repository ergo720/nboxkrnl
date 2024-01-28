/*
 * ergo720                Copyright (c) 2023
 */

#include "ob.hpp"
#include "obp.hpp"
#include "..\ex\ex.hpp"
#include "..\ps\ps.hpp"
#include <string.h>


EXPORTNUM(240) OBJECT_TYPE ObDirectoryObjectType = {
	ExAllocatePoolWithTag,
	ExFreePool,
	nullptr,
	nullptr,
	nullptr,
	&ObpDefaultObject,
	'eriD'
};

EXPORTNUM(249) OBJECT_TYPE ObSymbolicLinkObjectType = {
	ExAllocatePoolWithTag,
	ExFreePool,
	nullptr,
	ObpDeleteSymbolicLink,
	nullptr,
	&ObpDefaultObject,
	'bmyS'
};

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

	if (ObpCreatePermanentObjects() == FALSE) {
		return FALSE;
	}

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

	OBJECT_STRING RemainingName, OriName = *ObjectAttributes->ObjectName;
	OBJECT_STRING FirstName = { 0, 0, nullptr };
	while (OriName.Length) {
		ObpParseName(&OriName, &FirstName, &RemainingName);

		if (RemainingName.Length && (RemainingName.Buffer[0] == OB_PATH_DELIMITER)) {
			// Another delimiter in the name is invalid
			return STATUS_OBJECT_NAME_INVALID;
		}

		OriName = RemainingName;
	}

	if (FirstName.Length == 0) {
		// Empty object's name is invalid
		return STATUS_OBJECT_NAME_INVALID;
	}

	POBJECT_HEADER_NAME_INFO ObjectNameInfo = (POBJECT_HEADER_NAME_INFO)ObjectType->AllocateProcedure(sizeof(OBJECT_HEADER_NAME_INFO) + sizeof(OBJECT_HEADER) - sizeof(OBJECT_HEADER::Body)
		+ ObjectBodySize + FirstName.Length, ObjectType->PoolTag);

	if (ObjectNameInfo == nullptr) {
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	POBJECT_HEADER Obj = (POBJECT_HEADER)(ObjectNameInfo + 1);
	ObjectNameInfo->ChainLink = nullptr;
	ObjectNameInfo->Directory = nullptr;
	ObjectNameInfo->Name.Buffer = ((PCHAR)&Obj->Body + ObjectBodySize);
	ObjectNameInfo->Name.Length = FirstName.Length;
	ObjectNameInfo->Name.MaximumLength = FirstName.Length;
	strncpy(ObjectNameInfo->Name.Buffer, FirstName.Buffer, FirstName.Length);

	Obj->PointerCount = 1;
	Obj->HandleCount = 0;
	Obj->Type = ObjectType;
	Obj->Flags = OB_FLAG_NAMED_OBJECT;

	*Object = &Obj->Body;

	return STATUS_SUCCESS;
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
	PVOID ObjectToInsert = Object;

	*ReturnedHandle = NULL_HANDLE;
	POBJECT_DIRECTORY Directory;

	if ((ObjectAttributes == nullptr) || (ObjectAttributes->ObjectName == nullptr)) {
		Directory = nullptr;
	}
	else {
		if (NTSTATUS Status = ObpResolveRootHandlePath(ObjectAttributes, &Directory);
			!NT_SUCCESS(Status)) {
			ObUnlock(OldIrql);
			ObfDereferenceObject(Object);
			return Status;
		}

		OBJECT_STRING FirstName, RemainingName, OriName = *ObjectAttributes->ObjectName;
		while (true) {
			ObpParseName(&OriName, &FirstName, &RemainingName);

			// All names between the delimiters must be directory objects, and the object's name must not exist already
			if (PVOID ObjectInDirectory = ObpFindObjectInDirectory(Directory, &FirstName, TRUE); ObjectInDirectory != NULL_HANDLE) {
				if (RemainingName.Length == 0) {
					if (ObjectAttributes->Attributes & OBJ_OPENIF) {
						if (GetObjHeader(ObjectInDirectory)->Type == GetObjHeader(Object)->Type) {
							// Special case: if OBJ_OPENIF is set, insert the found object instead
							ObjectToInsert = ObjectInDirectory;
							Directory = nullptr;
							break;
						}
						else {
							ObUnlock(OldIrql);
							ObfDereferenceObject(Object);
							return STATUS_OBJECT_TYPE_MISMATCH;
						}
					}
					else {
						ObUnlock(OldIrql);
						ObfDereferenceObject(Object);
						return STATUS_OBJECT_NAME_COLLISION;
					}
				}

				if (GetObjHeader(ObjectInDirectory)->Type != &ObDirectoryObjectType) {
					ObUnlock(OldIrql);
					ObfDereferenceObject(Object);
					return STATUS_OBJECT_PATH_NOT_FOUND;
				}

				Directory = (POBJECT_DIRECTORY)ObjectInDirectory;
			}
			else {
				if (RemainingName.Length) {
					ObUnlock(OldIrql);
					ObfDereferenceObject(Object);
					return STATUS_OBJECT_PATH_NOT_FOUND;
				}

				break;
			}

			OriName = RemainingName;
		}
	}

	HANDLE Handle = ObpCreateHandleForObject(ObjectToInsert);
	if (Handle == NULL_HANDLE) {
		ObUnlock(OldIrql);
		ObfDereferenceObject(Object);
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	// After this point, it can no longer fail or else ObfDereferenceObject will drop the object PointerCount to zero and delete the object
	POBJECT_HEADER Obj = GetObjHeader(ObjectToInsert);
	Obj->PointerCount += (ObjectPointerBias + 1);

	if (Directory) {
		POBJECT_HEADER_NAME_INFO ObjDirectoryInfo = GetObjDirInfoHeader(Object);
		ULONG Index = ObpGetObjectHashIndex(&ObjDirectoryInfo->Name);
		Obj->Flags |= OB_FLAG_ATTACHED_OBJECT;

		ObjDirectoryInfo->Directory = Directory;
		ObjDirectoryInfo->ChainLink = Directory->HashBuckets[Index];
		Directory->HashBuckets[Index] = ObjDirectoryInfo;

		if ((Directory == ObpDosDevicesDirectoryObject) && (ObjDirectoryInfo->Name.Length == 2) && (ObjDirectoryInfo->Name.Buffer[1] == ':')) {
			PVOID DosDevicesObject = Object;

			if (GetObjHeader(DosDevicesObject)->Type == &ObSymbolicLinkObjectType) {
				DosDevicesObject = ((POBJECT_SYMBOLIC_LINK)DosDevicesObject)->LinkTargetObject;
			}

			CHAR DriveLetter = ObjDirectoryInfo->Name.Buffer[0];
			if ((DriveLetter >= 'A') && (DriveLetter <= 'Z')) {
				ObpDosDevicesDriveLetterArray[DriveLetter - 'A'] = DosDevicesObject;
			}
			else if ((DriveLetter >= 'a') && (DriveLetter <= 'z')) {
				ObpDosDevicesDriveLetterArray[DriveLetter - 'a'] = DosDevicesObject;
			}
		}

		++GetObjHeader(Directory)->PointerCount;
		++Obj->PointerCount;
	}

	if (ObjectAttributes && ObjectAttributes->Attributes & OBJ_PERMANENT) {
		Obj->Flags |= OB_FLAG_PERMANENT_OBJECT;
	}

	*ReturnedHandle = Handle;

	ObUnlock(OldIrql);
	ObfDereferenceObject(Object);

	return (Object == ObjectToInsert) ? STATUS_SUCCESS : STATUS_OBJECT_NAME_EXISTS;
}

EXPORTNUM(242) VOID XBOXAPI ObMakeTemporaryObject
(
	PVOID Object
)
{
	KIRQL OldIrql = ObLock();

	POBJECT_HEADER Obj = GetObjHeader(Object);
	Obj->Flags &= ~OB_FLAG_PERMANENT_OBJECT;

	if ((Obj->HandleCount == 0) && (Obj->Flags & OB_FLAG_ATTACHED_OBJECT)) {
		ObpDetachNamedObject(Object, OldIrql);
	}
	else {
		ObUnlock(OldIrql);
	}
}

EXPORTNUM(243) NTSTATUS XBOXAPI ObOpenObjectByName
(
	POBJECT_ATTRIBUTES ObjectAttributes,
	POBJECT_TYPE ObjectType,
	PVOID ParseContext,
	PHANDLE Handle
)
{
	PVOID Object;
	HANDLE NewHandle = NULL_HANDLE;
	NTSTATUS Status = ObpReferenceObjectByName(ObjectAttributes, ObjectType, ParseContext, &Object);

	if (NT_SUCCESS(Status)) {

		KIRQL OldIrql = ObLock();
		NewHandle = ObpCreateHandleForObject(Object);
		ObUnlock(OldIrql);

		if (NewHandle == NULL_HANDLE) {
			ObfDereferenceObject(Object);
			Status = STATUS_INSUFFICIENT_RESOURCES;
		}
	}

	*Handle = NewHandle;

	return Status;
}

EXPORTNUM(244) NTSTATUS XBOXAPI ObOpenObjectByPointer
(
	PVOID Object,
	POBJECT_TYPE ObjectType,
	PHANDLE ReturnedHandle
)
{
	*ReturnedHandle = NULL_HANDLE;

	if (NTSTATUS Status = NT_SUCCESS(ObReferenceObjectByPointer(Object, ObjectType))) {
		KIRQL OldIrql = ObLock();
		HANDLE Handle = ObpCreateHandleForObject(Object);
		ObUnlock(OldIrql);

		if (Handle == NULL_HANDLE) {
			ObfDereferenceObject(Object);
			return STATUS_INSUFFICIENT_RESOURCES;
		}

		*ReturnedHandle = Handle;
	}
	else {
		return Status;
	}

	return STATUS_SUCCESS;
}

EXPORTNUM(246) DLLEXPORT NTSTATUS XBOXAPI ObReferenceObjectByHandle
(
	HANDLE Handle,
	POBJECT_TYPE ObjectType,
	PVOID *ReturnedObject
)
{
	*ReturnedObject = NULL_HANDLE;

	// NOTE: on the xbox, NtCurrentProcess is not supported
	if (Handle == NtCurrentThread()) {
		if (!ObjectType || (ObjectType == &PsThreadObjectType)) {
			PVOID Object = (PETHREAD)KeGetCurrentThread();
			POBJECT_HEADER Obj = GetObjHeader(Object);
			InterlockedIncrement(&Obj->PointerCount);
			*ReturnedObject = Object;

			return STATUS_SUCCESS;
		}

		return STATUS_OBJECT_TYPE_MISMATCH;
	}
	else {
		KIRQL OldIrql = ObLock();
		PVOID Object = ObpGetObjectFromHandle<true>(Handle);
		ObUnlock(OldIrql);

		if (Object != NULL_HANDLE) {
			if (!ObjectType || (GetObjHeader(Object)->Type == ObjectType)) {
				*ReturnedObject = Object;

				return STATUS_SUCCESS;
			}

			ObfDereferenceObject(Object);
			return STATUS_OBJECT_TYPE_MISMATCH;
		}

		return STATUS_INVALID_HANDLE;
	}
}

EXPORTNUM(247) NTSTATUS XBOXAPI ObReferenceObjectByName
(
	POBJECT_STRING ObjectName,
	ULONG Attributes,
	POBJECT_TYPE ObjectType,
	PVOID ParseContext,
	PVOID *Object
)
{
	OBJECT_ATTRIBUTES ObjectAttributes;
	InitializeObjectAttributes(&ObjectAttributes, ObjectName, Attributes, nullptr);
	return ObpReferenceObjectByName(&ObjectAttributes, ObjectType, ParseContext, Object);
}

EXPORTNUM(248) NTSTATUS XBOXAPI ObReferenceObjectByPointer
(
	PVOID Object,
	POBJECT_TYPE ObjectType
)
{
	if (GetObjHeader(Object)->Type == ObjectType) {
		ObfReferenceObject(Object);
		return STATUS_SUCCESS;
	}

	return STATUS_OBJECT_TYPE_MISMATCH;
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

		// Releases the memory used for the object
		if (Obj->Flags & OB_FLAG_NAMED_OBJECT) {
			Obj->Type->FreeProcedure(GetObjDirInfoHeader(Object));
		}
		else {
			Obj->Type->FreeProcedure(Obj);
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
