/*
 * ergo720                Copyright (c) 2023
 * Fisherman166           Copyright (c) 2018
 */

#include "rtl.hpp"
#include "..\hal\halp.hpp"
#include "..\dbg\dbg.hpp"
#include "..\ex\ex.hpp"
#include <assert.h>


VOID RtlpInitializeCriticalSection(PRTL_CRITICAL_SECTION CriticalSection)
{
	RtlInitializeCriticalSection(CriticalSection);
	CriticalSection->Event.Absolute = FALSE;
	CriticalSection->Event.Inserted = FALSE;
}

EXPORTNUM(264) VOID XBOXAPI RtlAssert
(
	PVOID FailedAssertion,
	PVOID FileName,
	ULONG LineNumber,
	PCHAR Message
)
{
	// This routine is called from pdclib when an assertion fails in debug builds only

	DbgPrint("\n\n!!! Assertion failed: %s%s !!!\nSource File: %s, line %ld\n\n", Message ? Message : "", FailedAssertion, FileName, LineNumber);

	// NOTE: this should check for the presence of a guest debugger on the host side (such as lib86dbg) and, if present, communicate that it should hook
	// the breakpoint exception handler that int3 generates, so that it can take over us and start a debugging session

	__asm int 3
}

EXPORTNUM(265) __declspec(naked) VOID XBOXAPI RtlCaptureContext
(
	PCONTEXT ContextRecord
)
{
	// NOTE: this function sets esp and ebp to the values they had in the caller's caller. For example, when called from RtlUnwind, it will set them to
	// the values used in _global_unwind2. To achieve this effect, the caller must use __declspec(noinline) and #pragma optimize("y", off)
	__asm {
		push ebx
		mov ebx, [esp + 8]        // ebx = ContextRecord;

		mov [ebx]CONTEXT.Eax, eax // ContextRecord->Eax = eax;
		mov eax, [esp]            // eax = original value of ebx
		mov [ebx]CONTEXT.Ebx, eax // ContextRecord->Ebx = original value of ebx
		mov [ebx]CONTEXT.Ecx, ecx // ContextRecord->Ecx = ecx;
		mov [ebx]CONTEXT.Edx, edx // ContextRecord->Edx = edx;
		mov [ebx]CONTEXT.Esi, esi // ContextRecord->Esi = esi;
		mov [ebx]CONTEXT.Edi, edi // ContextRecord->Edi = edi;

		mov word ptr [ebx]CONTEXT.SegCs, cs // ContextRecord->SegCs = cs;
		mov word ptr [ebx]CONTEXT.SegSs, ss // ContextRecord->SegSs = ss;
		pushfd
		pop [ebx]CONTEXT.EFlags   // ContextRecord->EFlags = flags;

		mov eax, [ebp]            // eax = old ebp;
		mov [ebx]CONTEXT.Ebp, eax // ContextRecord->Ebp = ebp;
		mov eax, [ebp + 4]        // eax = return address;
		mov [ebx]CONTEXT.Eip, eax // ContextRecord->Eip = return address;
		lea eax, [ebp + 8]
		mov [ebx]CONTEXT.Esp, eax // ContextRecord->Esp = original esp value;

		pop ebx
		ret 4
	}
}

EXPORTNUM(277) VOID XBOXAPI RtlEnterCriticalSection
(
	PRTL_CRITICAL_SECTION CriticalSection
)
{
	// This function must update the members of CriticalSection atomically, so we use assembly

	__asm {
		mov ecx, CriticalSection
		mov eax, [KiPcr]KPCR.PrcbData.CurrentThread
		inc [ecx]RTL_CRITICAL_SECTION.LockCount
		jnz already_owned
		mov [ecx]RTL_CRITICAL_SECTION.OwningThread, eax
		mov [ecx]RTL_CRITICAL_SECTION.RecursionCount, 1
		jmp end_func
	already_owned:
		cmp [ecx]RTL_CRITICAL_SECTION.OwningThread, eax
		jz owned_by_self
		push eax
		push 0
		push FALSE
		push KernelMode
		push WrExecutive
		push ecx
		call KeWaitForSingleObject
		pop [ecx]RTL_CRITICAL_SECTION.OwningThread
		mov [ecx]RTL_CRITICAL_SECTION.RecursionCount, 1
		jmp end_func
	owned_by_self:
		inc [ecx]RTL_CRITICAL_SECTION.RecursionCount
	end_func:
	}
}

EXPORTNUM(278) VOID XBOXAPI RtlEnterCriticalSectionAndRegion
(
	PRTL_CRITICAL_SECTION CriticalSection
)
{
	KeEnterCriticalRegion();
	RtlEnterCriticalSection(CriticalSection);
}

EXPORTNUM(285) VOID XBOXAPI RtlFillMemoryUlong
(
	PVOID Destination,
	SIZE_T Length,
	ULONG Pattern
)
{
	assert(Length);
	assert((Length % sizeof(ULONG)) == 0); // Length must be a multiple of ULONG
	assert(((ULONG_PTR)Destination % sizeof(ULONG)) == 0); // Destination must be 4-byte aligned

	unsigned NumOfRepeats = Length / sizeof(ULONG);
	PULONG d = (PULONG)Destination;

	for (unsigned i = 0; i < NumOfRepeats; ++i) {
		d[i] = Pattern; // copy an ULONG at a time
	}
}

EXPORTNUM(291) VOID XBOXAPI RtlInitializeCriticalSection
(
	PRTL_CRITICAL_SECTION CriticalSection
)
{
	KeInitializeEvent((PKEVENT)&CriticalSection->Event, SynchronizationEvent, FALSE);
	CriticalSection->LockCount = -1;
	CriticalSection->RecursionCount = 0;
	CriticalSection->OwningThread = 0;
}

EXPORTNUM(294) VOID XBOXAPI RtlLeaveCriticalSection
(
	PRTL_CRITICAL_SECTION CriticalSection
)
{
	// This function must update the members of CriticalSection atomically, so we use assembly

	__asm {
		mov ecx, CriticalSection
		dec [ecx]RTL_CRITICAL_SECTION.RecursionCount
		jnz dec_count
		dec [ecx]RTL_CRITICAL_SECTION.LockCount
		mov [ecx]RTL_CRITICAL_SECTION.OwningThread, 0
		jl end_func
		push FALSE
		push PRIORITY_BOOST_EVENT
		push ecx
		call KeSetEvent
		jmp end_func
	dec_count:
		dec [ecx]RTL_CRITICAL_SECTION.LockCount
	end_func:
	}
}

EXPORTNUM(295) VOID XBOXAPI RtlLeaveCriticalSectionAndRegion
(
	PRTL_CRITICAL_SECTION CriticalSection
)
{
	// NOTE: this must check RecursionCount only once, so that it can unconditionally call KeLeaveCriticalRegion if the counter is zero regardless of
	// its current value. This, to guard against the case where a thread switch happens after the counter is updated but before KeLeaveCriticalRegion is called

	__asm {
		mov ecx, CriticalSection
		dec [ecx]RTL_CRITICAL_SECTION.RecursionCount
		jnz dec_count
		dec [ecx]RTL_CRITICAL_SECTION.LockCount
		mov [ecx]RTL_CRITICAL_SECTION.OwningThread, 0
		jl not_signalled
		push FALSE
		push PRIORITY_BOOST_EVENT
		push ecx
		call KeSetEvent
	not_signalled:
		call KeLeaveCriticalRegion
		jmp end_func
	dec_count:
		dec [ecx]RTL_CRITICAL_SECTION.LockCount
	end_func:
	}
}

EXPORTNUM(352) VOID XBOXAPI RtlRip
(
	PVOID ApiName,
	PVOID Expression,
	PVOID Message
)
{
	// This routine terminates the system. It should be used when a failure is expected, for example when executing unimplemented stuff

	const char *OpenBracket = " (";
	const char *CloseBracket = ") ";

	if (!Message) {
		if (Expression) {
			Message = const_cast<PCHAR>("failed");
		}
		else {
			Message = const_cast<PCHAR>("execution failure");
		}
	}

	if (!Expression) {
		Expression = const_cast<PCHAR>("");
		OpenBracket = "";
		CloseBracket = " ";
	}

	if (!ApiName) {
		ApiName = const_cast<PCHAR>("RIP");
	}

	DbgPrint("%s:%s%s%s%s\n\n", ApiName, OpenBracket, Expression, CloseBracket, Message);
	HalpShutdownSystem();
}
