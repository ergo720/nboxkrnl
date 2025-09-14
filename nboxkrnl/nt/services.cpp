/*
 * ergo720                Copyright (c) 2023
 */

#include "zw.hpp"


 // clang-format off
VOID XBOXAPI ZwContinue(PCONTEXT ContextRecord, BOOLEAN TestAlert)
{
	__asm {
		mov ecx, ContextRecord
		movzx edx, TestAlert
		int IDT_SERVICE_VECTOR_BASE + 8 // calls KiContinueService
	}
}

VOID XBOXAPI ZwRaiseException(PEXCEPTION_RECORD ExceptionRecord, PCONTEXT ContextRecord, BOOLEAN FirstChance)
{
	__asm {
		mov ecx, ExceptionRecord
		mov edx, ContextRecord
		movzx eax, FirstChance
		int IDT_SERVICE_VECTOR_BASE + 9 // calls KiRaiseExceptionService
	}
}
// clang-format on
