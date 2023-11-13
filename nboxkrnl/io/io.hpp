/*
 * ergo720                Copyright (c) 2023
 */

#pragma once

#include "iop.hpp"
#include "..\ob\ob.hpp"


#define IO_CHECK_CREATE_PARAMETERS 0x0200

#define FILE_ATTRIBUTE_VALID_FLAGS 0x00007fb7

#define FILE_VALID_OPTION_FLAGS 0x00ffffff

#define FILE_DIRECTORY_FILE             0x00000001
#define FILE_WRITE_THROUGH              0x00000002
#define FILE_SEQUENTIAL_ONLY            0x00000004
#define FILE_NO_INTERMEDIATE_BUFFERING  0x00000008
#define FILE_SYNCHRONOUS_IO_ALERT       0x00000010
#define FILE_SYNCHRONOUS_IO_NONALERT    0x00000020
#define FILE_NON_DIRECTORY_FILE         0x00000040
#define FILE_CREATE_TREE_CONNECTION     0x00000080
#define FILE_COMPLETE_IF_OPLOCKED       0x00000100
#define FILE_NO_EA_KNOWLEDGE            0x00000200
#define FILE_OPEN_FOR_RECOVERY          0x00000400
#define FILE_RANDOM_ACCESS              0x00000800
#define FILE_DELETE_ON_CLOSE            0x00001000
#define FILE_OPEN_BY_FILE_ID            0x00002000
#define FILE_OPEN_FOR_BACKUP_INTENT     0x00004000
#define FILE_NO_COMPRESSION             0x00008000
#define FILE_RESERVE_OPFILTER           0x00100000
#define FILE_OPEN_REPARSE_POINT         0x00200000
#define FILE_OPEN_NO_RECALL             0x00400000
#define FILE_OPEN_FOR_FREE_SPACE_QUERY  0x00800000

#define FILE_APPEND_DATA 0x0004

#define FILE_SHARE_READ         0x00000001
#define FILE_SHARE_WRITE        0x00000002
#define FILE_SHARE_DELETE       0x00000004
#define FILE_SHARE_VALID_FLAGS  0x00000007

#define DELETE        0x00010000L
#define READ_CONTROL  0x00020000L
#define WRITE_DAC     0x00040000L
#define WRITE_OWNER   0x00080000L
#define SYNCHRONIZE   0x00100000L

#define FILE_SUPERSEDE            0x00000000
#define FILE_OPEN                 0x00000001
#define FILE_CREATE               0x00000002
#define FILE_OPEN_IF              0x00000003
#define FILE_OVERWRITE            0x00000004
#define FILE_OVERWRITE_IF         0x00000005
#define FILE_MAXIMUM_DISPOSITION  FILE_OVERWRITE_IF

#define FILE_SUPERSEDE     0x00000000
#define FILE_OPEN          0x00000001
#define FILE_CREATE        0x00000002
#define FILE_OPEN_IF       0x00000003
#define FILE_OVERWRITE     0x00000004
#define FILE_OVERWRITE_IF  0x00000005


#ifdef __cplusplus
extern "C" {
#endif

EXPORTNUM(65) DLLEXPORT NTSTATUS XBOXAPI IoCreateDevice
(
	PDRIVER_OBJECT DriverObject,
	ULONG DeviceExtensionSize,
	PSTRING DeviceName,
	ULONG DeviceType,
	BOOLEAN Exclusive,
	PDEVICE_OBJECT *DeviceObject
);

EXPORTNUM(66) DLLEXPORT NTSTATUS XBOXAPI IoCreateFile
(
	PHANDLE FileHandle,
	ACCESS_MASK DesiredAccess,
	POBJECT_ATTRIBUTES ObjectAttributes,
	PIO_STATUS_BLOCK IoStatusBlock,
	PLARGE_INTEGER AllocationSize,
	ULONG FileAttributes,
	ULONG ShareAccess,
	ULONG Disposition,
	ULONG CreateOptions,
	ULONG Options
);

EXPORTNUM(70) DLLEXPORT extern OBJECT_TYPE IoDeviceObjectType;

EXPORTNUM(74) DLLEXPORT NTSTATUS XBOXAPI IoInvalidDeviceRequest
(
	PDEVICE_OBJECT DeviceObject,
	PIRP Irp
);

#ifdef __cplusplus
}
#endif

BOOLEAN IoInitSystem();
NTSTATUS XBOXAPI IoParseDevice(PVOID ParseObject, POBJECT_TYPE ObjectType, ULONG Attributes, POBJECT_STRING Name, POBJECT_STRING RemainderName,
	PVOID Context, PVOID *Object);
