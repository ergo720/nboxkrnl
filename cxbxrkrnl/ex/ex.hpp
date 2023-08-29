/*
 * ergo720                Copyright (c) 2022
 */

#pragma once

#include "..\types.hpp"


#ifdef __cplusplus
extern "C" {
#endif

EXPORTNUM(51)
DLLEXPORT LONG FASTCALL InterlockedCompareExchange(
	volatile PLONG Destination,
	LONG           Exchange,
	LONG           Comparand);

EXPORTNUM(54)
DLLEXPORT LONG FASTCALL InterlockedExchange(
	volatile PLONG Destination,
	LONG           Value);

#ifdef __cplusplus
}
#endif
