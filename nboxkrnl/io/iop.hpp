/*
 * ergo720                Copyright (c) 2023
 */

#pragma once

#include "..\ob\ob.hpp"
#include "..\ke\ke.hpp"


#define OPEN_PACKET_PATTERN              0xbeaa0251

#define FO_SYNCHRONOUS_IO                0x00000002
#define FO_ALERTABLE_IO                  0x00000004
#define FO_HANDLE_CREATED                0x00000020

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
#define FILE_DEVICE_DISK_FILE_SYSTEM    0x00000008
#define FILE_DEVICE_MEMORY_UNIT         0x0000003a
#define FILE_DEVICE_MEDIA_BOARD         0x0000003b

#define DO_RAW_MOUNT_ONLY               0x00000001
#define DO_EXCLUSIVE                    0x00000002
#define DO_DIRECT_IO                    0x00000004
#define DO_DEVICE_HAS_NAME              0x00000008
#define DO_DEVICE_INITIALIZING          0x00000010
#define DO_SCATTER_GATHER_IO            0x00000040

#define SL_CASE_SENSITIVE               0x00000080
#define SL_OPEN_TARGET_DIRECTORY        0x00000004 // same as IO_OPEN_TARGET_DIRECTORY used by IoCreateFile

#define SL_PENDING_RETURNED              0x01
#define SL_MUST_COMPLETE                 0x02
#define SL_INVOKE_ON_CANCEL              0x20
#define SL_INVOKE_ON_SUCCESS             0x40
#define SL_INVOKE_ON_ERROR               0x80

#define IRP_NOCACHE                     0x00000001
#define IRP_MOUNT_COMPLETION            0x00000002
#define IRP_SYNCHRONOUS_API             0x00000004
#define IRP_CREATE_OPERATION            0x00000008
#define IRP_READ_OPERATION              0x00000010
#define IRP_WRITE_OPERATION             0x00000020
#define IRP_CLOSE_OPERATION             0x00000040
#define IRP_DEFER_IO_COMPLETION         0x00000080
#define IRP_OB_QUERY_NAME               0x00000100
#define IRP_UNLOCK_USER_BUFFER          0x00000200
#define IRP_SCATTER_GATHER_OPERATION    0x00000400
#define IRP_UNMAP_SEGMENT_ARRAY         0x00000800
#define IRP_NO_CANCELIO                 0x00001000

#define IRP_MJ_CREATE                    0x00
#define IRP_MJ_CREATE_NAMED_PIPE         0x01
#define IRP_MJ_CLOSE                     0x02
#define IRP_MJ_READ                      0x03
#define IRP_MJ_WRITE                     0x04
#define IRP_MJ_QUERY_INFORMATION         0x05
#define IRP_MJ_SET_INFORMATION           0x06
#define IRP_MJ_QUERY_EA                  0x07
#define IRP_MJ_SET_EA                    0x08
#define IRP_MJ_FLUSH_BUFFERS             0x09
#define IRP_MJ_QUERY_VOLUME_INFORMATION  0x0a
#define IRP_MJ_SET_VOLUME_INFORMATION    0x0b
#define IRP_MJ_DIRECTORY_CONTROL         0x0c
#define IRP_MJ_FILE_SYSTEM_CONTROL       0x0d
#define IRP_MJ_DEVICE_CONTROL            0x0e
#define IRP_MJ_INTERNAL_DEVICE_CONTROL   0x0f
#define IRP_MJ_SHUTDOWN                  0x10
#define IRP_MJ_LOCK_CONTROL              0x11
#define IRP_MJ_CLEANUP                   0x12
#define IRP_MJ_CREATE_MAILSLOT           0x13
#define IRP_MJ_QUERY_SECURITY            0x14
#define IRP_MJ_SET_SECURITY              0x15
#define IRP_MJ_POWER                     0x16
#define IRP_MJ_SYSTEM_CONTROL            0x17
#define IRP_MJ_DEVICE_CHANGE             0x18
#define IRP_MJ_QUERY_QUOTA               0x19
#define IRP_MJ_SET_QUOTA                 0x1a
#define IRP_MJ_PNP                       0x1b
#define IRP_MJ_MAXIMUM_FUNCTION          0x1b


using PFILE_SEGMENT_ELEMENT = PVOID;
using PIO_TIMER = struct IO_TIMER *;
using DEVICE_TYPE = ULONG;
using PSECURITY_DESCRIPTOR = PVOID;

enum IO_ALLOCATION_ACTION {
	KeepObject = 1,
	DeallocateObject,
	DeallocateObjectKeepRegisters
};

enum MEDIA_TYPE {
	FixedMedia = 12
};

enum FILE_INFORMATION_CLASS {
	FileBasicInformation = 4,
	FileStandardInformation = 5,
	FilePositionInformation = 14,
	FileEndOfFileInformation = 20,
};
using PFILE_INFORMATION_CLASS = FILE_INFORMATION_CLASS *;

enum FS_INFORMATION_CLASS {
	FileFsVolumeInformation = 1,
	FileFsLabelInformation,
	FileFsSizeInformation,
	FileFsDeviceInformation,
	FileFsAttributeInformation,
	FileFsControlInformation,
	FileFsFullSizeInformation,
	FileFsObjectIdInformation,
	FileFsMaximumInformation
};
using PFS_INFORMATION_CLASS = FS_INFORMATION_CLASS *;

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

using PIO_COMPLETION_ROUTINE = NTSTATUS(XBOXAPI *)(
	struct DEVICE_OBJECT *DeviceObject,
	struct IRP *Irp,
	PVOID Context
	);

using PIO_APC_ROUTINE = VOID(XBOXAPI *)(
	PVOID ApcContext,
	struct IO_STATUS_BLOCK *IoStatusBlock,
	ULONG Reserved
	);

using PDRIVER_CANCEL = VOID(XBOXAPI *)(
	struct DEVICE_OBJECT *DeviceObject,
	struct IRP *Irp
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

struct FILE_FS_VOLUME_INFORMATION {
	LARGE_INTEGER VolumeCreationTime;
	ULONG VolumeSerialNumber;
	ULONG VolumeLabelLength;
	BOOLEAN SupportsObjects;
	CHAR VolumeLabel[1];
};
using PFILE_FS_VOLUME_INFORMATION = FILE_FS_VOLUME_INFORMATION *;

struct FILE_FS_SIZE_INFORMATION {
	LARGE_INTEGER TotalAllocationUnits;
	LARGE_INTEGER AvailableAllocationUnits;
	ULONG SectorsPerAllocationUnit;
	ULONG BytesPerSector;
};
using PFILE_FS_SIZE_INFORMATION = FILE_FS_SIZE_INFORMATION *;

struct FILE_FS_DEVICE_INFORMATION {
	DEVICE_TYPE DeviceType;
	ULONG Characteristics;
};
using PFILE_FS_DEVICE_INFORMATION = FILE_FS_DEVICE_INFORMATION *;

struct FILE_FS_ATTRIBUTE_INFORMATION {
	ULONG FileSystemAttributes;
	LONG MaximumComponentNameLength;
	ULONG FileSystemNameLength;
	WCHAR FileSystemName[1];
};
using PFILE_FS_ATTRIBUTE_INFORMATION = FILE_FS_ATTRIBUTE_INFORMATION *;

struct FILE_FS_CONTROL_INFORMATION {
	LARGE_INTEGER FreeSpaceStartFiltering;
	LARGE_INTEGER FreeSpaceThreshold;
	LARGE_INTEGER FreeSpaceStopFiltering;
	LARGE_INTEGER DefaultQuotaThreshold;
	LARGE_INTEGER DefaultQuotaLimit;
	ULONG FileSystemControlFlags;
};
using PFILE_FS_CONTROL_INFORMATION = FILE_FS_CONTROL_INFORMATION *;

struct FILE_FS_FULL_SIZE_INFORMATION {
	LARGE_INTEGER TotalAllocationUnits;
	LARGE_INTEGER CallerAvailableAllocationUnits;
	LARGE_INTEGER ActualAvailableAllocationUnits;
	ULONG SectorsPerAllocationUnit;
	ULONG BytesPerSector;
};
using PFILE_FS_FULL_SIZE_INFORMATION = FILE_FS_FULL_SIZE_INFORMATION *;

struct FILE_FS_OBJECTID_INFORMATION {
	UCHAR ObjectId[16];
	UCHAR ExtendedInfo[48];
};
using PFILE_FS_OBJECTID_INFORMATION = FILE_FS_OBJECTID_INFORMATION *;

struct DEVICE_OBJECT {
	CSHORT Type;
	USHORT Size;
	LONG ReferenceCount;
	struct DRIVER_OBJECT *DriverObject;
	struct DEVICE_OBJECT *MountedOrSelfDevice;
	struct IRP *CurrentIrp;
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
	struct FILE_OBJECT *RelatedFileObject;
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

struct DISK_GEOMETRY {
	LARGE_INTEGER Cylinders;
	MEDIA_TYPE MediaType;
	DWORD TracksPerCylinder;
	DWORD SectorsPerTrack;
	DWORD BytesPerSector;
};
using PDISK_GEOMETRY = DISK_GEOMETRY *;

struct IO_STACK_LOCATION {
	UCHAR MajorFunction;
	UCHAR MinorFunction;
	UCHAR Flags;
	UCHAR Control;

	union {
		struct {
			ACCESS_MASK DesiredAccess;
			ULONG Options;
			USHORT FileAttributes;
			USHORT ShareAccess;
			POBJECT_STRING RemainingName;
		} Create;

		struct {
			ULONG Length;
			union {
				ULONG BufferOffset;
				PVOID CacheBuffer;
			};
			LARGE_INTEGER ByteOffset;
		} Read;

		struct {
			ULONG Length;
			union {
				ULONG BufferOffset;
				PVOID CacheBuffer;
			};
			LARGE_INTEGER ByteOffset;
		} Write;

		struct {
			ULONG Length;
			POBJECT_STRING FileName;
			FILE_INFORMATION_CLASS FileInformationClass;
		} QueryDirectory;

		struct {
			ULONG Length;
			FILE_INFORMATION_CLASS FileInformationClass;
		} QueryFile;

		struct {
			ULONG Length;
			FILE_INFORMATION_CLASS FileInformationClass;
			PFILE_OBJECT FileObject;
		} SetFile;

		struct {
			ULONG Length;
			FS_INFORMATION_CLASS FsInformationClass;
		} QueryVolume;

		struct {
			ULONG Length;
			FS_INFORMATION_CLASS FsInformationClass;
		} SetVolume;

		struct {
			ULONG OutputBufferLength;
			PVOID InputBuffer;
			ULONG InputBufferLength;
			ULONG FsControlCode;
		} FileSystemControl;

		struct {
			ULONG OutputBufferLength;
			PVOID InputBuffer;
			ULONG InputBufferLength;
			ULONG IoControlCode;
		} DeviceIoControl;

		struct {
			struct _SCSI_REQUEST_BLOCK *Srb;
		} Scsi;

		struct {
			ULONG Length;
			PUCHAR Buffer;
			ULONG SectorNumber;
			ULONG BufferOffset;
		} IdexReadWrite;

		struct {
			PVOID Argument1;
			PVOID Argument2;
			PVOID Argument3;
			PVOID Argument4;
		} Others;

	} Parameters;

	PDEVICE_OBJECT DeviceObject;
	PFILE_OBJECT FileObject;
	PIO_COMPLETION_ROUTINE CompletionRoutine;
	PVOID Context;
};
using PIO_STACK_LOCATION = IO_STACK_LOCATION *;

struct IRP {
	CSHORT Type;
	USHORT Size;
	ULONG Flags;
	LIST_ENTRY ThreadListEntry;
	IO_STATUS_BLOCK IoStatus;
	CHAR StackCount;
	CHAR CurrentLocation;
	BOOLEAN PendingReturned;
	BOOLEAN Cancel;
	PIO_STATUS_BLOCK UserIosb;
	PKEVENT UserEvent;
	union {
		struct {
			PIO_APC_ROUTINE UserApcRoutine;
			PVOID UserApcContext;
		} AsynchronousParameters;
		LARGE_INTEGER AllocationSize;
	} Overlay;
	PVOID UserBuffer;
	PFILE_SEGMENT_ELEMENT SegmentArray;
	ULONG LockedBufferLength;
	union {
		struct {
			union {
				KDEVICE_QUEUE_ENTRY DeviceQueueEntry;
				struct {
					PVOID DriverContext[5];
				};
			};
			PETHREAD Thread;
			struct {
				LIST_ENTRY ListEntry;
				union {
					PIO_STACK_LOCATION CurrentStackLocation;
					ULONG PacketType;
				};
			};
			PFILE_OBJECT OriginalFileObject;
		} Overlay;
		KAPC Apc;
		PVOID CompletionKey;
	} Tail;
};
using PIRP = IRP *;


extern const UCHAR IopValidFsInformationQueries[];
extern const ULONG IopQueryFsOperationAccess[];

NTSTATUS IopMountDevice(PDEVICE_OBJECT DeviceObject);
VOID IopDereferenceDeviceObject(PDEVICE_OBJECT DeviceObject);
VOID IopQueueThreadIrp(PIRP Irp);
VOID IopDequeueThreadIrp(PIRP Irp);
VOID ZeroIrpStackLocation(PIO_STACK_LOCATION IrpStackPointer);
VOID IoMarkIrpPending(PIRP Irp);
PIO_STACK_LOCATION IoGetCurrentIrpStackLocation(PIRP Irp);
PIO_STACK_LOCATION IoGetNextIrpStackLocation(PIRP Irp);
VOID XBOXAPI IopCloseFile(PVOID Object, ULONG SystemHandleCount);
VOID XBOXAPI IopDeleteFile(PVOID Object);
NTSTATUS XBOXAPI IopParseFile(PVOID ParseObject, POBJECT_TYPE ObjectType, ULONG Attributes, POBJECT_STRING CompleteName, POBJECT_STRING RemainingName,
	PVOID Context, PVOID *Object);
VOID XBOXAPI IopCompleteRequest(PKAPC Apc, PKNORMAL_ROUTINE *NormalRoutine, PVOID *NormalContext, PVOID *SystemArgument1, PVOID *SystemArgument2);
VOID IopDropIrp(PIRP Irp, PFILE_OBJECT FileObject);
VOID IopAcquireSynchronousFileLock(PFILE_OBJECT FileObject);
VOID IopReleaseSynchronousFileLock(PFILE_OBJECT FileObject);
NTSTATUS IopCleanupFailedIrpAllocation(PFILE_OBJECT FileObject, PKEVENT EventObject);
NTSTATUS IopSynchronousService(PDEVICE_OBJECT DeviceObject, PIRP Irp, PFILE_OBJECT FileObject, BOOLEAN DeferredIoCompletion, BOOLEAN SynchronousIo);
