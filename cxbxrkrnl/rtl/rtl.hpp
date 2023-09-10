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

EXPORTNUM(303) DLLEXPORT VOID XBOXAPI RtlRaiseStatus
(
	NTSTATUS Status
);

EXPORTNUM(312) DLLEXPORT VOID XBOXAPI RtlUnwind
(
	PVOID TargetFrame,
	PVOID TargetIp,
	PEXCEPTION_RECORD ExceptionRecord,
	PVOID ReturnValue
);

[[noreturn]] EXPORTNUM(352) DLLEXPORT VOID XBOXAPI RtlRip
(
	PVOID ApiName,
	PVOID Expression,
	PVOID Message
);

#ifdef __cplusplus
}
#endif

#define RIP_UNIMPLEMENTED() RtlRip(const_cast<PCHAR>(__func__), nullptr, const_cast<PCHAR>("unimplemented!"))
#define RIP_API_MSG(Msg) RtlRip(const_cast<PCHAR>(__func__), nullptr, const_cast<PCHAR>(Msg))
