/*
 * ergo720                Copyright (c) 2022
 */

#pragma once

#include "..\nt\zw.hpp"


#ifdef __cplusplus
extern "C" {
#endif

EXPORTNUM(14) DLLEXPORT PVOID XBOXAPI ExAllocatePool
(
	SIZE_T NumberOfBytes
);

EXPORTNUM(15) DLLEXPORT PVOID XBOXAPI ExAllocatePoolWithTag
(
	SIZE_T NumberOfBytes,
	ULONG Tag
);

EXPORTNUM(17) DLLEXPORT VOID XBOXAPI ExFreePool
(
	PVOID P
);

EXPORTNUM(23) DLLEXPORT ULONG XBOXAPI ExQueryPoolBlockSize
(
	PVOID PoolBlock
);

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
