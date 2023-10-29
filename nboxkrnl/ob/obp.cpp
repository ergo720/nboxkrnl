/*
 * ergo720                Copyright (c) 2023
 */

#include "..\ex\ex.hpp"
#include "ob.hpp"
#include "obp.hpp"
#include <string.h>


EXPORTNUM(245) OBJECT_HANDLE_TABLE ObpObjectHandleTable;

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
