/*
 * ergo720                Copyright (c) 2022
 */

#pragma once

#include "..\types.hpp"

#ifdef __cplusplus
extern "C" {
#endif

EXPORTNUM(8) DLLEXPORT ULONG CDECL DbgPrint
(
	const CHAR *Format,
	...
);

#ifdef __cplusplus
}
#endif
