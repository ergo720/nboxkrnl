/*
 * ergo720                Copyright (c) 2023
 */

#pragma once

#include "..\types.hpp"
#include "..\io\io.hpp"


#ifdef __cplusplus
extern "C" {
#endif

EXPORTNUM(184) DLLEXPORT NTSTATUS XBOXAPI NtAllocateVirtualMemory
(
	PVOID *BaseAddress,
	ULONG ZeroBits,
	PULONG AllocationSize,
	DWORD AllocationType,
	DWORD Protect
);

EXPORTNUM(187) DLLEXPORT NTSTATUS XBOXAPI NtClose
(
	HANDLE Handle
);

EXPORTNUM(188) DLLEXPORT NTSTATUS XBOXAPI NtCreateDirectoryObject
(
	PHANDLE DirectoryHandle,
	POBJECT_ATTRIBUTES ObjectAttributes
);

EXPORTNUM(199) DLLEXPORT NTSTATUS XBOXAPI NtFreeVirtualMemory
(
	PVOID *BaseAddress,
	PULONG FreeSize,
	ULONG FreeType
);

EXPORTNUM(202) DLLEXPORT NTSTATUS XBOXAPI NtOpenFile
(
	PHANDLE FileHandle,
	ACCESS_MASK DesiredAccess,
	POBJECT_ATTRIBUTES ObjectAttributes,
	PIO_STATUS_BLOCK IoStatusBlock,
	ULONG ShareAccess,
	ULONG OpenOptions
);

EXPORTNUM(204) DLLEXPORT NTSTATUS XBOXAPI NtProtectVirtualMemory
(
	PVOID *BaseAddress,
	PSIZE_T RegionSize,
	ULONG NewProtect,
	PULONG OldProtect
);

#ifdef __cplusplus
}
#endif
