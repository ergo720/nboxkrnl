/*
 * ergo720                Copyright (c) 2023
 */

#pragma once

#include "..\types.hpp"
#include "..\ki\seh.hpp"


#ifdef __cplusplus
extern "C" {
#endif

EXPORTNUM(265) DLLEXPORT VOID XBOXAPI RtlCaptureContext
(
	PCONTEXT ContextRecord
);

EXPORTNUM(302) DLLEXPORT VOID XBOXAPI RtlRaiseException
(
	PEXCEPTION_RECORD ExceptionRecord
);

EXPORTNUM(312) DLLEXPORT VOID XBOXAPI RtlUnwind
(
	PVOID TargetFrame,
	PVOID TargetIp,
	PEXCEPTION_RECORD ExceptionRecord,
	PVOID ReturnValue
);

#ifdef __cplusplus
}
#endif
