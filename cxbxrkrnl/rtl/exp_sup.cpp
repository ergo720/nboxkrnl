/*
 * ergo720                Copyright (c) 2023
 */

#include "exp_sup.hpp"


static EXCEPTION_DISPOSITION CDECL RtlpNestedExceptionHandler(EXCEPTION_RECORD *pExceptionRecord, EXCEPTION_REGISTRATION_SEH *pRegistrationFrame,
    CONTEXT *pContextRecord, EXCEPTION_REGISTRATION_RECORD **pDispatcherContext)
{
    if (pExceptionRecord->ExceptionFlags & (EXCEPTION_UNWINDING | EXCEPTION_EXIT_UNWIND)) {
        return ExceptionContinueSearch;
    }

    *pDispatcherContext = pRegistrationFrame;
    return ExceptionNestedException;
}

static EXCEPTION_DISPOSITION CDECL RtlpNestedUnwindHandler(EXCEPTION_RECORD *pExceptionRecord, EXCEPTION_REGISTRATION_SEH *pRegistrationFrame,
    CONTEXT *pContextRecord, EXCEPTION_REGISTRATION_RECORD **pDispatcherContext)
{
    if (!(pExceptionRecord->ExceptionFlags & (EXCEPTION_UNWINDING | EXCEPTION_EXIT_UNWIND))) {
        return ExceptionContinueSearch;
    }

    *pDispatcherContext = pRegistrationFrame;
    return ExceptionCollidedUnwind;
}

template<bool IsUnwind>
static EXCEPTION_DISPOSITION RtlpExecuteHandler(PEXCEPTION_RECORD ExceptionRecord, PEXCEPTION_REGISTRATION_RECORD RegistrationFrame, PCONTEXT ContextRecord,
    EXCEPTION_REGISTRATION_RECORD **pDispatcherContext, PEXCEPTION_ROUTINE ExceptionRoutine)
{
    // ExceptionRoutine is either _except_handler3, _nested_unwind_handler, RtlpNestedExceptionHandler or RtlpNestedUnwindHandler

    EXCEPTION_REGISTRATION_RECORD NestedUnwindFrame;
    NestedUnwindFrame.Prev = KeGetPcr()->NtTib.ExceptionList;
    if constexpr (IsUnwind) {
        NestedUnwindFrame.Handler = static_cast<PVOID>(RtlpNestedUnwindHandler);
    }
    else {
        NestedUnwindFrame.Handler = static_cast<PVOID>(RtlpNestedExceptionHandler);
    }
    KeGetPcr()->NtTib.ExceptionList = &NestedUnwindFrame;
    EXCEPTION_DISPOSITION Result = ExceptionRoutine(ExceptionRecord, RegistrationFrame, ContextRecord, pDispatcherContext);
    KeGetPcr()->NtTib.ExceptionList = KeGetPcr()->NtTib.ExceptionList->Prev;
    return Result;
}

EXPORTNUM(302) VOID XBOXAPI RtlRaiseException(PEXCEPTION_RECORD ExceptionRecord)
{
    // TODO
}

BOOLEAN RtlDispatchException(PEXCEPTION_RECORD ExceptionRecord, PCONTEXT ContextRecord)
{
    ULONG StackBase = reinterpret_cast<ULONG>(KeGetPcr()->NtTib.StackBase);
    ULONG StackLimit = reinterpret_cast<ULONG>(KeGetPcr()->NtTib.StackLimit);
    PEXCEPTION_REGISTRATION_RECORD RegistrationPointer = KeGetPcr()->NtTib.ExceptionList;
    PEXCEPTION_REGISTRATION_RECORD NestedRegistration = nullptr;
    EXCEPTION_REGISTRATION_RECORD **ppRegistrationFrame = nullptr;

    while (RegistrationPointer != EXCEPTION_CHAIN_END) {
        // We check that the RegistrationPointer is inside the limits of the current stack, and that it's not misaligned

        ULONG RegistrationLowAddress = reinterpret_cast<ULONG>(RegistrationPointer);
        ULONG RegistrationHighAddress = RegistrationLowAddress + sizeof(EXCEPTION_REGISTRATION_RECORD);

        if ((RegistrationHighAddress > StackBase) || // outside of stack top
            (RegistrationLowAddress < StackLimit) || // outside of stack bottom
            (RegistrationLowAddress & 3)) { // not 4 byte aligned

            // Check to see if it failed because the exception occurred in the DPC stack

            if (((RegistrationLowAddress & 3) == 0) && // must be 4 byte aligned
                (KeGetCurrentIrql() >= DISPATCH_LEVEL)) {  // DPCs must execute at or above DISPATCH_LEVEL

                ULONG DpcStackBase = reinterpret_cast<ULONG>(KeGetPrcb()->DpcStack);

                if ((KeGetPrcb()->DpcRoutineActive) && // we must be running a DPC
                    (RegistrationHighAddress <= DpcStackBase) && // inside of DPC stack top
                    (RegistrationLowAddress >= (DpcStackBase - KERNEL_STACK_SIZE))) { // inside of DPC stack bottom

                    StackBase = DpcStackBase;
                    StackLimit = DpcStackBase - KERNEL_STACK_SIZE;
                    continue;
                }
            }

            ExceptionRecord->ExceptionFlags |= EXCEPTION_STACK_INVALID;
            return FALSE;
        }

        EXCEPTION_DISPOSITION Result = RtlpExecuteHandler<false>(ExceptionRecord, RegistrationPointer, ContextRecord,
            ppRegistrationFrame, static_cast<PEXCEPTION_ROUTINE>(RegistrationPointer->Handler));

        switch (Result)
        {
        case ExceptionNestedException:
            // TODO
            break;

        case ExceptionContinueExecution:
            if (ExceptionRecord->ExceptionFlags & EXCEPTION_NONCONTINUABLE) {
                EXCEPTION_RECORD ExceptionRecord1;
                ExceptionRecord1.ExceptionCode = STATUS_NONCONTINUABLE_EXCEPTION;
                ExceptionRecord1.ExceptionFlags = EXCEPTION_NONCONTINUABLE;
                ExceptionRecord1.ExceptionRecord = ExceptionRecord;
                ExceptionRecord1.NumberParameters = 0;
                RtlRaiseException(&ExceptionRecord1); // won't return
            }
            else {
                return TRUE;
            }
            break;

        case ExceptionContinueSearch:
            break;

        case ExceptionCollidedUnwind:
        default: {
            EXCEPTION_RECORD ExceptionRecord1;
            ExceptionRecord1.ExceptionCode = STATUS_NONCONTINUABLE_EXCEPTION;
            ExceptionRecord1.ExceptionFlags = EXCEPTION_NONCONTINUABLE;
            ExceptionRecord1.ExceptionRecord = ExceptionRecord;
            ExceptionRecord1.NumberParameters = 0;
            RtlRaiseException(&ExceptionRecord1); // won't return
        }
        }

        RegistrationPointer = RegistrationPointer->Prev;
    }

    return FALSE;
}
