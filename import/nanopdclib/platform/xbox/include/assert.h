/* Diagnostics <assert.h>

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include "pdclib/_PDCLIB_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _PDCLIB_ASSERT_H
#define _PDCLIB_ASSERT_H
void _xbox_assert(char const *const expression, char const *const file_name, char const *const function_name, unsigned long line);
#endif

  /* If _DEBUG is not set, assert() is a null operation. */
#undef assert

#ifndef _DEBUG
#define assert(ignore) ((void)0)
#else
#define assert(expression) \
	((void)((expression) ? 0 : (_xbox_assert(_PDCLIB_symbol2string(expression), __FILE__, __func__, __LINE__), 0)))
#endif

#ifndef __cplusplus
#define static_assert _Static_assert
#endif

#ifdef __cplusplus
}
#endif
