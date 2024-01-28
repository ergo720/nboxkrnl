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

#define NULL_HANDLE ((HANDLE)(ULONG_PTR)0)
#define NtCurrentThread() ((HANDLE)-2)
#define ObDosDevicesDirectory() ((HANDLE)-3)
#define ObWin32NamedObjectsDirectory() ((HANDLE)-4)

#define OB_NUMBER_HASH_BUCKETS 11

#define OB_PATH_DELIMITER '\\'

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

using OBJECT_STRING = STRING;
using POBJECT_STRING = OBJECT_STRING *;

using OB_ALLOCATE_METHOD = PVOID(XBOXAPI *)(
	SIZE_T NumberOfBytes,
	ULONG Tag
	);

using OB_FREE_METHOD = VOID(XBOXAPI *)(
	PVOID Pointer
	);

using OB_CLOSE_METHOD = VOID(XBOXAPI *)(
	PVOID Object,
	ULONG SystemHandleCount
	);

using OB_DELETE_METHOD = VOID(XBOXAPI *)(
	PVOID Object
	);

using OB_PARSE_METHOD = NTSTATUS(XBOXAPI *)(
	PVOID ParseObject,
	struct OBJECT_TYPE *ObjectType,
	ULONG Attributes,
	POBJECT_STRING CompleteName,
	POBJECT_STRING RemainingName,
	PVOID Context,
	PVOID *Object
	);

struct OBJECT_TYPE {
	OB_ALLOCATE_METHOD AllocateProcedure;
	OB_FREE_METHOD FreeProcedure;
	OB_CLOSE_METHOD CloseProcedure;
	OB_DELETE_METHOD DeleteProcedure;
	OB_PARSE_METHOD ParseProcedure;
	PVOID DefaultObject;
	ULONG PoolTag;
};
using POBJECT_TYPE = OBJECT_TYPE *;

struct OBJECT_ATTRIBUTES {
	HANDLE RootDirectory;
	PSTRING ObjectName;
	ULONG Attributes;
};
using POBJECT_ATTRIBUTES = OBJECT_ATTRIBUTES *;

struct OBJECT_HEADER {
	LONG PointerCount;
	LONG HandleCount;
	POBJECT_TYPE Type;
	ULONG Flags;
	QUAD Body;
};
using POBJECT_HEADER = OBJECT_HEADER *;


struct OBJECT_DIRECTORY {
	struct OBJECT_HEADER_NAME_INFO *HashBuckets[OB_NUMBER_HASH_BUCKETS];
};
using POBJECT_DIRECTORY = OBJECT_DIRECTORY *;

struct OBJECT_SYMBOLIC_LINK {
	PVOID LinkTargetObject;
	OBJECT_STRING LinkTarget;
};
using POBJECT_SYMBOLIC_LINK = OBJECT_SYMBOLIC_LINK *;

struct OBJECT_HEADER_NAME_INFO {
	struct OBJECT_HEADER_NAME_INFO *ChainLink;
	struct OBJECT_DIRECTORY *Directory;
	OBJECT_STRING Name;
};
using POBJECT_HEADER_NAME_INFO = OBJECT_HEADER_NAME_INFO *;

#ifdef __cplusplus
extern "C" {
#endif

EXPORTNUM(239) DLLEXPORT NTSTATUS XBOXAPI ObCreateObject
(
	POBJECT_TYPE ObjectType,
	POBJECT_ATTRIBUTES ObjectAttributes,
	ULONG ObjectBodySize,
	PVOID *Object
);

EXPORTNUM(240) DLLEXPORT extern OBJECT_TYPE ObDirectoryObjectType;

EXPORTNUM(241) DLLEXPORT NTSTATUS XBOXAPI ObInsertObject
(
	PVOID Object,
	POBJECT_ATTRIBUTES ObjectAttributes,
	ULONG ObjectPointerBias,
	PHANDLE ReturnedHandle
);

EXPORTNUM(242) DLLEXPORT VOID XBOXAPI ObMakeTemporaryObject
(
	PVOID Object
);

EXPORTNUM(243) DLLEXPORT NTSTATUS XBOXAPI ObOpenObjectByName
(
	POBJECT_ATTRIBUTES ObjectAttributes,
	POBJECT_TYPE ObjectType,
	PVOID ParseContext,
	PHANDLE Handle
);

EXPORTNUM(244) DLLEXPORT NTSTATUS XBOXAPI ObOpenObjectByPointer
(
	PVOID Object,
	POBJECT_TYPE ObjectType,
	PHANDLE ReturnedHandle
);

EXPORTNUM(245) DLLEXPORT extern OBJECT_HANDLE_TABLE ObpObjectHandleTable;

EXPORTNUM(246) DLLEXPORT NTSTATUS XBOXAPI ObReferenceObjectByHandle
(
	HANDLE Handle,
	POBJECT_TYPE ObjectType,
	PVOID *ReturnedObject
);

EXPORTNUM(247) DLLEXPORT NTSTATUS XBOXAPI ObReferenceObjectByName
(
	POBJECT_STRING ObjectName,
	ULONG Attributes,
	POBJECT_TYPE ObjectType,
	PVOID ParseContext,
	PVOID *Object
);

EXPORTNUM(248) DLLEXPORT NTSTATUS XBOXAPI ObReferenceObjectByPointer
(
	PVOID Object,
	POBJECT_TYPE ObjectType
);

EXPORTNUM(249) DLLEXPORT extern OBJECT_TYPE ObSymbolicLinkObjectType;

EXPORTNUM(250) DLLEXPORT VOID FASTCALL ObfDereferenceObject
(
	PVOID Object
);

EXPORTNUM(251) DLLEXPORT VOID FASTCALL ObfReferenceObject
(
	PVOID Object
);

#ifdef __cplusplus
}
#endif

BOOLEAN ObInitSystem();
