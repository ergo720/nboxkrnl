/*
 * ergo720                Copyright (c) 2023
 */

#pragma once

#include "..\types.hpp"


#ifdef __cplusplus
extern "C" {
#endif

EXPORTNUM(187) DLLEXPORT NTSTATUS XBOXAPI NtClose
(
	HANDLE Handle
);

#ifdef __cplusplus
}
#endif
