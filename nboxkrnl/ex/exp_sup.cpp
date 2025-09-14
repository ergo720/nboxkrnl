/*
 * ergo720                Copyright (c) 2022
 */

#include "ex.hpp"
#include "rtl.hpp"
#include "exp_sup.hpp"


EXPORTNUM(26) __declspec(naked) VOID XBOXAPI ExRaiseException
(
	PEXCEPTION_RECORD ExceptionRecord
)
{
	// clang-format off
	__asm {
		push ebp
		mov ebp, esp
		sub esp, SIZE CONTEXT
		push esp
		call RtlCaptureContext
		add dword ptr [esp]CONTEXT.Esp, 4 // pop ExceptionRecord argument
		mov dword ptr [esp]CONTEXT.ContextFlags, CONTEXT_CONTROL | CONTEXT_INTEGER | CONTEXT_SEGMENTS // set ContextFlags member of CONTEXT
		mov eax, dword ptr [ebp + 8]
		mov ecx, dword ptr [ebp + 4]
		mov dword ptr [eax]EXCEPTION_RECORD.ExceptionAddress, ecx // set ExceptionAddress member of ExceptionRecord argument to caller's eip
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
		push dword ptr [ebp + 8]
		call ZwRaiseException // won't return
	}
	// clang-format on
}

EXPORTNUM(27) __declspec(naked) VOID XBOXAPI ExRaiseStatus
(
	NTSTATUS Status
)
{
	// clang-format off
	__asm {
		push ebp
		mov ebp, esp
		sub esp, SIZE CONTEXT + SIZE EXCEPTION_RECORD
		push esp
		call RtlCaptureContext
		add dword ptr [esp]CONTEXT.Esp, 4 // pop Status argument
		mov dword ptr [esp]CONTEXT.ContextFlags, CONTEXT_CONTROL | CONTEXT_INTEGER | CONTEXT_SEGMENTS // set ContextFlags member of CONTEXT
		lea ecx, dword ptr [ebp - SIZE EXCEPTION_RECORD]
		mov eax, dword ptr [ebp + 8]
		mov dword ptr [ecx]EXCEPTION_RECORD.ExceptionCode, eax // set ExceptionCode member of ExceptionRecord to Status argument
		mov dword ptr [ecx]EXCEPTION_RECORD.ExceptionFlags, EXCEPTION_NONCONTINUABLE
		mov dword ptr [ecx]EXCEPTION_RECORD.ExceptionRecord, 0
		mov eax, dword ptr [ebp + 4]
		mov dword ptr [ecx]EXCEPTION_RECORD.ExceptionAddress, eax // set ExceptionAddress member of ExceptionRecord to caller's eip
		mov dword ptr [ecx]EXCEPTION_RECORD.NumberParameters, 0
		push esp
		push ecx
		call RtlDispatchException
		// Because the exception is non-continuable, RtlDispatchException should never return. If it does return, then it must be a bug
		push NORETURN_FUNCTION_RETURNED
		call KeBugCheckLogEip // won't return
	}
	// clang-format on
}
