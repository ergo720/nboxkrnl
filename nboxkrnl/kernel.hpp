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
#define IO_SET_ID 0x20B
// Request the name's length of the first XBE to launch from the DVD drive
#define XE_DVD_XBE_LENGTH 0x20C
// Send the address where to put the name of the first XBE to launch from the DVD drive
#define XE_DVD_XBE_ADDR 0x20D

#define KERNEL_STACK_SIZE 12288
#define KERNEL_BASE 0x80010000

enum SystemType {
	SYSTEM_XBOX,
	SYSTEM_CHIHIRO,
	SYSTEM_DEVKIT
};

enum IoRequestType : ULONG {
	Open =   1 << 16,
	Create = 2 << 16,
	Remove = 3 << 16,
	Close =  4 << 16,
	Read =   5 << 16,
	Write =  6 << 16
};

enum IoDevice : ULONG {
	Dvd = 0 << 12,
	Hdd = 1 << 12
};

// Renamed to avoid a name conflict with the IoStatus member of IRP
enum IoStatus2 : ULONG {
	Success = 0,
	Pending,
	Error
};

#pragma pack(1)
struct IoRequest {
	ULONG Id; // unique id to identify this request
	IoRequestType Type; // type of request and flags
	LONGLONG Offset; // file offset from which to start the I/O
	ULONG Size; // bytes to transfer or size of path for open/create requests
	ULONG HandleOrAddress; // virtual address of the data to transfer or file handle for open/create requests
	ULONG HandleOrPath; // file handle or file path for open/create requests
};
#pragma pack()

struct IoInfoBlock {
	IoStatus2 Status;
	ULONG Info;
};

struct XBOX_HARDWARE_INFO {
	ULONG Flags;
	UCHAR GpuRevision;
	UCHAR McpRevision;
	UCHAR Unknown3;
	UCHAR Unknown4;
};

inline SystemType XboxType;
inline LONG IoRequestId = -1;

#ifdef __cplusplus
extern "C" {
#endif

EXPORTNUM(322) DLLEXPORT extern XBOX_HARDWARE_INFO XboxHardwareInfo;

#ifdef __cplusplus
}
#endif

VOID FASTCALL OutputToHost(ULONG Value, USHORT Port);
ULONG FASTCALL InputFromHost(USHORT Port);
VOID FASTCALL SubmitIoRequestToHost(IoRequest *Request);
VOID FASTCALL RetrieveIoRequestFromHost(IoInfoBlock *Info, LONG Id);

VOID InitializeListHead(PLIST_ENTRY pListHead);
BOOLEAN IsListEmpty(PLIST_ENTRY pListHead);
VOID InsertTailList(PLIST_ENTRY pListHead, PLIST_ENTRY pEntry);
VOID InsertHeadList(PLIST_ENTRY pListHead, PLIST_ENTRY pEntry);
VOID RemoveEntryList(PLIST_ENTRY pEntry);
PLIST_ENTRY RemoveTailList(PLIST_ENTRY pListHead);
PLIST_ENTRY RemoveHeadList(PLIST_ENTRY pListHead);
