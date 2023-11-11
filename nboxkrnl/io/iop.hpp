/*
 * ergo720                Copyright (c) 2023
 */

#pragma once

#include "io.hpp"
#include "..\ob\ob.hpp"
#include "..\ke\ke.hpp"


#define IO_TYPE_ADAPTER                  0x00000001
#define IO_TYPE_CONTROLLER               0x00000002
#define IO_TYPE_DEVICE                   0x00000003
#define IO_TYPE_DRIVER                   0x00000004
#define IO_TYPE_FILE                     0x00000005
#define IO_TYPE_IRP                      0x00000006
#define IO_TYPE_MASTER_ADAPTER           0x00000007
#define IO_TYPE_OPEN_PACKET              0x00000008
#define IO_TYPE_TIMER                    0x00000009
#define IO_TYPE_VPB                      0x0000000a
#define IO_TYPE_ERROR_LOG                0x0000000b
#define IO_TYPE_ERROR_MESSAGE            0x0000000c
#define IO_TYPE_DEVICE_OBJECT_EXTENSION  0x0000000d


using PFILE_SEGMENT_ELEMENT = PVOID;

struct IO_COMPLETION_CONTEXT {
	PVOID Port;
	PVOID Key;
};
using PIO_COMPLETION_CONTEXT = IO_COMPLETION_CONTEXT *;

struct IRP {
	CSHORT Type;
	WORD Size;
	ULONG Flags;
	LIST_ENTRY ThreadListEntry;
	IO_STATUS_BLOCK IoStatus;
	CHAR StackCount;
	CHAR CurrentLocation;
	UCHAR PendingReturned;
	UCHAR Cancel;
	PIO_STATUS_BLOCK UserIosb;
	PKEVENT UserEvent;
	ULONGLONG Overlay;
	PVOID UserBuffer;
	PFILE_SEGMENT_ELEMENT SegmentArray;
	ULONG LockedBufferLength;
	ULONGLONG Tail;            
};
using PIRP = IRP *;

struct KDEVICE_QUEUE {
	CSHORT Type;
	UCHAR Size;
	BOOLEAN Busy;
	LIST_ENTRY DeviceListHead;
};
using PKDEVICE_QUEUE = KDEVICE_QUEUE *;

struct DEVICE_OBJECT {
	CSHORT Type;
	USHORT Size;
	LONG ReferenceCount;
	struct _DRIVER_OBJECT *DriverObject;
	struct _DEVICE_OBJECT *MountedOrSelfDevice;
	PIRP CurrentIrp;
	ULONG Flags;
	PVOID DeviceExtension;
	UCHAR DeviceType;
	UCHAR StartIoFlags;
	CCHAR StackSize;
	BOOLEAN DeletePending;
	ULONG SectorSize;
	ULONG AlignmentRequirement;
	KDEVICE_QUEUE DeviceQueue;
	KEVENT DeviceLock;
	ULONG StartIoKey;
};
using PDEVICE_OBJECT = DEVICE_OBJECT *;

struct FILE_OBJECT {
	CSHORT Type;
	BOOLEAN DeletePending : 1;
	BOOLEAN ReadAccess : 1;
	BOOLEAN WriteAccess : 1;
	BOOLEAN DeleteAccess : 1;
	BOOLEAN SharedRead : 1;
	BOOLEAN SharedWrite : 1;
	BOOLEAN SharedDelete : 1;
	BOOLEAN Reserved : 1;
	UCHAR Flags;
	PDEVICE_OBJECT DeviceObject;
	PVOID FsContext;
	PVOID FsContext2;
	NTSTATUS FinalStatus;
	LARGE_INTEGER CurrentByteOffset;
	struct _FILE_OBJECT *RelatedFileObject;
	PIO_COMPLETION_CONTEXT CompletionContext;
	LONG LockCount;
	KEVENT Lock;
	KEVENT Event;
};
using PFILE_OBJECT = FILE_OBJECT *;

struct DUMMY_FILE_OBJECT {
	OBJECT_HEADER ObjectHeader;
	CHAR FileObjectBody[sizeof(FILE_OBJECT)];
};
using PDUMMY_FILE_OBJECT = DUMMY_FILE_OBJECT *;

struct FILE_NETWORK_OPEN_INFORMATION {
	LARGE_INTEGER CreationTime;
	LARGE_INTEGER LastAccessTime;
	LARGE_INTEGER LastWriteTime;
	LARGE_INTEGER ChangeTime;
	LARGE_INTEGER AllocationSize;
	LARGE_INTEGER EndOfFile;
	ULONG FileAttributes;
};
using PFILE_NETWORK_OPEN_INFORMATION = FILE_NETWORK_OPEN_INFORMATION *;

struct OPEN_PACKET {
	CSHORT Type;
	CSHORT Size;
	PFILE_OBJECT FileObject;
	NTSTATUS FinalStatus;
	ULONG_PTR Information;
	ULONG ParseCheck;
	PFILE_OBJECT RelatedFileObject;
	LARGE_INTEGER AllocationSize;
	ULONG CreateOptions;
	USHORT FileAttributes;
	USHORT ShareAccess;
	ULONG Options;
	ULONG Disposition;
	ULONG DesiredAccess;
	PFILE_NETWORK_OPEN_INFORMATION NetworkInformation;
	PDUMMY_FILE_OBJECT LocalFileObject;
	BOOLEAN QueryOnly;
	BOOLEAN DeleteOnly;
};
using POPEN_PACKET = OPEN_PACKET *;
