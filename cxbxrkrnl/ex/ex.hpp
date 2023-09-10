/*
 * ergo720                Copyright (c) 2022
 */

#pragma once

#include "..\nt\zw.hpp"


#ifdef __cplusplus
extern "C" {
#endif

EXPORTNUM(26) DLLEXPORT VOID XBOXAPI ExRaiseException
(
	PEXCEPTION_RECORD ExceptionRecord
);

EXPORTNUM(27) DLLEXPORT VOID XBOXAPI ExRaiseStatus
(
	NTSTATUS Status
);

EXPORTNUM(51) DLLEXPORT LONG FASTCALL InterlockedCompareExchange
(
	volatile PLONG Destination,
	LONG  Exchange,
	LONG  Comparand
);

EXPORTNUM(54) DLLEXPORT LONG FASTCALL InterlockedExchange
(
	volatile PLONG Destination,
	LONG Value
);

#ifdef __cplusplus
}
#endif
