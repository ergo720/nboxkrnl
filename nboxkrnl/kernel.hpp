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
// Request the path's length of the XBE to launch when no reboot occured
#define XE_XBE_PATH_LENGTH 0x20D
// Send the address where to put the path of the XBE to launch when no reboot occured
#define XE_XBE_PATH_ADDR 0x20E
// Request the total ACPI time since booting
#define KE_ACPI_TIME_LOW 0x20F
#define KE_ACPI_TIME_HIGH 0x210

#define KERNEL_STACK_SIZE 12288
#define KERNEL_BASE 0x80010000

// Host device number
#define DEV_CDROM      0
#define DEV_EEPROM     1
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

// Special host handles
#define UNUSED_HANDLE
#define EEPROM_HANDLE     DEV_EEPROM
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

enum SystemType {
	SYSTEM_XBOX,
	SYSTEM_CHIHIRO,
	SYSTEM_DEVKIT
};

enum IoRequestType : ULONG {
	Open       = 1 << 28,
	Remove     = 2 << 28,
	Close      = 3 << 28,
	Read       = 4 << 28,
	Write      = 5 << 28,
};

enum IoFlags : ULONG {
	IsDirectory         = 1 << 3,
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
	NotFound
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
struct IoRequest {
	ULONGLONG Id; // unique id to identify this request
	ULONG Type; // type of request and flags
	LONGLONG OffsetOrInitialSize; // file offset from which to start the I/O or file initial size
	ULONG Size; // bytes to transfer or size of path for open/create requests
	ULONGLONG HandleOrAddress; // virtual address of the data to transfer or file handle for open/create requests
	ULONGLONG HandleOrPath; // file handle or file path for open/create requests
};

struct IoInfoBlock {
	IoStatus Status;
	IoInfo Info;
	uint64_t Info2OrId; // extra info or id of the io request to query
	uint32_t Ready; // set to 0 by the guest, then set to 1 by the host when the io request is complete
};
#pragma pack()

struct XBOX_HARDWARE_INFO {
	ULONG Flags;
	UCHAR GpuRevision;
	UCHAR McpRevision;
	UCHAR Unknown3;
	UCHAR Unknown4;
};

inline SystemType XboxType;
inline ULONGLONG IoRequestId = 0;
inline ULONGLONG IoHostFileHandle = FIRST_FREE_HANDLE;

#ifdef __cplusplus
extern "C" {
#endif

EXPORTNUM(322) DLLEXPORT extern XBOX_HARDWARE_INFO XboxHardwareInfo;

#ifdef __cplusplus
}
#endif

IoInfoBlock SubmitIoRequestToHost(ULONG Type, LONGLONG OffsetOrInitialSize, ULONG Size, ULONGLONG HandleOrAddress, ULONGLONG HandleOrPath);
ULONGLONG FASTCALL InterlockedIncrement64(volatile PULONGLONG Addend);
NTSTATUS HostToNtStatus(IoStatus Status);
VOID KeSetSystemTime(PLARGE_INTEGER NewTime, PLARGE_INTEGER OldTime);

VOID InitializeListHead(PLIST_ENTRY pListHead);
BOOLEAN IsListEmpty(PLIST_ENTRY pListHead);
VOID InsertTailList(PLIST_ENTRY pListHead, PLIST_ENTRY pEntry);
VOID InsertHeadList(PLIST_ENTRY pListHead, PLIST_ENTRY pEntry);
VOID RemoveEntryList(PLIST_ENTRY pEntry);
PLIST_ENTRY RemoveTailList(PLIST_ENTRY pListHead);
PLIST_ENTRY RemoveHeadList(PLIST_ENTRY pListHead);

static inline VOID CDECL outl(USHORT Port, ULONG Value)
{
	__asm {
		mov eax, Value
		mov dx, Port
		out dx, eax
	}
}

static inline VOID CDECL outw(USHORT Port, USHORT Value)
{
	__asm {
		mov ax, Value
		mov dx, Port
		out dx, ax
	}
}

static inline VOID CDECL outb(USHORT Port, BYTE Value)
{
	__asm {
		mov al, Value
		mov dx, Port
		out dx, al
	}
}

static inline ULONG CDECL inl(USHORT Port)
{
	__asm {
		mov dx, Port
		in eax, dx
	}
}

static inline USHORT CDECL inw(USHORT Port)
{
	__asm {
		mov dx, Port
		in ax, dx
	}
}

static inline BYTE CDECL inb(USHORT Port)
{
	__asm {
		mov dx, Port
		in al, dx
	}
}

static inline VOID CDECL enable()
{
	__asm sti
}

static inline VOID CDECL disable()
{
	__asm cli
}
