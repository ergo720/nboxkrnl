/*
 * ergo720                Copyright (c) 2023
 */

#pragma once

#include "..\types.hpp"
#include "..\ki\seh.hpp"


NTSTATUS XBOXAPI ZwContinue(PCONTEXT ContextRecord, BOOLEAN TestAlert);
VOID XBOXAPI ZwRaiseException(PEXCEPTION_RECORD ExceptionRecord, PCONTEXT ContextRecord, BOOLEAN FirstChance);
