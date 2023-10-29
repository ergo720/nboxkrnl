/*
 * ergo720                Copyright (c) 2023
 */

#pragma once

#include "ob.hpp"

// Object attributes specified in OBJECT_ATTRIBUTES
#define OBJ_PERMANENT  0x00000010L

// Object flags specified in OBJECT_HEADER
#define OB_FLAG_NAMED_OBJECT      0x01
#define OB_FLAG_PERMANENT_OBJECT  0x02
#define OB_FLAG_ATTACHED_OBJECT   0x04

#define IsHandleOnSegmentBoundary(Handle)     (((ULONG_PTR)(Handle) & (OB_HANDLES_PER_SEGMENT * sizeof(HANDLE) - 1)) == 0)
#define HandleToUlong(Handle)                 ((ULONG)(ULONG_PTR)(Handle))
#define UlongToHandle(Ulong)                  ((HANDLE)(ULONG_PTR)(Ulong))
#define EncodeFreeHandle(Handle)              (HandleToUlong(Handle) | 1)
#define DecodeFreeHandle(Handle)              (HandleToUlong(Handle) & ~1)
#define IsFreeHandle(Handle)                  (HandleToUlong(Handle) & 1)
#define GetObjHeader(Obj)                     (CONTAINING_RECORD(Obj, OBJECT_HEADER, Body))
#define GetTableByteOffsetFromHandle(Handle)  (HandleToUlong(Handle) & (OB_HANDLES_PER_TABLE * sizeof(PVOID) - 1))
#define GetTableFromHandle(Handle)            ObpObjectHandleTable.RootTable[HandleToUlong(Handle) >> (OB_HANDLES_PER_TABLE_SHIFT + 2)]
#define GetHandleContentsPointer(Handle)      ((PVOID *)((PUCHAR)GetTableFromHandle(Handle) + GetTableByteOffsetFromHandle(Handle)))

 // Macros to ensure thread safety
#define ObLock() KeRaiseIrqlToDpcLevel()
#define ObUnlock(Irql) KfLowerIrql(Irql)


BOOLEAN ObpAllocateNewHandleTable();
HANDLE ObpCreateHandleForObject(PVOID Object);
PVOID ObpDestroyObjectHandle(HANDLE Handle);
