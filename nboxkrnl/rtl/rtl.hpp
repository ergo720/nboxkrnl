/*
 * ergo720                Copyright (c) 2023
 */

#pragma once

#include "..\types.hpp"
#include "..\ki\seh.hpp"


struct RTL_CRITICAL_SECTION {
	DISPATCHER_HEADER Event;
	LONG LockCount;
	LONG RecursionCount;
	HANDLE OwningThread;
};
using PRTL_CRITICAL_SECTION = RTL_CRITICAL_SECTION *;

#ifdef __cplusplus
extern "C" {
#endif

EXPORTNUM(264) DLLEXPORT VOID XBOXAPI RtlAssert
(
	PVOID FailedAssertion,
	PVOID FileName,
	ULONG LineNumber,
	PCHAR Message
);

EXPORTNUM(265) DLLEXPORT VOID XBOXAPI RtlCaptureContext
(
	PCONTEXT ContextRecord
);

EXPORTNUM(277) DLLEXPORT VOID XBOXAPI RtlEnterCriticalSection
(
	PRTL_CRITICAL_SECTION CriticalSection
);

EXPORTNUM(278) DLLEXPORT VOID XBOXAPI RtlEnterCriticalSectionAndRegion
(
	PRTL_CRITICAL_SECTION CriticalSection
);

EXPORTNUM(285) DLLEXPORT VOID XBOXAPI RtlFillMemoryUlong
(
	PVOID Destination,
	SIZE_T Length,
	ULONG Pattern
);

EXPORTNUM(291) DLLEXPORT VOID XBOXAPI RtlInitializeCriticalSection
(
	PRTL_CRITICAL_SECTION CriticalSection
);

EXPORTNUM(294) DLLEXPORT VOID XBOXAPI RtlLeaveCriticalSection
(
	PRTL_CRITICAL_SECTION CriticalSection
);

EXPORTNUM(295) DLLEXPORT VOID XBOXAPI RtlLeaveCriticalSectionAndRegion
(
	PRTL_CRITICAL_SECTION CriticalSection
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

VOID RtlpInitializeCriticalSection(PRTL_CRITICAL_SECTION CriticalSection);
