/*
 * PatrickvL              Copyright (c) 2018
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
