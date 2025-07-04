/*
* PatrickvL              Copyright (c) 2016
*/

#include "rtl.hpp"


// Source: Cxbx-Reloaded
EXPORTNUM(296) CHAR XBOXAPI RtlLowerChar
(
	CHAR Character
)
{
	BYTE CharCode = (BYTE)Character;

	if (CharCode >= 'A' && CharCode <= 'Z') {
		CharCode ^= 0x20;
	}
	// Latin alphabet (ISO 8859-1)
	else if (CharCode >= 0xc0 && CharCode <= 0xde && CharCode != 0xd7) {
		CharCode ^= 0x20;
	}

	return (CHAR)CharCode;
}
