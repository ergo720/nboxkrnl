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

VOID __SEH_prolog();
VOID __SEH_epilog();

// NOTE1: every function invoking SEH_Create must use __declspec(naked), or else the prologue emitted by the compiler will break the call to __SEH_prolog. Also, every
// call to SEH_Create must have a corresponding call to SEH_Destroy used as the epilogue of the function
// NOTE2: StackUsedByArgs is the size, in bytes, of the stack used by the arguments of the function. This is used for stdcall and fastcall functions, since they must release
// that number of bytes before returning. On the contrary, cdecl functions don't release them (the caller does), and so StackUsedByArgs must be zero instead

#define SEH_Create(SEHTable) \
	__asm { \
		__asm push __LOCAL_SIZE + 8 \
		__asm push offset SEHTable \
		__asm call offset __SEH_prolog \
	}

#define SEH_Destroy(StackUsedByArgs) \
	__asm { \
		__asm call offset __SEH_epilog \
		__asm ret StackUsedByArgs \
	}

// These macros mimic the __try, __except and __finally keywords used by MSVC for SEH exception handling, and should be used in the same manner.
// For every _START macro used, there must be a corresponding _END macro too. Using both EXCEPT and FINALLY macros at the same TryLevel will result in erroneous behaviour.
// Every new TryLevel used in SEH_TRY_START must be different, and they should index the ScopeTableEntry that should handle this TryLevel when an exception is triggered

#define SEH_TRY_START(lv) \
	__asm { mov dword ptr [ebp - 4], lv } // set TryLevel member of EXCEPTION_REGISTRATION_SEH

#define SEH_TRY_END_WITH_EXCEPT(dst) \
	__asm { \
		__asm mov dword ptr [ebp - 4], TRYLEVEL_NONE \
		__asm jmp dst \
	}

#define SEH_TRY_END_WITH_FINALLY(dst) \
	__asm { \
		__asm mov dword ptr [ebp - 4], TRYLEVEL_NONE \
		__asm call offset dst \
	}

#define SEH_EXCEPT_START() \
	__asm { mov esp, dword ptr [ebp - 24] } // restore esp of this SEH guarded function from saved esp pushed by __SEH_prolog

#define SEH_EXCEPT_END(dst) \
	__asm { \
		__asm mov dword ptr [ebp - 4], TRYLEVEL_NONE \
		__asm dst: \
	}

#define SEH_FINALLY_START(dst) \
	__asm { dst: }

#define SEH_FINALLY_END() \
	__asm { ret }

// retrieve excptPtrs written by _except_handler3 right below EXCEPTION_REGISTRATION_SEH
#define GetExceptionInformation(var) \
	__asm { \
		__asm mov eax, dowrd ptr [ebp - 20] \
		__asm mov var, eax \
	}

// retrieve the ExceptionCode member of EXCEPTION_RECORD pointed by ExceptionRecord of excptPtrs
#define GetExceptionCode(var) \
	__asm { \
		__asm mov eax, dowrd ptr [ebp - 20] \
		__asm mov eax, [eax] \
		__asm mov eax, [eax] \
		__asm mov var, eax \
	}
