/*
 * ergo720                Copyright (c) 2023
 */

#pragma once

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

#define FILE_DEVICE_CD_ROM              0x00000002
#define FILE_DEVICE_DISK                0x00000007
#define FILE_DEVICE_MEMORY_UNIT         0x0000003a
#define FILE_DEVICE_MEDIA_BOARD         0x0000003b

#define DO_RAW_MOUNT_ONLY               0x00000001
#define DO_EXCLUSIVE                    0x00000002
#define DO_DIRECT_IO                    0x00000004
#define DO_DEVICE_HAS_NAME              0x00000008
#define DO_DEVICE_INITIALIZING          0x00000010
#define DO_SCATTER_GATHER_IO            0x00000040

#define IRP_MJ_MAXIMUM_FUNCTION 0x1b


using PFILE_SEGMENT_ELEMENT = PVOID;
using PIO_TIMER = struct IO_TIMER *;
using DEVICE_TYPE = ULONG;
using PSECURITY_DESCRIPTOR = PVOID;

enum IO_ALLOCATION_ACTION {
	KeepObject = 1,
	DeallocateObject,
	DeallocateObjectKeepRegisters
};

using PDRIVER_CONTROL = IO_ALLOCATION_ACTION(XBOXAPI *)(
	struct DEVICE_OBJECT *DeviceObject,
	struct IRP *Irp,
	PVOID MapRegisterBase,
	PVOID Context
	);

using PDRIVER_ADD_DEVICE = NTSTATUS(XBOXAPI *)(
	struct DRIVER_OBJECT *DriverObject,
	struct DEVICE_OBJECT *PhysicalDeviceObject
	);

using PDRIVER_STARTIO = VOID(XBOXAPI *)(
	struct DEVICE_OBJECT *DeviceObject,
	struct IRP *Irp
	);

using PDRIVER_DISPATCH = NTSTATUS(XBOXAPI *)(
	struct DEVICE_OBJECT *DeviceObject,
	struct IRP *Irp
	);

using PDRIVER_DELETEDEVICE = VOID(XBOXAPI *)(
	struct DEVICE_OBJECT *DeviceObject
	);

using PDRIVER_DISMOUNTVOLUME = NTSTATUS(XBOXAPI *)(
	struct DEVICE_OBJECT *DeviceObject
	);

struct IO_COMPLETION_CONTEXT {
	PVOID Port;
	PVOID Key;
};
using PIO_COMPLETION_CONTEXT = IO_COMPLETION_CONTEXT *;

struct IO_STATUS_BLOCK {
	union {
		NTSTATUS Status;
		PVOID Pointer;
	};
	ULONG_PTR Information;
};
using PIO_STATUS_BLOCK = IO_STATUS_BLOCK *;

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

struct DEVICE_OBJECT {
	CSHORT Type;
	USHORT Size;
	LONG ReferenceCount;
	struct DRIVER_OBJECT *DriverObject;
	struct DEVICE_OBJECT *MountedOrSelfDevice;
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

struct KDEVICE_QUEUE_ENTRY {
	LIST_ENTRY DeviceListEntry;
	ULONG SortKey;
	BOOLEAN Inserted;
};
using PKDEVICE_QUEUE_ENTRY = KDEVICE_QUEUE_ENTRY *;

struct WAIT_CONTEXT_BLOCK {
	KDEVICE_QUEUE_ENTRY WaitQueueEntry;
	PDRIVER_CONTROL DeviceRoutine;
	PVOID DeviceContext;
	ULONG NumberOfMapRegisters;
	PVOID DeviceObject;
	PVOID CurrentIrp;
	PKDPC BufferChainingDpc;
};
using PWAIT_CONTEXT_BLOCK = WAIT_CONTEXT_BLOCK *;

struct DRIVER_EXTENSION {
	struct DRIVER_OBJECT *DriverObject;
	PDRIVER_ADD_DEVICE AddDevice;
	ULONG Count;
	UNICODE_STRING ServiceKeyName;
};
using PDRIVER_EXTENSION = DRIVER_EXTENSION *;

struct DRIVER_OBJECT {
	PDRIVER_STARTIO DriverStartIo;
	PDRIVER_DELETEDEVICE DriverDeleteDevice;
	PDRIVER_DISMOUNTVOLUME DriverDismountVolume;
	PDRIVER_DISPATCH MajorFunction[IRP_MJ_MAXIMUM_FUNCTION + 1];

};
using PDRIVER_OBJECT = DRIVER_OBJECT *;

struct PARTITION_INFORMATION {
	LARGE_INTEGER StartingOffset;
	LARGE_INTEGER PartitionLength;
	DWORD HiddenSectors;
	DWORD PartitionNumber;
	BYTE  PartitionType;
	BOOLEAN BootIndicator;
	BOOLEAN RecognizedPartition;
	BOOLEAN RewritePartition;
};
using PPARTITION_INFORMATION = PARTITION_INFORMATION *;

struct IDE_DISK_EXTENSION {
	PDEVICE_OBJECT DeviceObject;
	PARTITION_INFORMATION PartitionInformation;
};
using PIDE_DISK_EXTENSION = IDE_DISK_EXTENSION *;
