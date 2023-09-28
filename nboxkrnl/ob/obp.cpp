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
