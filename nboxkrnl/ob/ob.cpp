/*
 * ergo720                Copyright (c) 2023
 */

#include "ob.hpp"
#include "obp.hpp"


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
