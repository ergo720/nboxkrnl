/*
 * ergo720                Copyright (c) 2023
 */

#pragma once

#include "..\types.hpp"


#define OB_HANDLES_PER_TABLE_SHIFT   6
#define OB_HANDLES_PER_TABLE         (1 << OB_HANDLES_PER_TABLE_SHIFT) // 64
#define OB_BYTES_PER_TABLE           (OB_HANDLES_PER_TABLE * sizeof(HANDLE)) // 256
#define OB_TABLES_PER_SEGMENT        8
#define OB_HANDLES_PER_SEGMENT       (OB_TABLES_PER_SEGMENT * OB_HANDLES_PER_TABLE) // 512

/*
The handle table RootTable works as if it were a 2D array of handles of one row and multiple columns. The row is allocated in segments increments of 8 elements each, and each
element has a pointer to a table of 64 handles. When a handle is free, the corresponding 4 byte element in the table holds the handle biased by one of the next free handle. The
last entry has a value of -1, so that Ob knows when it needs to allocate a new table. When a handle is in use, its entry in the table holds the virtual address of the kernel object
body that the handle refers to. The handles themselves are indices used to index RootTable to quickly find the corresponding object

			 segmentN -> ...
			 ...
			 segment1 -> ...
RootTable -> segment0 -> table0 addr -> handle0, handle1, obj addr, ..., handle62, -1/handle63
						 table1 addr -> ...
						 table2 addr -> ...
						 table3 addr -> ...
						 table4 addr -> ...
						 table5 addr -> ...
						 table6 addr -> ...
						 table7 addr -> ...
*/
struct OBJECT_HANDLE_TABLE {
	LONG HandleCount;
	LONG_PTR FirstFreeTableEntry;
	HANDLE NextHandleNeedingPool;
	PVOID **RootTable;
	PVOID *BuiltinRootTable[OB_TABLES_PER_SEGMENT]; // NOTE: unused
};
using POBJECT_HANDLE_TABLE = OBJECT_HANDLE_TABLE *;

#ifdef __cplusplus
extern "C" {
#endif

EXPORTNUM(245) DLLEXPORT extern OBJECT_HANDLE_TABLE ObpObjectHandleTable;

#ifdef __cplusplus
}
#endif

BOOLEAN ObInitSystem();
