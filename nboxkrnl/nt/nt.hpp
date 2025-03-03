/*
 * ergo720                Copyright (c) 2023
 */

#pragma once

#include "..\types.hpp"
#include "io.hpp"


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

EXPORTNUM(190) DLLEXPORT NTSTATUS XBOXAPI NtCreateFile
(
	PHANDLE FileHandle,
	ACCESS_MASK DesiredAccess,
	POBJECT_ATTRIBUTES ObjectAttributes,
	PIO_STATUS_BLOCK IoStatusBlock,
	PLARGE_INTEGER AllocationSize,
	ULONG FileAttributes,
	ULONG ShareAccess,
	ULONG CreateDisposition,
	ULONG CreateOptions
);

EXPORTNUM(192) DLLEXPORT NTSTATUS XBOXAPI NtCreateMutant
(
	PHANDLE MutantHandle,
	POBJECT_ATTRIBUTES ObjectAttributes,
	BOOLEAN InitialOwner
);

EXPORTNUM(196) DLLEXPORT NTSTATUS XBOXAPI NtDeviceIoControlFile
(
	HANDLE FileHandle,
	HANDLE Event,
	PIO_APC_ROUTINE ApcRoutine,
	PVOID ApcContext,
	PIO_STATUS_BLOCK IoStatusBlock,
	ULONG IoControlCode,
	PVOID InputBuffer,
	ULONG InputBufferLength,
	PVOID OutputBuffer,
	ULONG OutputBufferLength
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

EXPORTNUM(203) DLLEXPORT NTSTATUS XBOXAPI NtOpenSymbolicLinkObject
(
	PHANDLE LinkHandle,
	POBJECT_ATTRIBUTES ObjectAttributes
);

EXPORTNUM(204) DLLEXPORT NTSTATUS XBOXAPI NtProtectVirtualMemory
(
	PVOID *BaseAddress,
	PSIZE_T RegionSize,
	ULONG NewProtect,
	PULONG OldProtect
);

EXPORTNUM(211) DLLEXPORT NTSTATUS XBOXAPI NtQueryInformationFile
(
	HANDLE FileHandle,
	PIO_STATUS_BLOCK IoStatusBlock,
	PVOID FileInformation,
	ULONG Length,
	FILE_INFORMATION_CLASS FileInformationClass
);

EXPORTNUM(218) DLLEXPORT NTSTATUS XBOXAPI NtQueryVolumeInformationFile
(
	HANDLE FileHandle,
	PIO_STATUS_BLOCK IoStatusBlock,
	PFILE_FS_SIZE_INFORMATION FileInformation,
	ULONG Length,
	FS_INFORMATION_CLASS FileInformationClass
);

EXPORTNUM(219) DLLEXPORT NTSTATUS XBOXAPI NtReadFile
(
	HANDLE FileHandle,
	HANDLE Event,
	PIO_APC_ROUTINE ApcRoutine,
	PVOID ApcContext,
	PIO_STATUS_BLOCK IoStatusBlock,
	PVOID Buffer,
	ULONG Length,
	PLARGE_INTEGER ByteOffset
);

EXPORTNUM(221) DLLEXPORT NTSTATUS XBOXAPI NtReleaseMutant
(
	HANDLE MutantHandle,
	PLONG PreviousCount
);

EXPORTNUM(232) DLLEXPORT VOID XBOXAPI NtUserIoApcDispatcher
(
	PVOID ApcContext,
	PIO_STATUS_BLOCK IoStatusBlock,
	ULONG Reserved
);

EXPORTNUM(233) DLLEXPORT NTSTATUS XBOXAPI NtWaitForSingleObject
(
	HANDLE Handle,
	BOOLEAN Alertable,
	PLARGE_INTEGER Timeout
);

EXPORTNUM(234) DLLEXPORT NTSTATUS XBOXAPI NtWaitForSingleObjectEx
(
	HANDLE Handle,
	KPROCESSOR_MODE WaitMode,
	BOOLEAN Alertable,
	PLARGE_INTEGER Timeout
);

EXPORTNUM(236) DLLEXPORT NTSTATUS XBOXAPI NtWriteFile
(
	HANDLE FileHandle,
	HANDLE Event,
	PIO_APC_ROUTINE ApcRoutine,
	PVOID ApcContext,
	PIO_STATUS_BLOCK IoStatusBlock,
	PVOID Buffer,
	ULONG Length,
	PLARGE_INTEGER ByteOffset
);

#ifdef __cplusplus
}
#endif
