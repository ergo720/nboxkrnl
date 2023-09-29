/*
 * ergo720                Copyright (c) 2022
 */

#include "ex.hpp"
#include "..\ke\ke.hpp"


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

EXPORTNUM(52) LONG FASTCALL InterlockedDecrement
(
	volatile PLONG Addend
)
{
	__asm {
		or eax, 0xFFFFFFFF
		xadd [ecx], eax
		dec eax
	}
}

EXPORTNUM(53) LONG FASTCALL InterlockedIncrement
(
	volatile PLONG Addend
)
{
	__asm {
		mov eax, 1
		xadd [ecx], eax
		inc eax
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

EXPORTNUM(321) XBOX_KEY_DATA XboxEEPROMKey;
