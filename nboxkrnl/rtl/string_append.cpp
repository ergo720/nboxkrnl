/*
* SPDX-License-Identifier: GPL-2.0+
*
* PatrickvL              Copyright (c) 2016
*
* This program is free software; you can redistribute it
* and/or modify it under the terms of the GNU General Public
* License as published by the Free Software Foundation; either
* version 2 of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty
* of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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
