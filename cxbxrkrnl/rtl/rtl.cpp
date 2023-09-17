/*
 * ergo720                Copyright (c) 2023
 */

#include "rtl.hpp"
#include "..\hal\halp.hpp"
#include "..\dbg\dbg.hpp"
#include <assert.h>


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
