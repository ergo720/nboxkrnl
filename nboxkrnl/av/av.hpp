/*
* ergo720                Copyright (c) 2025
*/

#pragma once

#include "..\types.hpp"


#ifdef __cplusplus
extern "C" {
#endif

EXPORTNUM(1) DLLEXPORT PVOID XBOXAPI AvGetSavedDataAddress();

EXPORTNUM(2) DLLEXPORT VOID XBOXAPI AvSendTVEncoderOption
(
	PVOID RegisterBase,
	ULONG Option,
	ULONG Param,
	ULONG *Result
);

EXPORTNUM(4) DLLEXPORT VOID XBOXAPI AvSetSavedDataAddress
(
	PVOID Address
);

#ifdef __cplusplus
}
#endif
