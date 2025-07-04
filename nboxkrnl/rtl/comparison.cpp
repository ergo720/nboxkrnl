/*
* PatrickvL              Copyright (c) 2016
*/

#include "rtl.hpp"
//#include <string.h>


// Source: Cxbx-Reloaded
EXPORTNUM(268) SIZE_T XBOXAPI RtlCompareMemory
(
	PVOID Source1,
	PVOID Source2,
	SIZE_T Length
)
{
	SIZE_T Result = Length;

	PBYTE pBytes1 = (PBYTE)Source1;
	PBYTE pBytes2 = (PBYTE)Source2;
	for (DWORD i = 0; i < Length; i++) {
		if (pBytes1[i] != pBytes2[i]) {
			Result = i;
			break;
		}
	}

	return Result;
}

// Source: Cxbx-Reloaded
EXPORTNUM(269) SIZE_T XBOXAPI RtlCompareMemoryUlong
(
	PVOID Source,
	SIZE_T Length,
	ULONG Pattern
)
{
	PULONG ptr = (PULONG)Source;
	ULONG_PTR len = Length / sizeof(ULONG);

	for (ULONG_PTR i = 0; i < len; i++) {
		if (*ptr != Pattern) {
			break;
		}
		ptr++;
	}

	return (SIZE_T)((PCHAR)ptr - (PCHAR)Source);
}

// Source: Cxbx-Reloaded
EXPORTNUM(270) LONG XBOXAPI RtlCompareString
(
	PSTRING String1,
	PSTRING String2,
	BOOLEAN CaseInSensitive
)
{
	const USHORT l1 = String1->Length;
	const USHORT l2 = String2->Length;
	const USHORT maxLen = (l1 <= l2 ? l1 : l2);

	const PCHAR str1 = String1->Buffer;
	const PCHAR str2 = String2->Buffer;

	if (CaseInSensitive) {
		for (unsigned i = 0; i < maxLen; i++) {
			UCHAR char1 = RtlLowerChar(str1[i]);
			UCHAR char2 = RtlLowerChar(str2[i]);
			if (char1 != char2) {
				return char1 - char2;
			}
		}
	}
	else {
		for (unsigned i = 0; i < maxLen; i++) {
			if (str1[i] != str2[i]) {
				return str1[i] - str2[i];
			}
		}
	}

	return l1 - l2;
}

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
