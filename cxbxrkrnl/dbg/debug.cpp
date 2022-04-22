/*
 * ergo720                Copyright (c) 2022
 */

#include "dbg.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

// The list of i/o ports used on the xbox is at https://xboxdevwiki.net/Memory, so avoid using one of those

// The following four ports are used by the kernel to output debug strings to the host
#define DBG_OUTPUT_STR_PORT 0x0200


VOID FASTCALL DbgPrintStr(ULONG StrAddr, USHORT Port)
{
	__asm {
		mov eax, ecx
		out dx, eax
	}
}

EXPORTNUM(8) ULONG CDECL DbgPrint
(
	const CHAR *Format,
	...
)
{
	if (Format != nullptr) {
		// We print at most 512 characters long strings
		char buff[512];
		va_list vlist;
		va_start(vlist, Format);
		vsnprintf(buff, sizeof(buff), Format, vlist);
		va_end(vlist);

		DbgPrintStr(reinterpret_cast<ULONG>(buff), DBG_OUTPUT_STR_PORT);
	}

	return STATUS_SUCCESS;
}
