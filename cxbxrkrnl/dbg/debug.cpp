/*
 * ergo720                Copyright (c) 2022
 */

#include "..\kernel.hpp"
#include "dbg.hpp"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>


EXPORTNUM(8)
ULONG CDECL DbgPrint(
	const CHAR* Format,
	...)
{
	if (Format != nullptr) {
		// We print at most 512 characters long strings
		char    buff[512];
		va_list vlist;
		va_start(vlist, Format);
		vsnprintf(buff, sizeof(buff), Format, vlist);
		va_end(vlist);

		OutputToHost(reinterpret_cast<ULONG>(buff), DBG_OUTPUT_STR_PORT);
	}

	return STATUS_SUCCESS;
}
