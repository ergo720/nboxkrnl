/*
 * ergo720                Copyright (c) 2023
 */

#pragma once

#include "ob.hpp"

// Object attributes specified in OBJECT_ATTRIBUTES
#define OBJ_PERMANENT  0x00000010L
#define OBJ_OPENIF     0x00000080L

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
#define GetObjDirInfoHeader(Obj)              ((POBJECT_HEADER_NAME_INFO)(GetObjHeader(Obj)) - 1)
#define GetObjectFromDirInfoHeader(Obj)       (&((POBJECT_HEADER)((POBJECT_HEADER_NAME_INFO)(Obj) + 1))->Body)
#define GetTableByteOffsetFromHandle(Handle)  (HandleToUlong(Handle) & (OB_HANDLES_PER_TABLE * sizeof(PVOID) - 1))
#define GetTableFromHandle(Handle)            ObpObjectHandleTable.RootTable[HandleToUlong(Handle) >> (OB_HANDLES_PER_TABLE_SHIFT + 2)]
#define GetHandleContentsPointer(Handle)      ((PVOID *)((PUCHAR)GetTableFromHandle(Handle) + GetTableByteOffsetFromHandle(Handle)))

#define InitializeObjectAttributes(p, n, a, r){ \
	(p)->RootDirectory = r;                     \
	(p)->Attributes = a;                        \
	(p)->ObjectName = n;                        \
}

#define INITIALIZE_GLOBAL_OBJECT_STRING(ObString, String, ...) \
    CHAR ObString##Buffer[] = String;                          \
    __VA_ARGS__ OBJECT_STRING ObString = {                     \
        sizeof(String) - sizeof(CHAR),                         \
        sizeof(String),                                        \
        ObString##Buffer                                       \
    }

 // Macros to ensure thread safety
#define ObLock() KeRaiseIrqlToDpcLevel()
#define ObUnlock(Irql) KfLowerIrql(Irql)


inline POBJECT_DIRECTORY ObpRootDirectoryObject;
inline POBJECT_DIRECTORY ObpDosDevicesDirectoryObject;
inline POBJECT_DIRECTORY ObpWin32NamedObjectsDirectoryObject;
inline INITIALIZE_GLOBAL_OBJECT_STRING(ObpDosDevicesString, "\\??", inline);
inline INITIALIZE_GLOBAL_OBJECT_STRING(ObpWin32NamedObjectsString, "\\Win32NamedObjects", inline);
inline PVOID ObpDosDevicesDriveLetterArray['Z' - 'A' + 1] = { 0 };

BOOLEAN ObpAllocateNewHandleTable();
HANDLE ObpCreateHandleForObject(PVOID Object);
PVOID ObpDestroyObjectHandle(HANDLE Handle);
template<bool ShouldReference>
PVOID ObpGetObjectFromHandle(HANDLE Handle);
NTSTATUS ObpReferenceObjectByName(POBJECT_ATTRIBUTES ObjectAttributes, POBJECT_TYPE ObjectType, PVOID ParseContext, PVOID *ReturnedObject);
BOOLEAN ObpCreatePermanentObjects();
VOID ObpParseName(POBJECT_STRING Name, POBJECT_STRING FirstName, POBJECT_STRING RemainderName);
NTSTATUS ObpResolveRootHandlePath(POBJECT_ATTRIBUTES ObjectAttributes, POBJECT_DIRECTORY *Directory);
PVOID ObpFindObjectInDirectory(POBJECT_DIRECTORY Directory, POBJECT_STRING Name, BOOLEAN ShouldFollowSymbolicLinks);
ULONG ObpGetObjectHashIndex(POBJECT_STRING Name);
VOID XBOXAPI ObpDeleteSymbolicLink(PVOID Object);
