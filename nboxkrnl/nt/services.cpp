/*
 * ergo720                Copyright (c) 2023
 */

#include "zw.hpp"


VOID XBOXAPI ZwContinue(PCONTEXT ContextRecord, BOOLEAN TestAlert)
{
	ASM_BEGIN
		ASM(mov ecx, ContextRecord);
		ASM(movzx edx, TestAlert);
		ASM(int IDT_SERVICE_VECTOR_BASE + 8); // calls KiContinueService
	ASM_END
}

VOID XBOXAPI ZwRaiseException(PEXCEPTION_RECORD ExceptionRecord, PCONTEXT ContextRecord, BOOLEAN FirstChance)
{
	ASM_BEGIN
		ASM(mov ecx, ExceptionRecord);
		ASM(mov edx, ContextRecord);
		ASM(movzx eax, FirstChance);
		ASM(int IDT_SERVICE_VECTOR_BASE + 9); // calls KiRaiseExceptionService
	ASM_END
}
