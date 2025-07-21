/*
* ergo720                Copyright (c) 2025
*/

#pragma once

#include "..\types.hpp"


#ifdef __cplusplus
extern "C" {
#endif

EXPORTNUM(2) DLLEXPORT VOID XBOXAPI AvSendTVEncoderOption
(
	PVOID RegisterBase,
	ULONG Option,
	ULONG Param,
	ULONG *Result
);

#ifdef __cplusplus
}
#endif
