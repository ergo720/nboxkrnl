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

VOID __SEH_prolog();
VOID __SEH_epilog();

// NOTE1: every function invoking SEH_Create must use __declspec(naked), or else the prologue emitted by the compiler will break the call to __SEH_prolog. Also, every
// call to SEH_Create must have a corresponding call to SEH_Destroy used as the epilog of the function
// NOTE2: StackUsedByArgs is the size, in bytes, of the stack used by the arguments of the function. This is used for stdcall and fastcall functions, since they must release
// that number of bytes before returning. On the contrary, cdecl functions don't release them (the caller does), and so StackUsedByArgs must be zero instead

#define SEH_Create(SEHTable) \
    __asm push __LOCAL_SIZE \
    __asm push offset SEHTable \
    __asm call offset __SEH_prolog

#define SEH_Destroy(StackUsedByArgs) \
    __asm call offset __SEH_epilog \
    __asm ret StackUsedByArgs
