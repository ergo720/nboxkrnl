/*
* SPDX-License-Identifier: GPL-2.0+
*
* PatrickvL              Copyright (c) 2018
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
EXPORTNUM(272) VOID XBOXAPI RtlCopyString
(
	PSTRING DestinationString,
	PSTRING SourceString
)
{
	if (SourceString == nullptr) {
		DestinationString->Length = 0;
		return;
	}

	CHAR *pd = DestinationString->Buffer;
	CHAR *ps = SourceString->Buffer;
	USHORT len = SourceString->Length;
	if ((USHORT)len > DestinationString->MaximumLength) {
		len = DestinationString->MaximumLength;
	}

	DestinationString->Length = (USHORT)len;
	memcpy(pd, ps, len);
}

// Source: Cxbx-Reloaded
EXPORTNUM(289) VOID XBOXAPI RtlInitAnsiString
(
	PANSI_STRING DestinationString,
	PCSZ SourceString
)
{
	DestinationString->Buffer = const_cast<PCHAR>(SourceString);
	if (SourceString) {
		DestinationString->Length = (USHORT)strlen(DestinationString->Buffer);
		DestinationString->MaximumLength = DestinationString->Length + 1;
	}
	else {
		DestinationString->Length = DestinationString->MaximumLength = 0;
	}
}
