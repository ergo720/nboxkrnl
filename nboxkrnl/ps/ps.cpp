/*
 * ergo720                Copyright (c) 2023
 */

#include "ps.hpp"
#include "psp.hpp"
#include "..\kernel.hpp"
#include "..\rtl\rtl.hpp"
#include "..\mm\mm.hpp"
#include "..\nt\nt.hpp"
#include "..\xe\xe.hpp"
#include <string.h>


EXPORTNUM(259) OBJECT_TYPE PsThreadObjectType =
{
	ExAllocatePoolWithTag,
	ExFreePool,
	nullptr,
	nullptr,
	nullptr,
	(PVOID)offsetof(KTHREAD, Header),
	'erhT'
};

BOOLEAN PsInitSystem()
{
	HANDLE Handle;
	NTSTATUS Status = PsCreateSystemThread(&Handle, nullptr, XbeStartupThread, nullptr, FALSE);
	
	if (!NT_SUCCESS(Status)) {
		return FALSE;
	}

	NtClose(Handle);

	return TRUE;
}

EXPORTNUM(254) NTSTATUS XBOXAPI PsCreateSystemThread
(
	PHANDLE ThreadHandle,
	PHANDLE ThreadId,
	PKSTART_ROUTINE StartRoutine,
	PVOID StartContext,
	BOOLEAN DebuggerThread
)
{
	return PsCreateSystemThreadEx(ThreadHandle, 0, KERNEL_STACK_SIZE, 0, ThreadId, StartRoutine, StartContext, FALSE, DebuggerThread, PspSystemThreadStartup);
}

EXPORTNUM(255) NTSTATUS XBOXAPI PsCreateSystemThreadEx
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
)
{
	PETHREAD eThread;
	NTSTATUS Status = ObCreateObject(&PsThreadObjectType, nullptr, sizeof(ETHREAD) + ThreadExtensionSize, (PVOID *)&eThread);

	if (!NT_SUCCESS(Status)) {
		return Status;
	}

	memset(eThread, 0, sizeof(ETHREAD) + ThreadExtensionSize);

	InitializeListHead(&eThread->ReaperLink);
	InitializeListHead(&eThread->IrpList);

	KernelStackSize = ROUND_UP_4K(KernelStackSize);
	if (KernelStackSize < KERNEL_STACK_SIZE) {
		KernelStackSize = KERNEL_STACK_SIZE;
	}

	PVOID KernelStack = MmCreateKernelStack(KernelStackSize, DebuggerThread);
	if (KernelStack == nullptr) {
		ObfDereferenceObject(eThread);
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	eThread->StartAddress = StartRoutine;
	KeInitializeThread(&eThread->Tcb, KernelStack, KernelStackSize, TlsDataSize, SystemRoutine, StartRoutine, StartContext, &KiUniqueProcess);

	if (CreateSuspended) {
		KeSuspendThread(&eThread->Tcb);
	}

	// After this point, failures will be dealt with by PsTerminateSystemThread

	// Increment the ref count of the thread once more, so that we can safely use eThread if this thread terminates before PsCreateSystemThreadEx has finished
	ObfReferenceObject(eThread);

	// Increment the ref count of the thread once more. This is to guard against the case the XBE closes the thread handle manually
	// before this thread terminates with PsTerminateSystemThread
	ObfReferenceObject(eThread);

	Status = ObInsertObject(eThread, nullptr, 0, &eThread->UniqueThread);

	if (NT_SUCCESS(Status)) {
		PspCallThreadNotificationRoutines(eThread, TRUE);
		Status = ObOpenObjectByPointer(eThread, &PsThreadObjectType, ThreadHandle);
	}

	if (!NT_SUCCESS(Status)) {
		eThread->Tcb.HasTerminated = TRUE;
		if (CreateSuspended) {
			KeResumeThread(&eThread->Tcb);
		}
	}
	else {
		if (ThreadId) {
			*ThreadId = eThread->UniqueThread;
		}
	}

	KeQuerySystemTime(&eThread->CreateTime);
	// TODO: attempt to schedule the thread

	ObfDereferenceObject(eThread);

	return Status;
}

EXPORTNUM(258) DLLEXPORT VOID XBOXAPI PsTerminateSystemThread
(
	NTSTATUS ExitStatus
)
{
	RIP_UNIMPLEMENTED();
}
