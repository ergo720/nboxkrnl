/*
 * ergo720                Copyright (c) 2023
 */

#pragma once

#include "..\types.hpp"
#include "..\ke\ke.hpp"
#include "..\ob\ob.hpp"
#include "..\ex\ex.hpp"


#ifdef __cplusplus
extern "C" {
#endif

EXPORTNUM(254) DLLEXPORT NTSTATUS XBOXAPI PsCreateSystemThread
(
	PHANDLE ThreadHandle,
	PHANDLE ThreadId,
	PKSTART_ROUTINE StartRoutine,
	PVOID StartContext,
	BOOLEAN DebuggerThread
);

EXPORTNUM(255) DLLEXPORT NTSTATUS XBOXAPI PsCreateSystemThreadEx
(
	PHANDLE ThreadHandle,
	SIZE_T ThreadExtensionSize,
	SIZE_T KernelStackSize,
	SIZE_T TlsDataSize,
	PHANDLE ThreadId,
	PKSTART_ROUTINE StartRoutine,
	PVOID StartContext,
	BOOLEAN CreateSuspended,
	BOOLEAN DebuggerThread,
	PKSYSTEM_ROUTINE SystemRoutine
);

EXPORTNUM(258) DLLEXPORT VOID XBOXAPI PsTerminateSystemThread
(
	NTSTATUS ExitStatus
);

EXPORTNUM(259) DLLEXPORT extern OBJECT_TYPE PsThreadObjectType;

#ifdef __cplusplus
}
#endif

BOOLEAN PsInitSystem();
