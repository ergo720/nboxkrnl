/*
 * ergo720                Copyright (c) 2023
 */

#include "exp_sup.hpp"
#include "rtl.hpp"
#include "..\nt\zw.hpp"


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
	KeGetExceptionHead([NestedUnwindFrame].Prev);
	if constexpr (IsUnwind) {
		NestedUnwindFrame.Handler = static_cast<PVOID>(RtlpNestedUnwindHandler);
	}
	else {
		NestedUnwindFrame.Handler = static_cast<PVOID>(RtlpNestedExceptionHandler);
	}
	KeSetExceptionHead(NestedUnwindFrame);
	EXCEPTION_DISPOSITION Result = ExceptionRoutine(ExceptionRecord, RegistrationFrame, ContextRecord, pDispatcherContext);
	KeResetExceptionHead([NestedUnwindFrame].Prev);
	return Result;
}

static BOOLEAN RtlpVerifyStackLimitsForExceptionFrame(PEXCEPTION_REGISTRATION_RECORD RegistrationFrame, PULONG StackBase, PULONG StackLimit)
{
	ULONG RegistrationLowAddress = reinterpret_cast<ULONG>(RegistrationFrame);
	ULONG RegistrationHighAddress = RegistrationLowAddress + sizeof(EXCEPTION_REGISTRATION_RECORD);

	if ((RegistrationHighAddress > *StackBase) || // outside of stack top
		(RegistrationLowAddress < *StackLimit) || // outside of stack bottom
		(RegistrationLowAddress & 3)) { // not 4 byte aligned

		// Check to see if it failed because the exception occurred in the DPC stack

		if (((RegistrationLowAddress & 3) == 0) && // must be 4 byte aligned
			(KeGetCurrentIrql() >= DISPATCH_LEVEL)) {  // DPCs must execute at or above DISPATCH_LEVEL

			ULONG DpcStackBase, DpcRoutineActive;
			KeGetDpcStack(DpcStackBase);
			KeGetDpcActive(DpcRoutineActive);

			if (DpcRoutineActive && // we must be running a DPC
				(RegistrationHighAddress <= DpcStackBase) && // inside of DPC stack top
				(RegistrationLowAddress >= (DpcStackBase - KERNEL_STACK_SIZE))) { // inside of DPC stack bottom

				*StackBase = DpcStackBase;
				*StackLimit = DpcStackBase - KERNEL_STACK_SIZE;
				return TRUE;
			}
		}

		return FALSE;
	}

	return TRUE;
}

EXPORTNUM(302) __declspec(naked) VOID XBOXAPI RtlRaiseException
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
		ASM(add [esp]CONTEXT.Esp, 4); // pop ExceptionRecord argument
		ASM(mov [esp]CONTEXT.ContextFlags, CONTEXT_CONTROL | CONTEXT_INTEGER | CONTEXT_SEGMENTS); // set ContextFlags member of CONTEXT
		ASM(mov eax, [ebp + 8]);
		ASM(mov ecx, [ebp + 4]);
		ASM(mov [eax]EXCEPTION_RECORD.ExceptionAddress, ecx); // set ExceptionAddress member of ExceptionRecord argument to caller's eip
		ASM(mov ecx, esp);
		ASM(push TRUE);
		ASM(push ecx);
		ASM(push [ebp + 8]);
		ASM(call ZwRaiseException);
		// ZwRaiseException should never return. It would only be possible if KiRaiseException fails before copying the CONTEXT to the KTRAP_FRAME, but the
		// function right now always succeeds
		ASM(push NORETURN_FUNCTION_RETURNED);
		ASM(call KeBugCheckLogEip); // won't return
	ASM_END
}

BOOLEAN XBOXAPI RtlDispatchException(PEXCEPTION_RECORD ExceptionRecord, PCONTEXT ContextRecord)
{
	ULONG StackBase, StackLimit;
	PEXCEPTION_REGISTRATION_RECORD RegistrationPointer;
	EXCEPTION_REGISTRATION_RECORD** ppRegistrationFrame = nullptr;
	KeGetStackBase(StackBase);
	KeGetStackLimit(StackLimit);
	KeGetExceptionHead(RegistrationPointer);

	while (RegistrationPointer != EXCEPTION_CHAIN_END) {

		if (RtlpVerifyStackLimitsForExceptionFrame(RegistrationPointer, &StackBase, &StackLimit) == FALSE) {
			ExceptionRecord->ExceptionFlags |= EXCEPTION_STACK_INVALID;
			return FALSE;
		}

		EXCEPTION_DISPOSITION Result = RtlpExecuteHandler<false>(ExceptionRecord, RegistrationPointer, ContextRecord,
			ppRegistrationFrame, static_cast<PEXCEPTION_ROUTINE>(RegistrationPointer->Handler));

		switch (Result)
		{
		case ExceptionNestedException:
			RIP_API_MSG("unhandled nested exception");
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
			ExceptionRecord1.ExceptionCode = STATUS_INVALID_DISPOSITION;
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

EXPORTNUM(303) VOID XBOXAPI RtlRaiseStatus
(
	NTSTATUS Status
)
{
	EXCEPTION_RECORD ExceptionRecord;
	ExceptionRecord.ExceptionCode = Status;
	ExceptionRecord.ExceptionFlags = EXCEPTION_NONCONTINUABLE;
	ExceptionRecord.ExceptionRecord = nullptr;
	ExceptionRecord.NumberParameters = 0;

	RtlRaiseException(&ExceptionRecord);
}

#pragma optimize("y", off)

EXPORTNUM(312) __declspec(noinline) VOID XBOXAPI RtlUnwind
(
	PVOID TargetFrame,
	PVOID TargetIp,
	PEXCEPTION_RECORD ExceptionRecord,
	PVOID ReturnValue
)
{
	// NOTE: this function must never be inlined, or else we won't be able to capture the return address placed by the caller on the stack. It also expects that
	// the compiler has generated the standard prologue sequence push ebp / mov ebp, esp

	EXCEPTION_RECORD LocalExceptionRecord;
	if (!ExceptionRecord) {
		ExceptionRecord = &LocalExceptionRecord;
		ExceptionRecord->ExceptionCode = STATUS_UNWIND;
		ExceptionRecord->ExceptionFlags = 0;
		ExceptionRecord->ExceptionRecord = nullptr;
		ExceptionRecord->NumberParameters = 0;
		ASM_BEGIN
			ASM(mov eax, dword ptr [ebp + 4]);
			ASM(mov dword ptr [LocalExceptionRecord].ExceptionAddress, eax);
		ASM_END
	}

	ExceptionRecord->ExceptionFlags |= EXCEPTION_UNWINDING;
	if (!TargetFrame) {
		ExceptionRecord->ExceptionFlags |= EXCEPTION_EXIT_UNWIND;
	}

	// Capture the context of the caller of RtlUnwind
	CONTEXT ContextRecord;
	ContextRecord.ContextFlags = CONTEXT_INTEGER | CONTEXT_CONTROL | CONTEXT_SEGMENTS;
	RtlCaptureContext(&ContextRecord);
	ContextRecord.Esp += 16;
	ContextRecord.Eax = reinterpret_cast<ULONG>(ReturnValue);

	ULONG StackBase, StackLimit;
	PEXCEPTION_REGISTRATION_RECORD RegistrationPointer;
	EXCEPTION_REGISTRATION_RECORD** ppRegistrationFrame = nullptr;
	KeGetStackBase(StackBase);
	KeGetStackLimit(StackLimit);
	KeGetExceptionHead(RegistrationPointer);

	while (RegistrationPointer != EXCEPTION_CHAIN_END) {
		if (RegistrationPointer == static_cast<PEXCEPTION_REGISTRATION_RECORD>(TargetFrame)) {

			// NOTE: On the xbox, the TargetIp argument is ignored, and RtlUnwind always returns to its caller when it's done its job. Unfortunately, we must still use ZwContinue to
			// return to the caller because we need to set the return value. We cannot just change the return type from VOID to something else because this is a kernel export
			ZwContinue(&ContextRecord, FALSE);
		}
		else if (TargetFrame && (static_cast<PEXCEPTION_REGISTRATION_RECORD>(TargetFrame) < RegistrationPointer)) {

			// Since the stack grows down, and we are scanning the exception records while going upwards on the stack, the TargetFrame is wrong if we reach here
			EXCEPTION_RECORD ExceptionRecord1;
			ExceptionRecord1.ExceptionCode = STATUS_INVALID_UNWIND_TARGET;
			ExceptionRecord1.ExceptionFlags = EXCEPTION_NONCONTINUABLE;
			ExceptionRecord1.ExceptionRecord = ExceptionRecord;
			ExceptionRecord1.NumberParameters = 0;
			RtlRaiseException(&ExceptionRecord1); // won't return
		}

		if (RtlpVerifyStackLimitsForExceptionFrame(RegistrationPointer, &StackBase, &StackLimit) == FALSE) {
			EXCEPTION_RECORD ExceptionRecord1;
			ExceptionRecord1.ExceptionCode = STATUS_BAD_STACK;
			ExceptionRecord1.ExceptionFlags = EXCEPTION_NONCONTINUABLE;
			ExceptionRecord1.ExceptionRecord = ExceptionRecord;
			ExceptionRecord1.NumberParameters = 0;
			RtlRaiseException(&ExceptionRecord1); // won't return
		}

		EXCEPTION_DISPOSITION Result = RtlpExecuteHandler<true>(ExceptionRecord, RegistrationPointer, &ContextRecord,
			ppRegistrationFrame, static_cast<PEXCEPTION_ROUTINE>(RegistrationPointer->Handler));

		switch (Result)
		{
		case ExceptionContinueSearch:
			break;

		case ExceptionCollidedUnwind:
			RIP_API_MSG("unhandled nested exception");
			break;

		case ExceptionNestedException:
		case ExceptionContinueExecution:
		default: {
			EXCEPTION_RECORD ExceptionRecord1;
			ExceptionRecord1.ExceptionCode = STATUS_INVALID_DISPOSITION;
			ExceptionRecord1.ExceptionFlags = EXCEPTION_NONCONTINUABLE;
			ExceptionRecord1.ExceptionRecord = ExceptionRecord;
			ExceptionRecord1.NumberParameters = 0;
			RtlRaiseException(&ExceptionRecord1); // won't return
		}
		}

		ASM_BEGIN
			ASM(mov eax, [RegistrationPointer]);
			ASM(mov eax, [eax]);
			ASM(mov dword ptr [KiPcr].NtTib.ExceptionList, eax);
		ASM_END

		RegistrationPointer = RegistrationPointer->Prev;
	}

	if (TargetFrame == EXCEPTION_CHAIN_END) {

		// NOTE: On the xbox, the TargetIp argument is ignored, and RtlUnwind always returns to its caller when it's done its job. Unfortunately, we must still use ZwContinue to
		// return to the caller because we need to set the return value. We cannot just change the return type from VOID to something else because this is a kernel export
		ZwContinue(&ContextRecord, FALSE);
	}
	else {
		// This means that we couldn't find the specified TargetFrame in the chain for this thread, so the TargetFrame is probably for another thread
		ZwRaiseException(ExceptionRecord, &ContextRecord, FALSE);
	}
}

#pragma optimize("y", on)
