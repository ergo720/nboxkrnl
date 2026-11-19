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


// Source: Cxbx-Reloaded
EXPORTNUM(316) CHAR XBOXAPI RtlUpperChar
(
	CHAR Character
)
{
	BYTE CharCode = (BYTE)Character;

	if (CharCode >= 'a' && CharCode <= 'z') {
		CharCode ^= 0x20;
	}
	// Latin alphabet (ISO 8859-1)
	else if (CharCode >= 0xe0 && CharCode <= 0xfe && CharCode != 0xf7) {
		CharCode ^= 0x20;
	}
	else if (CharCode == 0xFF) {
		CharCode = '?';
	}

	return CharCode;
}

// Source: Cxbx-Reloaded
EXPORTNUM(317) VOID XBOXAPI RtlUpperString
(
	PSTRING DestinationString,
	PSTRING SourceString
)
{
	CHAR *pDst = DestinationString->Buffer;
	CHAR *pSrc = SourceString->Buffer;
	ULONG length = SourceString->Length;
	if ((USHORT)length > DestinationString->MaximumLength) {
		length = DestinationString->MaximumLength;
	}

	DestinationString->Length = (USHORT)length;
	while (length > 0) {
		*pDst++ = RtlUpperChar(*pSrc++);
		length--;
	}
}
