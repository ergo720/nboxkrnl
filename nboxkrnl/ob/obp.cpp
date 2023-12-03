/*
 * ergo720                Copyright (c) 2023
 */

#include "obp.hpp"
#include "..\ex\ex.hpp"
#include "..\nt\nt.hpp"
#include "..\rtl\rtl.hpp"
#include <string.h>


EXPORTNUM(245) OBJECT_HANDLE_TABLE ObpObjectHandleTable;

ULONG ObpGetObjectHashIndex(POBJECT_STRING Name)
{
	// This uses the djb2 algorithm to hash the object's name

	unsigned long Hash = 5381;
	for (int i = 0, Char = Name->Buffer[i]; i < (int)Name->Length; ++i, Char = Name->Buffer[i]) {
		Hash = ((Hash << 5) + Hash) + Char; // Hash * 33 + Char
	}

	return Hash & (OB_NUMBER_HASH_BUCKETS - 1);
}

BOOLEAN ObpCreatePermanentObjects()
{
	POBJECT_DIRECTORY *Objects[] = { &ObpRootDirectoryObject, &ObpDosDevicesDirectoryObject, &ObpWin32NamedObjectsDirectoryObject, &ObpIoDevicesDirectoryObject };
	POBJECT_STRING ObjectStrings[] = { nullptr, &ObpDosDevicesString, &ObpWin32NamedObjectsString, &ObpIoDevicesString };

	for (unsigned i = 0; i < 4; ++i) {
		OBJECT_ATTRIBUTES ObjectAttributes;
		InitializeObjectAttributes(&ObjectAttributes, ObjectStrings[i], OBJ_PERMANENT, nullptr);

		HANDLE Handle;
		NTSTATUS Status = NtCreateDirectoryObject(&Handle, &ObjectAttributes);

		if (!NT_SUCCESS(Status)) {
			return FALSE;
		}

		Status = ObReferenceObjectByHandle(Handle, &ObDirectoryObjectType, (PVOID *)Objects[i]);

		if (!NT_SUCCESS(Status)) {
			return FALSE;
		}

		NtClose(Handle);
	}

	return TRUE;
}

BOOLEAN ObpAllocateNewHandleTable()
{
	PVOID *NewTable = (PVOID *)ExAllocatePoolWithTag(OB_BYTES_PER_TABLE, 'atbO');
	if (NewTable == nullptr) {
		return FALSE;
	}

	if (IsHandleOnSegmentBoundary(ObpObjectHandleTable.NextHandleNeedingPool)) {
		// The RootTable has no more space, so we need to allocate a new buffer large enough to hold all the existing segments, then copy over the old segments
		// to the new larger buffer, and delete the old RootTable

		ULONG OldTableSize = (HandleToUlong(ObpObjectHandleTable.NextHandleNeedingPool) / OB_BYTES_PER_TABLE) / OB_TABLES_PER_SEGMENT;
		ULONG NewTableSize = OldTableSize + 1;
		PVOID *NewRootTable = (PVOID *)ExAllocatePoolWithTag(sizeof(HANDLE) * OB_TABLES_PER_SEGMENT * NewTableSize, 'orbO');
		if (NewRootTable == nullptr) {
			ExFreePool(NewTable);
			return FALSE;
		}

		memcpy(NewRootTable, ObpObjectHandleTable.RootTable, OldTableSize);
		ExFreePool(ObpObjectHandleTable.RootTable);
		ObpObjectHandleTable.RootTable = (PVOID **)NewRootTable;
	}

	ObpObjectHandleTable.RootTable[HandleToUlong(ObpObjectHandleTable.NextHandleNeedingPool) / OB_BYTES_PER_TABLE] = NewTable;
	HANDLE NextFreeHandle = UlongToHandle(EncodeFreeHandle(ObpObjectHandleTable.NextHandleNeedingPool) + sizeof(HANDLE));
	for (unsigned i = 0; i < (OB_HANDLES_PER_TABLE - 1); ++i, NextFreeHandle = UlongToHandle(HandleToUlong(NextFreeHandle) + sizeof(HANDLE))) {
		ObpObjectHandleTable.RootTable[HandleToUlong(ObpObjectHandleTable.NextHandleNeedingPool) / OB_BYTES_PER_TABLE][i] = NextFreeHandle;
	}
	ObpObjectHandleTable.RootTable[HandleToUlong(ObpObjectHandleTable.NextHandleNeedingPool) / OB_BYTES_PER_TABLE][OB_HANDLES_PER_TABLE - 1] = (HANDLE)-1;
	ObpObjectHandleTable.FirstFreeTableEntry = HandleToUlong(ObpObjectHandleTable.NextHandleNeedingPool);
	ObpObjectHandleTable.NextHandleNeedingPool = UlongToHandle(DecodeFreeHandle(NextFreeHandle));

	return TRUE;
}

HANDLE ObpCreateHandleForObject(PVOID Object)
{
	if (ObpObjectHandleTable.FirstFreeTableEntry == -1) {
		if (ObpAllocateNewHandleTable() == FALSE) {
			return NULL_HANDLE;
		}
	}

	// This gets the next free available handle in the system, finds the element pointer of that handle in the handle table, reads the next free handle from that
	// table element, and writes the object address to it, so that successive references to the handle can recover the object
	HANDLE Handle = UlongToHandle(ObpObjectHandleTable.FirstFreeTableEntry);
	PVOID *HandlePtr = GetHandleContentsPointer(Handle);
	ObpObjectHandleTable.FirstFreeTableEntry = DecodeFreeHandle(*HandlePtr);
	*HandlePtr = Object;
	++ObpObjectHandleTable.HandleCount;
	++GetObjHeader(Object)->HandleCount;

	return Handle;
}

PVOID ObpDestroyObjectHandle(HANDLE Handle)
{
	if (HandleToUlong(Handle) < HandleToUlong(ObpObjectHandleTable.NextHandleNeedingPool)) {
		PVOID *HandlePtr = GetHandleContentsPointer(Handle);
		PVOID Object = *HandlePtr;

		if (Object && !IsFreeHandle(Object)) {
			*HandlePtr = (PVOID)ObpObjectHandleTable.FirstFreeTableEntry;
			ObpObjectHandleTable.FirstFreeTableEntry = HandleToUlong(Handle);
			--ObpObjectHandleTable.HandleCount;

			return Object;
		}
	}

	return NULL_HANDLE;
}

template<bool ShouldReference>
PVOID ObpGetObjectFromHandle(HANDLE Handle)
{
	if (HandleToUlong(Handle) < HandleToUlong(ObpObjectHandleTable.NextHandleNeedingPool)) {
		PVOID *HandlePtr = GetHandleContentsPointer(Handle);
		PVOID Object = *HandlePtr;

		if (Object && !IsFreeHandle(Object)) {
			if constexpr (ShouldReference) {
				++GetObjHeader(Object)->PointerCount;
			}
			return Object;
		}
	}

	return NULL_HANDLE;
}

VOID ObpParseName(POBJECT_STRING Name, POBJECT_STRING FirstName, POBJECT_STRING RemainingName)
{
	// This function attempts to find the first delimiter character in the supplied name. If there is one, the characters preceding it are considered the FirstName,
	// and the characters following it are the RemainingName. If the delimiter is the leading character, it is discarded and the search starts after it.
	// If there is no delimiter, then all characters are the FirstName and no RemainingName is present. Note that, in all cases, the delimiter itself is not included
	// in FirstName and RemainingName
	/*
	* Name -> FirstName -> RemainingName
	* empty -> empty -> empty
	* D -> D -> empty
	* \ -> empty -> empty
	* \D -> D -> empty
	* C\D -> C -> D
	* \C\D -> C -> D
	* \C\ -> C -> empty
	* ??$D -> ??$D -> empty
	*/

	ULONG FirstDelimiterIdx = -1, StartIdx = 0;
	if (Name->Buffer[0] == OB_PATH_DELIMITER) {
		StartIdx = 1;
	}

	for (ULONG i = StartIdx; i < Name->Length; ++i) {
		if (Name->Buffer[i] == OB_PATH_DELIMITER) {
			FirstDelimiterIdx = i;
			break;
		}
	}

	if (FirstDelimiterIdx == -1) {
		FirstName->Buffer = &Name->Buffer[StartIdx];
		FirstName->Length = Name->Length - StartIdx;
		FirstName->MaximumLength = FirstName->Length + 1;
		RemainingName->Buffer = nullptr;
		RemainingName->Length = 0;
		RemainingName->MaximumLength = 0;
	}
	else {
		FirstName->Buffer = &Name->Buffer[StartIdx];
		FirstName->Length = FirstDelimiterIdx - StartIdx;
		FirstName->MaximumLength = FirstName->Length + 1;
		if (Name->Length > FirstDelimiterIdx) {
			RemainingName->Buffer = &Name->Buffer[FirstDelimiterIdx + 1];
			RemainingName->Length = Name->Length - (FirstName->Length + StartIdx + 1);
			RemainingName->MaximumLength = RemainingName->Length + 1;
		}
		else {
			RemainingName->Buffer = nullptr;
			RemainingName->Length = 0;
			RemainingName->MaximumLength = 0;
		}
	}
}

PVOID ObpFindObjectInDirectory(POBJECT_DIRECTORY Directory, POBJECT_STRING Name, BOOLEAN ShouldFollowSymbolicLinks)
{
	PVOID Object = NULL_HANDLE;

	if ((Directory == ObpDosDevicesDirectoryObject) && ShouldFollowSymbolicLinks && (Name->Length == 2) && (Name->Buffer[1] == ':')) {
		// We are looking for a drive letter, so use the builtin array to find the object

		CHAR DriveLetter = Name->Buffer[0];
		if ((DriveLetter >= 'A') && (DriveLetter <= 'Z')) {
			Object = ObpDosDevicesDriveLetterArray[DriveLetter - 'A'];
		}
		else if ((DriveLetter >= 'a') && (DriveLetter <= 'z')) {
			Object = ObpDosDevicesDriveLetterArray[DriveLetter - 'a'];
		}

		return Object;
	}

	// Find the object inside the hash table HashBuckets of the directory object. Collisions are solved with a linked list in the buckets
	POBJECT_HEADER_NAME_INFO ObjHeaderNameInfo = Directory->HashBuckets[ObpGetObjectHashIndex(Name)];
	while (ObjHeaderNameInfo) {
		if (RtlEqualString(Name, &ObjHeaderNameInfo->Name, TRUE)) {

			Object = GetObjectFromDirInfoHeader(ObjHeaderNameInfo);
			if (ShouldFollowSymbolicLinks && (GetObjHeader(Object)->Type == &ObSymbolicLinkObjectType)) {
				Object = ((POBJECT_SYMBOLIC_LINK)Object)->LinkTargetObject;
			}

			return Object;
		}

		ObjHeaderNameInfo = ObjHeaderNameInfo->ChainLink;
	}

	return Object;
}

NTSTATUS ObpResolveRootHandlePath(POBJECT_ATTRIBUTES ObjectAttributes, POBJECT_DIRECTORY *Directory)
{
	if (ObjectAttributes->RootDirectory) {
		// If there is a RootHandle, then the search is relative to the path specified by the handle

		if (ObjectAttributes->ObjectName->Length && (ObjectAttributes->ObjectName->Buffer[0] == OB_PATH_DELIMITER)) {
			// If the name starts with a delimiter, then it's an absolute path
			return STATUS_OBJECT_NAME_INVALID;
		}

		if (ObjectAttributes->RootDirectory == ObWin32NamedObjectsDirectory()) {
			*Directory = ObpWin32NamedObjectsDirectoryObject;
		}
		else if (ObjectAttributes->RootDirectory == ObDosDevicesDirectory()) {
			*Directory = ObpDosDevicesDirectoryObject;
		}
		else {
			PVOID FoundObject = ObpGetObjectFromHandle<false>(ObjectAttributes->RootDirectory);
			if (FoundObject == NULL_HANDLE) {
				return STATUS_INVALID_HANDLE;
			}

			if (GetObjHeader(FoundObject)->Type != &ObDirectoryObjectType) {
				return STATUS_OBJECT_TYPE_MISMATCH;
			}

			*Directory = (POBJECT_DIRECTORY)FoundObject;
		}
	}
	else {
		if (!ObjectAttributes->ObjectName->Length || (ObjectAttributes->ObjectName->Buffer[0] != OB_PATH_DELIMITER)) {
			// If the name doesn't start with a delimiter, then it's a relative path
			return STATUS_OBJECT_NAME_INVALID;
		}

		*Directory = ObpRootDirectoryObject;
	}

	return STATUS_SUCCESS;
}

NTSTATUS ObpReferenceObjectByName(POBJECT_ATTRIBUTES ObjectAttributes, POBJECT_TYPE ObjectType, PVOID ParseContext, PVOID *ReturnedObject)
{
	*ReturnedObject = nullptr;
	OBJECT_STRING OriName = { 0, 0, nullptr };
	if (ObjectAttributes->ObjectName) {
		OriName = *ObjectAttributes->ObjectName;
	}

	KIRQL OldIrql = ObLock();
	POBJECT_DIRECTORY Directory;
	if (NTSTATUS Status = ObpResolveRootHandlePath(ObjectAttributes, &Directory); !NT_SUCCESS(Status)) {
		ObUnlock(OldIrql);
		return Status;
	}

	PVOID FoundObject = Directory;
	BOOLEAN ShouldFollowSymbolicLinks = ObjectType == &ObSymbolicLinkObjectType ? FALSE : TRUE;
	OBJECT_STRING FirstName, RemainingName;
	POBJECT_HEADER Obj = GetObjHeader(FoundObject);
	if (OriName.Length == 0) {
		// Special case: reference a directory with a relative path specified by the RootDirectory handle
		goto BypassFindObject;
	}

	while (true) {
		ObpParseName(&OriName, &FirstName, &RemainingName);

		if (RemainingName.Length && (RemainingName.Buffer[0] == OB_PATH_DELIMITER)) {
			// Another delimiter in the name is invalid
			ObUnlock(OldIrql);
			return STATUS_OBJECT_NAME_INVALID;
		}

		if (FoundObject = ObpFindObjectInDirectory(Directory, &FirstName, ShouldFollowSymbolicLinks); FoundObject == NULL_HANDLE) {
			NTSTATUS Status = RemainingName.Length ? STATUS_OBJECT_PATH_NOT_FOUND : STATUS_OBJECT_NAME_NOT_FOUND;
			ObUnlock(OldIrql);
			return Status;
		}

		Obj = GetObjHeader(FoundObject);
		if (RemainingName.Length == 0) {
		BypassFindObject:
			if (Obj->Type->ParseProcedure) {
				goto CallParseProcedure;
			}

			if (ObjectType && (ObjectType != Obj->Type)) {
				ObUnlock(OldIrql);
				return STATUS_OBJECT_TYPE_MISMATCH;
			}

			++Obj->PointerCount;
			*ReturnedObject = FoundObject;

			ObUnlock(OldIrql);
			return STATUS_SUCCESS;
		}

		if (Obj->Type != &ObDirectoryObjectType) {
			// NOTE: ParseProcedure is supposed to parse the remaining part of the name

			if (Obj->Type->ParseProcedure == nullptr) {
				ObUnlock(OldIrql);
				return STATUS_OBJECT_PATH_NOT_FOUND;
			}

		CallParseProcedure:
			++Obj->PointerCount;
			ObUnlock(OldIrql);

			PVOID ParsedObject = nullptr;
			NTSTATUS Status = Obj->Type->ParseProcedure(FoundObject, ObjectType,
				ObjectAttributes->Attributes, ObjectAttributes->ObjectName, &RemainingName, ParseContext, &ParsedObject);

			ObfDereferenceObject(FoundObject);

			if (NT_SUCCESS(Status)) {
				if ((ObjectType == nullptr) || (ObjectType == GetObjHeader(ParsedObject)->Type)) {
					*ReturnedObject = ParsedObject;
					Status = STATUS_SUCCESS;
				}
				else {
					ObfDereferenceObject(ParsedObject);
					Status = STATUS_OBJECT_TYPE_MISMATCH;
				}
			}

			return Status;
		}

		// Reset Directory to the FoundObject, which we know it's a directory object if we reach here. Otherwise, the code will keep looking for the objects specified
		// in RemainingName in the root directory that was set when this loop first started
		Directory = (POBJECT_DIRECTORY)FoundObject;
		OriName = RemainingName;
	}

	__asm {
	unreachable_eip:
		push 0
		push 0
		push 0
		push unreachable_eip
		push UNREACHABLE_CODE_REACHED
		call KeBugCheckEx // won't return
	}
}

VOID XBOXAPI ObpDeleteSymbolicLink(PVOID Object)
{
	RIP_UNIMPLEMENTED();
}

template PVOID ObpGetObjectFromHandle<true>(HANDLE Handle);
template PVOID ObpGetObjectFromHandle<false>(HANDLE Handle);
