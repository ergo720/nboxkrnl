/*
 * ergo720                Copyright (c) 2022
 */

#pragma once

#include "..\types.h"


#ifdef __cplusplus
extern "C" {
#endif

EXPORTNUM(51) LONG FASTCALL InterlockedCompareExchange
(
	volatile PLONG Destination,
	LONG  Exchange,
	LONG  Comparand
);

EXPORTNUM(54) LONG FASTCALL InterlockedExchange
(
	volatile PLONG Destination,
	LONG Value
);

#ifdef __cplusplus
}
#endif
