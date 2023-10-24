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
// Type of I/O request to submit (open, close, read or write)
#define IO_REQUEST_TYPE 0x206
// (Temporary): request to load the XBE. When the kernel I/O functions are added, this should be the file's path or the file's handle
#define XE_LOAD_XBE 0x207
// File offset to read from / write to
#define IO_OFFSET 0x208
// Size of the operation
#define IO_SIZE 0x209
// Guest address where to write the file to read or the address of block to write to the file
#define IO_ADDR 0x20A
// Request the Status member of IoInfoBlock
#define IO_QUERY_STATUS 0x20B
// Request the Info member of IoInfoBlock
#define IO_QUERY_INFO 0x20C

#define KERNEL_STACK_SIZE 12288
#define KERNEL_BASE 0x80010000

enum SystemType {
	SYSTEM_XBOX,
	SYSTEM_CHIHIRO,
	SYSTEM_DEVKIT
};

enum IoRequestType {
	Open = 0,
	Close,
	Read,
	Write
};

enum IoStatus {
	NoIoPending = -3,
	NotFound,
	Error,
	Success
};

struct IoRequest {
	ULONG Type;
	HANDLE Handle;
	ULONG Offset;
	ULONG Size;
	PVOID Address;
};

struct IoInfoBlock {
	IoStatus Status;
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
VOID FASTCALL RetrieveIoRequestFromHost(IoInfoBlock *Info);

VOID InitializeListHead(PLIST_ENTRY pListHead);
VOID InsertTailList(PLIST_ENTRY pListHead, PLIST_ENTRY pEntry);
VOID InsertHeadList(PLIST_ENTRY pListHead, PLIST_ENTRY pEntry);
