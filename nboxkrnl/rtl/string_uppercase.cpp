/*
* PatrickvL              Copyright (c) 2016
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
