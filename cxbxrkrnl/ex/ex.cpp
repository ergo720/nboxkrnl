/*
 * ergo720                Copyright (c) 2022
 */

#include "ex.h"


EXPORTNUM(51) LONG FASTCALL InterlockedCompareExchange
(
	volatile PLONG Destination,
	LONG  Exchange,
	LONG  Comparand
)
{
	__asm {
		mov eax, Comparand
		cmpxchg [ecx], edx
	}
}

EXPORTNUM(54) LONG FASTCALL InterlockedExchange
(
	volatile PLONG Destination,
	LONG Value
)
{
	__asm {
		mov eax, Value
		xchg [ecx], eax
	}
}
