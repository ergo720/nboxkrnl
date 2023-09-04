/*
 * ergo720                Copyright (c) 2023
 */

#include "zw.hpp"


NTSTATUS XBOXAPI ZwContinue(PCONTEXT ContextRecord, BOOLEAN TestAlert)
{
	// TODO

	return STATUS_SUCCESS;
}

__declspec(naked) VOID XBOXAPI ZwRaiseException(PEXCEPTION_RECORD ExceptionRecord, PCONTEXT ContextRecord, BOOLEAN FirstChance)
{
	__asm {
		mov ecx, ExceptionRecord
		mov edx, ContextRecord
		movzx eax, FirstChance
		int IDT_SERVICE_VECTOR_BASE + 9 // calls KiRaiseExceptionService
		ret 12
	}
}
