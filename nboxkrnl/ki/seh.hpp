/*
 * ergo720                Copyright (c) 2023
 */

#pragma once

#include "..\types.hpp"
#include "ki.hpp"

#define EXCEPTION_NONCONTINUABLE  0x01
#define EXCEPTION_UNWINDING       0x02
#define EXCEPTION_EXIT_UNWIND     0x04
#define EXCEPTION_STACK_INVALID   0x08
#define EXCEPTION_NESTED_CALL     0x10
#define EXCEPTION_TARGET_UNWIND   0x20
#define EXCEPTION_COLLIDED_UNWIND 0x40
#define EXCEPTION_UNWIND (EXCEPTION_UNWINDING | EXCEPTION_EXIT_UNWIND | EXCEPTION_TARGET_UNWIND | EXCEPTION_COLLIDED_UNWIND)
#define EXCEPTION_MAXIMUM_PARAMETERS 15
#define TRYLEVEL_NONE -1

#define EXCEPTION_EXECUTE_HANDLER 1
#define EXCEPTION_CONTINUE_SEARCH 0
#define EXCEPTION_CONTINUE_EXECUTION (-1)


enum EXCEPTION_DISPOSITION {
	ExceptionContinueExecution = 0,
	ExceptionContinueSearch = 1,
	ExceptionNestedException = 2,
	ExceptionCollidedUnwind = 3,
};

struct EXCEPTION_REGISTRATION_RECORD {
	struct EXCEPTION_REGISTRATION_RECORD *Prev;
	PVOID Handler;
};
using PEXCEPTION_REGISTRATION_RECORD = EXCEPTION_REGISTRATION_RECORD *;

struct EXCEPTION_RECORD {
	NTSTATUS ExceptionCode;
	ULONG ExceptionFlags;
	struct EXCEPTION_RECORD *ExceptionRecord;
	PVOID ExceptionAddress;
	ULONG NumberParameters;
	ULONG_PTR ExceptionInformation[EXCEPTION_MAXIMUM_PARAMETERS];
};
using PEXCEPTION_RECORD = EXCEPTION_RECORD *;

struct ScopeTableEntry {
	DWORD EnclosingLevel;
	VOID *FilterFunction;
	VOID *HandlerFunction;
};

struct EXCEPTION_REGISTRATION_SEH : EXCEPTION_REGISTRATION_RECORD {
	ScopeTableEntry *ScopeTable;
	DWORD TryLevel;
	DWORD _ebp;
};

struct EXCEPTION_POINTERS {
	PEXCEPTION_RECORD ExceptionRecord;
	PCONTEXT ContextRecord;
};
using PEXCEPTION_POINTERS = EXCEPTION_POINTERS *;

using PEXCEPTION_ROUTINE = EXCEPTION_DISPOSITION(CDECL *)(EXCEPTION_RECORD *, EXCEPTION_REGISTRATION_RECORD *,
	CONTEXT *, EXCEPTION_REGISTRATION_RECORD **);


inline PEXCEPTION_POINTERS __declspec(naked) GetExceptionInformation()
{
	// Retrieves excptPtrs written by _except_handler3 right below EXCEPTION_REGISTRATION_SEH
	// This function must be naked because it needs to use the ebp of its caller, not the ebp of this function

	ASM_BEGIN
		ASM(mov eax, dword ptr [ebp - 20]);
		ASM(ret);
	ASM_END
}

inline NTSTATUS __declspec(naked) GetExceptionCode()
{
	// Retrieves the ExceptionCode member of EXCEPTION_RECORD pointed by ExceptionRecord of excptPtrs
	// This function must be naked because it needs to use the ebp of its caller, not the ebp of this function

	ASM_BEGIN
		ASM(mov eax, dword ptr [ebp - 20]);
		ASM(mov eax, [eax]);
		ASM(mov eax, [eax]);
		ASM(ret);
	ASM_END
}
