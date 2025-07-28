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
	ASM_BEGIN
		ASM(push ebp);
		ASM(mov ebp, esp);
		ASM(sub esp, SIZE CONTEXT);
		ASM(push esp);
		ASM(call RtlCaptureContext);
		ASM(add dword ptr [esp]CONTEXT.Esp, 4); // pop ExceptionRecord argument
		ASM(mov dword ptr [esp]CONTEXT.ContextFlags, CONTEXT_CONTROL | CONTEXT_INTEGER | CONTEXT_SEGMENTS); // set ContextFlags member of CONTEXT
		ASM(mov eax, dword ptr [ebp + 8]);
		ASM(mov ecx, dword ptr [ebp + 4]);
		ASM(mov dword ptr [eax]EXCEPTION_RECORD.ExceptionAddress, ecx); // set ExceptionAddress member of ExceptionRecord argument to caller's eip
		ASM(push esp);
		ASM(push eax);
		ASM(call RtlDispatchException);
		// If the exception is continuable, then RtlDispatchException will return
		ASM(mov ecx, esp);
		ASM(push FALSE);
		ASM(push ecx);
		ASM(test al, al);
		ASM(jz exp_unhandled);
		ASM(call ZwContinue); // won't return
	exp_unhandled:
		ASM(push dword ptr [ebp + 8]);
		ASM(call ZwRaiseException); // won't return
	ASM_END
}

EXPORTNUM(27) __declspec(naked) VOID XBOXAPI ExRaiseStatus
(
	NTSTATUS Status
)
{
	ASM_BEGIN
		ASM(push ebp);
		ASM(mov ebp, esp);
		ASM(sub esp, SIZE CONTEXT + SIZE EXCEPTION_RECORD);
		ASM(push esp);
		ASM(call RtlCaptureContext);
		ASM(add dword ptr [esp]CONTEXT.Esp, 4); // pop Status argument
		ASM(mov dword ptr [esp]CONTEXT.ContextFlags, CONTEXT_CONTROL | CONTEXT_INTEGER | CONTEXT_SEGMENTS); // set ContextFlags member of CONTEXT
		ASM(lea ecx, dword ptr [ebp - SIZE EXCEPTION_RECORD]);
		ASM(mov eax, dword ptr [ebp + 8]);
		ASM(mov dword ptr [ecx]EXCEPTION_RECORD.ExceptionCode, eax); // set ExceptionCode member of ExceptionRecord to Status argument
		ASM(mov dword ptr [ecx]EXCEPTION_RECORD.ExceptionFlags, EXCEPTION_NONCONTINUABLE);
		ASM(mov dword ptr [ecx]EXCEPTION_RECORD.ExceptionRecord, 0);
		ASM(mov eax, dword ptr [ebp + 4]);
		ASM(mov dword ptr [ecx]EXCEPTION_RECORD.ExceptionAddress, eax); // set ExceptionAddress member of ExceptionRecord to caller's eip
		ASM(mov dword ptr [ecx]EXCEPTION_RECORD.NumberParameters, 0);
		ASM(push esp);
		ASM(push ecx);
		ASM(call RtlDispatchException);
		// Because the exception is non-continuable, RtlDispatchException should never return. If it does return, then it must be a bug
		ASM(push NORETURN_FUNCTION_RETURNED);
		ASM(call KeBugCheckLogEip); // won't return
	ASM_END
}
