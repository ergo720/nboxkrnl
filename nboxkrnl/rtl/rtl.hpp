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

#define INITIALIZE_GLOBAL_CRITICAL_SECTION(CriticalSection)        \
    RTL_CRITICAL_SECTION CriticalSection = {                       \
        SynchronizationEvent,                                      \
        FALSE,                                                     \
        offsetof(RTL_CRITICAL_SECTION, LockCount) / sizeof(LONG),  \
        FALSE,                                                     \
        FALSE,                                                     \
        &CriticalSection.Event.WaitListHead,                       \
        &CriticalSection.Event.WaitListHead,                       \
        -1,                                                        \
        0,                                                         \
        NULL                                                       \
    }

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

EXPORTNUM(279) DLLEXPORT BOOLEAN XBOXAPI RtlEqualString
(
	PSTRING String1,
	PSTRING String2,
	BOOLEAN CaseInSensitive
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

EXPORTNUM(301) DLLEXPORT ULONG XBOXAPI RtlNtStatusToDosError
(
	NTSTATUS Status
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

EXPORTNUM(316) DLLEXPORT CHAR XBOXAPI RtlUpperChar
(
	CHAR Character
);

EXPORTNUM(320) DLLEXPORT VOID XBOXAPI RtlZeroMemory
(
	PVOID Destination,
	SIZE_T Length
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
