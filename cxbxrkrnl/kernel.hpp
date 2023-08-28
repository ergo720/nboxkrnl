/*
 * ergo720                Copyright (c) 2022
 * cxbxr devs
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

#define KERNEL_STACK_SIZE 12288
#define KERNEL_BASE 0x80010000

enum SystemType {
	SYSTEM_XBOX,
	SYSTEM_CHIHIRO,
	SYSTEM_DEVKIT
};


VOID FASTCALL OutputToHost(ULONG Value, USHORT Port);
ULONG FASTCALL InputFromHost(USHORT Port);

VOID InitializeListHead(PLIST_ENTRY pListHead);
VOID InsertTailList(PLIST_ENTRY pListHead, PLIST_ENTRY pEntry);
VOID InsertHeadList(PLIST_ENTRY pListHead, PLIST_ENTRY pEntry);