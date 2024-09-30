/*
 * ergo720                Copyright (c) 2023
 * Stefan Schmidt         Copyright (c) 2020-2023
 * Lucas Jansson          Copyright (c) 2021
 */

#include "seh.hpp"
#include "rtl.hpp"


EXCEPTION_DISPOSITION CDECL _nested_unwind_handler(EXCEPTION_RECORD *pExceptionRecord, EXCEPTION_REGISTRATION_SEH *pRegistrationFrame,
	CONTEXT *pContextRecord, EXCEPTION_REGISTRATION_RECORD **pDispatcherContext)
{
	if (!(pExceptionRecord->ExceptionFlags & (EXCEPTION_UNWINDING | EXCEPTION_EXIT_UNWIND))) {
		return ExceptionContinueSearch;
	}

	*pDispatcherContext = pRegistrationFrame;
	return ExceptionCollidedUnwind;
}

static inline void CDECL call_ebp_func(void *func, void *_ebp)
{
	ASM_BEGIN
		ASM(mov eax, func);
		ASM(push ebp);
		ASM(mov ebp, _ebp);
		ASM(call eax);
		ASM(pop ebp);
	ASM_END
}

void _local_unwind2(EXCEPTION_REGISTRATION_SEH *pRegistrationFrame, int stop)
{
	// Manually install exception handler frame
	EXCEPTION_REGISTRATION_RECORD nestedUnwindFrame;
	KeGetExceptionHead([nestedUnwindFrame].Prev);
	nestedUnwindFrame.Handler = reinterpret_cast<void *>(_nested_unwind_handler);
	KeSetExceptionHead(nestedUnwindFrame);

	const ScopeTableEntry *scopeTable = pRegistrationFrame->ScopeTable;

	while (true) {
		LONG currentTrylevel = pRegistrationFrame->TryLevel;

		if (currentTrylevel == TRYLEVEL_NONE) {
			break;
		}

		if (stop != TRYLEVEL_NONE && currentTrylevel <= stop) {
			break;
		}

		LONG oldTrylevel = currentTrylevel;
		pRegistrationFrame->TryLevel = scopeTable[currentTrylevel].EnclosingLevel;

		if (!scopeTable[oldTrylevel].FilterFunction) {
			// If no filter funclet is present, then it's a __finally statement
			// instead of an __except statement
			call_ebp_func(scopeTable[oldTrylevel].HandlerFunction, &pRegistrationFrame->_ebp);
		}
	}

	// Manually remove exception handler frame
	KeResetExceptionHead([nestedUnwindFrame].Prev);
}

void _global_unwind2(EXCEPTION_REGISTRATION_SEH *pRegistrationFrame)
{
	// NOTE: RtlUnwind will trash all the non-volatile registers (save for ebp) despite being stdcall. This happens because the register context is captured with RtlCaptureContext
	// only after some of the function has already executed, and at that point the non-volatile registers are likely already trashed

	ASM_BEGIN
		ASM(push ebx);
		ASM(push esi);
		ASM(push edi);
		ASM(push 0);
		ASM(push 0);
		ASM(push 0);
		ASM(push pRegistrationFrame);
		ASM(call RtlUnwind);
		ASM(pop edi);
		ASM(pop esi);
		ASM(pop ebx);
	ASM_END
}

// This function must use extern "C" so that MSVC can link against our implementation of _except_handler3 when we use __try / __except in the kernel. It's also necessary
// that this project does not use whole program optimization or else MSVC will attempt to link against _except_handler4 instead, causing a linking error
extern "C"  EXCEPTION_DISPOSITION CDECL _except_handler3(EXCEPTION_RECORD* pExceptionRecord, EXCEPTION_REGISTRATION_SEH* pRegistrationFrame,
	CONTEXT *pContextRecord, EXCEPTION_REGISTRATION_RECORD **pDispatcherContext)
{
	// Clear the direction flag - the function triggering the exception might
	// have modified it, but it's expected to not be set
	ASM(cld);

	if (pExceptionRecord->ExceptionFlags & (EXCEPTION_UNWINDING | EXCEPTION_EXIT_UNWIND)) {
		// We're in an unwinding pass, so unwind all local scopes
		_local_unwind2(pRegistrationFrame, TRYLEVEL_NONE);
		return ExceptionContinueSearch;
	}

	// A pointer to a EXCEPTION_POINTERS structure needs to be put below
	// the registration pointer. This is required because the intrinsics
	// implementing GetExceptionInformation() and GetExceptionCode() retrieve
	// their information from this structure.
	// NOTE: notice the double pointer cast below, so this will write at (pRegistrationFrame - 4), the correct spot for &excptPtrs
	EXCEPTION_POINTERS excptPtrs;
	excptPtrs.ExceptionRecord = pExceptionRecord;
	excptPtrs.ContextRecord = pContextRecord;
	reinterpret_cast<volatile PEXCEPTION_POINTERS *>(pRegistrationFrame)[-1] = &excptPtrs;

	const ScopeTableEntry *scopeTable = pRegistrationFrame->ScopeTable;
	LONG currentTrylevel = pRegistrationFrame->TryLevel;

	// Search all scopes from the inside out trying to find a filter that accepts the exception
	while (currentTrylevel != TRYLEVEL_NONE) {
		const void *filterFunclet = scopeTable[currentTrylevel].FilterFunction;
		if (filterFunclet) {
			const DWORD _ebp = (DWORD)&pRegistrationFrame->_ebp;
			LONG filterResult;

			ASM_BEGIN
				ASM(mov eax, filterFunclet);
				ASM(push ebp);
				ASM(mov ebp, _ebp);
				ASM(call eax);
				ASM(pop ebp);
				ASM(mov filterResult, eax);
			ASM_END

			if (filterResult != EXCEPTION_CONTINUE_SEARCH) {
				if (filterResult == EXCEPTION_CONTINUE_EXECUTION) {
					return ExceptionContinueExecution;
				}

				// Trigger a second pass through the stack frames, unwinding all scopes
				_global_unwind2(pRegistrationFrame);

				const DWORD scopeEbp = (DWORD)&pRegistrationFrame->_ebp;
				const DWORD newTrylevel = scopeTable[currentTrylevel].EnclosingLevel;
				const DWORD handlerFunclet = (DWORD)scopeTable[currentTrylevel].HandlerFunction;

				_local_unwind2(pRegistrationFrame, currentTrylevel);
				pRegistrationFrame->TryLevel = newTrylevel;

				ASM_BEGIN
					ASM(mov eax, handlerFunclet);
					ASM(mov ebp, scopeEbp);
					ASM(jmp eax); // won't return
				ASM_END
			}
		}

		currentTrylevel = pRegistrationFrame->ScopeTable[currentTrylevel].EnclosingLevel;
	}

	// No filter in this frame accepted the exception, continue searching
	return ExceptionContinueSearch;
}
