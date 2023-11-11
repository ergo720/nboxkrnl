/*
 * PatrickvL              Copyright (c) 2018
 * x1nixmzeng             Copyright (c) 2018
 */

#include "rtl.hpp"


// Source: Cxbx-Reloaded
EXPORTNUM(279) BOOLEAN XBOXAPI RtlEqualString
(
	PSTRING String1,
	PSTRING String2,
	BOOLEAN CaseInSensitive
)
{
	BOOLEAN bRet = TRUE;

	USHORT l1 = String1->Length;
	USHORT l2 = String2->Length;
	if (l1 != l2) {
		return FALSE;
	}

	CHAR *p1 = String1->Buffer;
	CHAR *p2 = String2->Buffer;
	CHAR *last = p1 + l1;

	if (CaseInSensitive) {
		while (p1 < last) {
			CHAR c1 = *p1++;
			CHAR c2 = *p2++;
			if (c1 != c2) {
				c1 = RtlUpperChar(c1);
				c2 = RtlUpperChar(c2);
				if (c1 != c2) {
					return FALSE;
				}
			}
		}

		return TRUE;
	}

	while (p1 < last) {
		if (*p1++ != *p2++) {
			bRet = FALSE;
			break;
		}
	}

	return bRet;
}

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
