/*
 * ergo720                Copyright (c) 2022
 */

#include "ex.hpp"
#include "..\rtl\rtl.hpp"
#include "..\rtl\exp_sup.hpp"


EXPORTNUM(26) __declspec(naked) VOID XBOXAPI ExRaiseException
(
	PEXCEPTION_RECORD ExceptionRecord
)
{
	__asm {
		push ebp
		mov ebp, esp
		sub esp, SIZE CONTEXT
		push esp
		call RtlCaptureContext
		add [esp]CONTEXT.Esp, 4 // pop ExceptionRecord argument
		mov [esp]CONTEXT.ContextFlags, CONTEXT_CONTROL | CONTEXT_INTEGER | CONTEXT_SEGMENTS // set ContextFlags member of CONTEXT
		mov eax, [ebp + 8]
		mov ecx, [ebp + 4]
		mov [eax]EXCEPTION_RECORD.ExceptionAddress, ecx // set ExceptionAddress member of ExceptionRecord argument to caller's eip
		push esp
		push eax
		call RtlDispatchException
		// If the exception is continuable, then RtlDispatchException will return
		mov ecx, esp
		push FALSE
		push ecx
		test al, al
		jz exp_unhandled
		call ZwContinue // won't return
		exp_unhandled:
		push [ebp + 8]
		call ZwRaiseException // won't return
	}
}

EXPORTNUM(27) __declspec(naked) VOID XBOXAPI ExRaiseStatus
(
	NTSTATUS Status
)
{
	__asm {
		push ebp
		mov ebp, esp
		sub esp, SIZE CONTEXT + SIZE EXCEPTION_RECORD
		push esp
		call RtlCaptureContext
		add [esp]CONTEXT.Esp, 4 // pop Status argument
		mov [esp]CONTEXT.ContextFlags, CONTEXT_CONTROL | CONTEXT_INTEGER | CONTEXT_SEGMENTS // set ContextFlags member of CONTEXT
		lea ecx, [ebp - SIZE EXCEPTION_RECORD]
		mov eax, [ebp + 8]
		mov [ecx]EXCEPTION_RECORD.ExceptionCode, eax // set ExceptionCode member of ExceptionRecord to Status argument
		mov [ecx]EXCEPTION_RECORD.ExceptionFlags, EXCEPTION_NONCONTINUABLE
		mov [ecx]EXCEPTION_RECORD.ExceptionRecord, 0
		mov eax, [ebp + 4]
		mov [ecx]EXCEPTION_RECORD.ExceptionAddress, eax // set ExceptionAddress member of ExceptionRecord to caller's eip
		mov [ecx]EXCEPTION_RECORD.NumberParameters, 0
		push esp
		push ecx
		call RtlDispatchException
		// Because the exception is non-continuable, RtlDispatchException should never return. If it does return, then it must be a bug
	noreturn_eip:
		push 0
		push 0
		push 0
		push noreturn_eip
		push NORETURN_FUNCTION_RETURNED
		call KeBugCheckEx // won't return
	}
}
