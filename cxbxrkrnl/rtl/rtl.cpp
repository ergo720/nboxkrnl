/*
 * ergo720                Copyright (c) 2023
 */

#include "rtl.hpp"


EXPORTNUM(265) __declspec(naked) VOID XBOXAPI RtlCaptureContext
(
	PCONTEXT ContextRecord
)
{
	// NOTE: this function expects the caller to be __cdecl, or else it fails
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

		mov [ebx]CONTEXT.Ebp, ebp // ContextRecord->Ebp = ebp;
		mov eax, [ebp + 4]        // eax = return address;
		mov [ebx]CONTEXT.Eip, eax // ContextRecord->Eip = return address;
		lea eax, [ebp + 8]
		mov [ebx]CONTEXT.Esp, eax // ContextRecord->Esp = original esp value;

		pop ebx
		ret 4
	}
}
