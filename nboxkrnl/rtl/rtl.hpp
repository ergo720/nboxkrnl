/*
 * ergo720                Copyright (c) 2023
 */

#pragma once

#include "..\types.hpp"
#include "seh.hpp"
#include "io.hpp"


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

EXPORTNUM(260) DLLEXPORT NTSTATUS XBOXAPI RtlAnsiStringToUnicodeString
(
	PUNICODE_STRING DestinationString,
	PSTRING SourceString,
	UCHAR AllocateDestinationString
);

EXPORTNUM(261) DLLEXPORT NTSTATUS XBOXAPI RtlAppendStringToString
(
	PSTRING Destination,
	PSTRING Source
);

EXPORTNUM(262) DLLEXPORT NTSTATUS XBOXAPI RtlAppendUnicodeStringToString
(
	PUNICODE_STRING Destination,
	PUNICODE_STRING Source
);

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

EXPORTNUM(266) DLLEXPORT USHORT XBOXAPI RtlCaptureStackBackTrace
(
	ULONG FramesToSkip,
	ULONG FramesToCapture,
	PVOID *BackTrace,
	PULONG BackTraceHash
);

EXPORTNUM(267) DLLEXPORT NTSTATUS XBOXAPI RtlCharToInteger
(
	PCSZ String,
	ULONG Base,
	PULONG Value
);

EXPORTNUM(268) DLLEXPORT SIZE_T XBOXAPI RtlCompareMemory
(
	PVOID Source1,
	PVOID Source2,
	SIZE_T Length
);

EXPORTNUM(269) DLLEXPORT SIZE_T XBOXAPI RtlCompareMemoryUlong
(
	PVOID Source,
	SIZE_T Length,
	ULONG Pattern
);

EXPORTNUM(270) DLLEXPORT LONG XBOXAPI RtlCompareString
(
	PSTRING String1,
	PSTRING String2,
	BOOLEAN CaseInSensitive
);

EXPORTNUM(272) DLLEXPORT VOID XBOXAPI RtlCopyString
(
	PSTRING DestinationString,
	PSTRING SourceString
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

EXPORTNUM(281) DLLEXPORT LARGE_INTEGER XBOXAPI RtlExtendedIntegerMultiply
(
	LARGE_INTEGER Multiplicand,
	LONG Multiplier
);

EXPORTNUM(282) DLLEXPORT LARGE_INTEGER XBOXAPI RtlExtendedLargeIntegerDivide
(
	LARGE_INTEGER Dividend,
	ULONG Divisor,
	PULONG Remainder
);

EXPORTNUM(283) DLLEXPORT LARGE_INTEGER XBOXAPI RtlExtendedMagicDivide
(
	LARGE_INTEGER Dividend,
	LARGE_INTEGER MagicDivisor,
	CCHAR ShiftCount
);

EXPORTNUM(284) DLLEXPORT VOID XBOXAPI RtlFillMemory
(
	VOID *Destination,
	DWORD Length,
	BYTE  Fill
);

EXPORTNUM(285) DLLEXPORT VOID XBOXAPI RtlFillMemoryUlong
(
	PVOID Destination,
	SIZE_T Length,
	ULONG Pattern
);

EXPORTNUM(287) DLLEXPORT VOID XBOXAPI RtlFreeUnicodeString
(
	PUNICODE_STRING UnicodeString
);

EXPORTNUM(289) DLLEXPORT VOID XBOXAPI RtlInitAnsiString
(
	PANSI_STRING DestinationString,
	PCSZ SourceString
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

EXPORTNUM(296) DLLEXPORT CHAR XBOXAPI RtlLowerChar
(
	CHAR Character
);

EXPORTNUM(297) DLLEXPORT VOID XBOXAPI RtlMapGenericMask
(
	PACCESS_MASK AccessMask,
	PGENERIC_MAPPING GenericMapping
);

EXPORTNUM(298) DLLEXPORT VOID XBOXAPI RtlMoveMemory
(
	VOID *Destination,
	VOID *Source,
	SIZE_T Length
);

EXPORTNUM(299) DLLEXPORT NTSTATUS XBOXAPI RtlMultiByteToUnicodeN
(
	PWSTR UnicodeString,
	ULONG MaxBytesInUnicodeString,
	PULONG BytesInUnicodeString,
	PCHAR MultiByteString,
	ULONG BytesInMultiByteString
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

EXPORTNUM(304) DLLEXPORT BOOLEAN XBOXAPI RtlTimeFieldsToTime
(
	PTIME_FIELDS TimeFields,
	PLARGE_INTEGER Time
);

EXPORTNUM(305) DLLEXPORT VOID XBOXAPI RtlTimeToTimeFields
(
	PLARGE_INTEGER Time,
	PTIME_FIELDS TimeFields
);

EXPORTNUM(307) DLLEXPORT ULONG FASTCALL RtlUlongByteSwap
(
	ULONG Source
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

EXPORTNUM(317) DLLEXPORT VOID XBOXAPI RtlUpperString
(
	PSTRING DestinationString,
	PSTRING SourceString
);

EXPORTNUM(318) DLLEXPORT USHORT FASTCALL RtlUshortByteSwap
(
	USHORT Source
);

EXPORTNUM(319) DLLEXPORT ULONG XBOXAPI RtlWalkFrameChain
(
	PVOID *Callers,
	ULONG Count,
	ULONG Flags
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

[[noreturn]] VOID CDECL RipWithMsg(const char *Func, const char *Msg, ...);

#define RIP_UNIMPLEMENTED() RtlRip(const_cast<PCHAR>(__func__), nullptr, const_cast<PCHAR>("unimplemented!"))
#define RIP_API_MSG(Msg) RtlRip(const_cast<PCHAR>(__func__), nullptr, const_cast<PCHAR>(Msg))
#define RIP_API_FMT(Msg, ...) RipWithMsg(__func__, Msg __VA_OPT__(,) __VA_ARGS__)
