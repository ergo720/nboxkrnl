/*
 * ergo720                Copyright (c) 2022
 */

#pragma once

#include "types.hpp"


// We use i/o instructions to communicate with the host that's running us
// The list of i/o ports used on the xbox is at https://xboxdevwiki.net/Memory, so avoid using one of those

// Output debug strings to the host
#define DBG_OUTPUT_STR_PORT 0x0200
// Get the system type that we should use
#define KE_SYSTEM_TYPE 0x0201
// Request an abort interrupt to terminate execution
#define KE_ABORT 0x202
// Request the clock increment in 100 ns units since the last clock
#define KE_CLOCK_INCREMENT_LOW 0x203
#define KE_CLOCK_INCREMENT_HIGH 0x204
// Request the total execution time since booting in ms
#define KE_TIME_MS 0x205
// Submit a new I/O request
#define IO_START 0x206
// Retry submitting a I/O request
#define IO_RETRY 0x207
// Request the I/O request with the specified id
#define IO_QUERY 0x208
// Check if a I/O request was submitted successfully
#define IO_CHECK_ENQUEUE 0x20A
// Request the path's length of the XBE to launch when no reboot occurred
#define XE_XBE_PATH_LENGTH 0x20D
// Send the address where to put the path of the XBE to launch when no reboot occurred
#define XE_XBE_PATH_ADDR 0x20E
// Request the total ACPI time since booting
#define KE_ACPI_TIME_LOW 0x20F
#define KE_ACPI_TIME_HIGH 0x210

#define KERNEL_STACK_SIZE 12288
#define KERNEL_BASE 0x80010000

// Host device number
#define DEV_CDROM      0
#define DEV_UNUSED     1
#define DEV_PARTITION0 2
#define DEV_PARTITION1 3
#define DEV_PARTITION2 4
#define DEV_PARTITION3 5
#define DEV_PARTITION4 6
#define DEV_PARTITION5 7
#define DEV_PARTITION6 8 // non-standard
#define DEV_PARTITION7 9 // non-standard
#define NUM_OF_DEVS    10
#define DEV_TYPE(n)    ((n) << 23)

// Special host handles NOTE: these numbers must be less than 64 KiB - 1. This is because normal files use the address of a kernel file info struct as the host handle,
// and addresses below 64 KiB are forbidden on the xbox. This avoids collisions with these handles
#define CDROM_HANDLE      DEV_CDROM
#define UNUSED_HANDLE     DEV_UNUSED
#define PARTITION0_HANDLE DEV_PARTITION0
#define PARTITION1_HANDLE DEV_PARTITION1
#define PARTITION2_HANDLE DEV_PARTITION2
#define PARTITION3_HANDLE DEV_PARTITION3
#define PARTITION4_HANDLE DEV_PARTITION4
#define PARTITION5_HANDLE DEV_PARTITION5
#define PARTITION6_HANDLE DEV_PARTITION6
#define PARTITION7_HANDLE DEV_PARTITION7
#define FIRST_FREE_HANDLE NUM_OF_DEVS

#define LOWER_32(A) ((A) & 0xFFFFFFFF)
#define UPPER_32(A) ((A) >> 32)

enum ConsoleType {
	CONSOLE_XBOX,
	CONSOLE_CHIHIRO,
	CONSOLE_DEVKIT
};

enum IoRequestType : ULONG {
	Open       = 1 << 28,
	Remove     = 2 << 28,
	Close      = 3 << 28,
	Read       = 4 << 28,
	Write      = 5 << 28,
};

enum IoFlags : ULONG {
	MustBeADirectory    = 1 << 4,
	MustNotBeADirectory = 1 << 5
};

enum IoStatus : NTSTATUS {
	Success = 0,
	Pending,
	Error,
	Failed,
	IsADirectory,
	NotADirectory,
	NameNotFound,
	PathNotFound,
	Corrupt,
	CannotDelete,
	NotEmpty,
	Full
};

// Same as FILE_ macros of IO
enum IoInfo : ULONG {
	Superseded = 0,
	Opened,
	Created,
	Overwritten,
	Exists,
	NotExists
};

// Type layout of IoRequest
// IoRequestType - DevType - IoFlags - Disposition
// 31 - 28         27 - 23   22 - 3	   2 - 0

#pragma pack(1)
struct IoRequestHeader {
	ULONG Id; // unique id to identify this request
	ULONG Type; // type of request and flags
};

struct IoRequestXX {
	ULONG Handle; // file handle (it's the address of the kernel file info object that tracks the file)
};

// Specialized version of IoRequest for read/write requests only
struct IoRequestRw {
	LONGLONG Offset; // file offset from which to start the I/O
	ULONG Size; // bytes to transfer
	ULONG Address; // virtual address of the data to transfer
	ULONG Handle; // file handle (it's the address of the kernel file info object that tracks the file)
	ULONG Timestamp; // file timestamp
};

// Specialized version of IoRequest for open/create requests only
struct IoRequestOc {
	LONGLONG InitialSize; // file initial size
	ULONG Size; // size of file path
	ULONG Handle; // file handle (it's the address of the kernel file info object that tracks the file)
	ULONG Path; // file path address
	ULONG Attributes; // file attributes (only uses a single byte really)
	ULONG Timestamp; // file timestamp
	ULONG DesiredAccess; // the kind of access requested for the file
	ULONG CreateOptions; // how the create the file
};

struct IoRequest {
	IoRequestHeader Header;
	union {
		IoRequestOc m_oc;
		IoRequestRw m_rw;
		IoRequestXX m_xx;
	};
};

struct IoInfoBlock {
	ULONG Id; // id of the io request to query
	union {
		IoStatus HostStatus; // the final status of the request as received by the host
		NTSTATUS NtStatus; // HostStatus converted to a status suitable for the kernel
	};
	IoInfo Info; // request-specific information
	ULONG Ready; // set to 0 by the guest, then set to 1 by the host when the io request is complete
};

struct IoInfoBlockOc {
	IoInfoBlock Header;
	ULONG FileSize; // actual size of the opened file
	union {
		struct {
			ULONG FreeClusters; // number of free clusters remaining
			ULONG CreationTime; // timestamp when the file was created
			ULONG LastAccessTime; // timestamp when the file was read/written/executed
			ULONG LastWriteTime; // timestamp when the file was written/truncated/overwritten
		} Fatx;
		LONGLONG XdvdfsTimestamp; // only one timestamp for xdvdfs, because xiso is read-only
	};
};
#pragma pack()

struct XBOX_HARDWARE_INFO {
	ULONG Flags;
	UCHAR GpuRevision;
	UCHAR McpRevision;
	UCHAR Unknown3;
	UCHAR Unknown4;
};

struct XBOX_KRNL_VERSION {
	USHORT Major;
	USHORT Minor;
	USHORT Build;
	USHORT Qfe;
};

inline ConsoleType XboxType;
inline LONG IopHostRequestId = 0;
inline LONG IopHostFileHandle = FIRST_FREE_HANDLE;

#ifdef __cplusplus
extern "C" {
#endif

EXPORTNUM(322) DLLEXPORT extern XBOX_HARDWARE_INFO XboxHardwareInfo;

EXPORTNUM(324) DLLEXPORT extern XBOX_KRNL_VERSION XboxKrnlVersion;

DLLEXPORT extern const char *NboxkrnlVersion;

#ifdef __cplusplus
}
#endif

IoInfoBlock SubmitIoRequestToHost(ULONG Type, ULONG Handle);
IoInfoBlock SubmitIoRequestToHost(ULONG Type, LONGLONG Offset, ULONG Size, ULONG Address, ULONG Handle);
IoInfoBlock SubmitIoRequestToHost(ULONG Type, LONGLONG Offset, ULONG Size, ULONG Address, ULONG Handle, ULONG Timestamp);
IoInfoBlockOc SubmitIoRequestToHost(ULONG Type, LONGLONG InitialSize, ULONG Size, ULONG Handle, ULONG Path, ULONG Attributes, ULONG Timestamp,
	ULONG DesiredAccess, ULONG CreateOptions);
VOID KeSetSystemTime(PLARGE_INTEGER NewTime, PLARGE_INTEGER OldTime);

VOID InitializeListHead(PLIST_ENTRY pListHead);
BOOLEAN IsListEmpty(PLIST_ENTRY pListHead);
VOID InsertTailList(PLIST_ENTRY pListHead, PLIST_ENTRY pEntry);
VOID InsertHeadList(PLIST_ENTRY pListHead, PLIST_ENTRY pEntry);
VOID RemoveEntryList(PLIST_ENTRY pEntry);
PLIST_ENTRY RemoveTailList(PLIST_ENTRY pListHead);
PLIST_ENTRY RemoveHeadList(PLIST_ENTRY pListHead);
