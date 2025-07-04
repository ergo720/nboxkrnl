/*
* PatrickvL              Copyright (c) 2016
*/

#include "rtl.hpp"
#include <string.h>


// Source: Cxbx-Reloaded
EXPORTNUM(261) NTSTATUS XBOXAPI RtlAppendStringToString
(
	PSTRING Destination,
	PSTRING Source
)
{
	NTSTATUS Result = STATUS_SUCCESS;

	USHORT dstLen = Destination->Length;
	USHORT srcLen = Source->Length;
	if (srcLen > 0) {
		if ((srcLen + dstLen) > Destination->MaximumLength) {
			Result = STATUS_BUFFER_TOO_SMALL;
		}
		else {
			CHAR *dstBuf = Destination->Buffer + Destination->Length;
			CHAR *srcBuf = Source->Buffer;
			memmove(dstBuf, srcBuf, srcLen);
			Destination->Length += srcLen;
		}
	}

	return Result;
}

// Source: Cxbx-Reloaded
EXPORTNUM(262) NTSTATUS XBOXAPI RtlAppendUnicodeStringToString
(
	PUNICODE_STRING Destination,
	PUNICODE_STRING Source
)
{
	NTSTATUS Result = STATUS_SUCCESS;

	USHORT dstLen = Destination->Length;
	USHORT srcLen = Source->Length;
	if (srcLen > 0) {
		if ((srcLen + dstLen) > Destination->MaximumLength) {
			Result = STATUS_BUFFER_TOO_SMALL;
		}
		else {
			WCHAR *dstBuf = (WCHAR*)(Destination->Buffer + (Destination->Length / sizeof(WCHAR)));
			memmove(dstBuf, Source->Buffer, srcLen);
			Destination->Length += srcLen;
			if (Destination->Length < Destination->MaximumLength) {
				dstBuf[srcLen / sizeof(WCHAR)] = UNICODE_NULL;
			}
		}
	}

	return Result;
}
