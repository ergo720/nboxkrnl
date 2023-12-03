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
// Request the total execution time since booting in 100 ns units
#define KE_TIME_LOW 0x203
#define KE_TIME_HIGH 0x204
// Request the total execution time since booting in ms
#define KE_TIME_MS 0x205
// Submit a new I/O request
#define IO_START 0x206
// Retry submitting a I/O request
#define IO_RETRY 0x207
// Request the Status member of IoInfoBlock
#define IO_QUERY_STATUS 0x208
// Request the Info member of IoInfoBlock
#define IO_QUERY_INFO 0x209
// Check if a I/O request was submitted successfully
#define IO_CHECK_ENQUEUE 0x20A
// Set the id of the I/O request to query
#define IO_SET_ID_LOW 0x20B
#define IO_SET_ID_HIGH 0x20C
// Request the name's length of the first XBE to launch from the DVD drive
#define XE_DVD_XBE_LENGTH 0x20D
// Send the address where to put the name of the first XBE to launch from the DVD drive
#define XE_DVD_XBE_ADDR 0x20E
// Request the total ACPI time since booting
#define KE_ACPI_TIME_LOW 0x20F
#define KE_ACPI_TIME_HIGH 0x210

#define KERNEL_STACK_SIZE 12288
#define KERNEL_BASE 0x80010000

// Special host handles
#define XBE_HANDLE ((ULONGLONG)0)
#define EEPROM_HANDLE ((ULONGLONG)1)
#define LAST_NON_FREE_HANDLE EEPROM_HANDLE

enum SystemType {
	SYSTEM_XBOX,
	SYSTEM_CHIHIRO,
	SYSTEM_DEVKIT
};

enum IoRequestType : ULONG {
	Open =   1 << 28,
	Remove = 2 << 28,
	Close =  3 << 28,
	Read =   4 << 28,
	Write =  5 << 28
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
// IoRequestType - IoFlags - Disposition
// 31 - 28         27 - 3    2 - 0

#pragma pack(1)
struct IoRequest {
	ULONGLONG Id; // unique id to identify this request
	ULONG Type; // type of request and flags
	LONGLONG Offset; // file offset from which to start the I/O
	ULONG Size; // bytes to transfer or size of path for open/create requests
	ULONGLONG HandleOrAddress; // virtual address of the data to transfer or file handle for open/create requests
	ULONGLONG HandleOrPath; // file handle or file path for open/create requests
};
#pragma pack()

struct IoInfoBlock {
	IoStatus Status;
	IoInfo Info;
};

struct XBOX_HARDWARE_INFO {
	ULONG Flags;
	UCHAR GpuRevision;
	UCHAR McpRevision;
	UCHAR Unknown3;
	UCHAR Unknown4;
};

inline SystemType XboxType;
inline ULONGLONG IoRequestId = 0;
inline ULONGLONG IoHostFileHandle = LAST_NON_FREE_HANDLE;

#ifdef __cplusplus
extern "C" {
#endif

EXPORTNUM(322) DLLEXPORT extern XBOX_HARDWARE_INFO XboxHardwareInfo;

#ifdef __cplusplus
}
#endif

VOID FASTCALL SubmitIoRequestToHost(IoRequest *Request);
VOID FASTCALL RetrieveIoRequestFromHost(IoInfoBlock *Info, ULONGLONG Id);
ULONGLONG FASTCALL InterlockedIncrement64(volatile PULONGLONG Addend);

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

static inline ULONG CDECL inl(USHORT Port)
{
	__asm {
		mov dx, Port
		in eax, dx
	}
}
