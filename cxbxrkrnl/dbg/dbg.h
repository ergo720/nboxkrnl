/*
 * ergo720                Copyright (c) 2022
 */

#pragma once

#include "..\types.h"

#ifdef __cplusplus
extern "C" {
#endif

EXPORTNUM(8) ULONG CDECL DbgPrint
(
	const CHAR *Format,
	...
);

#ifdef __cplusplus
}
#endif
