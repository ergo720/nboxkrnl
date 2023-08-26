/*
 * ergo720                Copyright (c) 2023
 * Stefan Schmidt         Copyright (c) 2020-2023
 * Lucas Jansson          Copyright (c) 2021
 */

#include "seh.h"

#define DISPOSITION_DISMISS           0
#define DISPOSITION_CONTINUE_SEARCH   1
#define DISPOSITION_NESTED_EXCEPTION  2
#define DISPOSITION_COLLIDED_UNWIND   3
#define TRYLEVEL_NONE                -1

#define EXCEPTION_EXECUTE_HANDLER     1
#define EXCEPTION_CONTINUE_SEARCH     0
#define EXCEPTION_CONTINUE_EXECUTION  (-1)

enum EXCEPTION_DISPOSITION {
    ExceptionContinueExecution = 0,
    ExceptionContinueSearch = 1,
    ExceptionNestedException = 2,
    ExceptionCollidedUnwind = 3,
};


extern "C" int _nested_unwind_handler(EXCEPTION_RECORD *pExceptionRecord, EXCEPTION_REGISTRATION_SEH *pRegistrationFrame,
	CONTEXT *pContextRecord, EXCEPTION_REGISTRATION_RECORD **pDispatcherContext)
{
	if (!(pExceptionRecord->ExceptionFlags & (EXCEPTION_UNWINDING | EXCEPTION_EXIT_UNWIND))) {
		return DISPOSITION_CONTINUE_SEARCH;
	}

	*pDispatcherContext = pRegistrationFrame;
	return DISPOSITION_COLLIDED_UNWIND;
}

static inline void FASTCALL call_ebp_func(void *func, void *_ebp)
{
	__asm {
		push ebp
		mov ebp, edx
		call ecx
		pop ebp
	}
}

extern "C" void _local_unwind2(EXCEPTION_REGISTRATION_SEH *pRegistrationFrame, int stop)
{
	// Manually install exception handler frame
	EXCEPTION_REGISTRATION_RECORD nestedUnwindFrame;
	nestedUnwindFrame.Handler = reinterpret_cast<void *>(_nested_unwind_handler);

	__asm {
		mov eax, DWORD PTR fs:[0]
		mov DWORD PTR [nestedUnwindFrame].Prev, eax
		lea eax, nestedUnwindFrame
		mov DWORD PTR fs:[0], eax
	}

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
	__asm {
		mov eax, DWORD PTR [nestedUnwindFrame].Prev
		mov DWORD PTR fs:[0], eax
	}
}

extern "C" void _global_unwind2(EXCEPTION_REGISTRATION_SEH *pRegistrationFrame)
{
	// TODO
}

extern "C" int _except_handler3(EXCEPTION_RECORD *pExceptionRecord, EXCEPTION_REGISTRATION_SEH *pRegistrationFrame,
	CONTEXT *pContextRecord, EXCEPTION_REGISTRATION_RECORD **pDispatcherContext)
{
	// Clear the direction flag - the function triggering the exception might
	// have modified it, but it's expected to not be set
	__asm cld

	if (pExceptionRecord->ExceptionFlags & (EXCEPTION_UNWINDING | EXCEPTION_EXIT_UNWIND)) {
		// We're in an unwinding pass, so unwind all local scopes
		_local_unwind2(pRegistrationFrame, TRYLEVEL_NONE);
		return DISPOSITION_CONTINUE_SEARCH;
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

			__asm {
				push ebp
				mov ebp, _ebp
				call filterFunclet
				pop ebp
				mov filterResult, eax
			}

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

				// XXX: not sure if the following should use a CALL instead of a JMP, it depends on how __except will be implemented
				__asm {
					mov ebp, scopeEbp
					jmp handlerFunclet // won't return
				}
			}
		}

		currentTrylevel = pRegistrationFrame->ScopeTable[currentTrylevel].EnclosingLevel;
	}

	// No filter in this frame accepted the exception, continue searching
	return DISPOSITION_CONTINUE_SEARCH;
}

// Check https://reactos-blog.blogspot.com/2009/08/inside-mind-of-reactos-developer.html for information about __SEH_prolog / __SEH_epilog

__declspec(naked) VOID __SEH_prolog()
{
	__asm {
		push offset _except_handler3
		mov eax, DWORD PTR fs:[0]
		push eax
		mov eax, [esp + 16]
		mov DWORD PTR [esp + 16], ebp
		lea ebp, [esp + 16]
		sub esp, eax
		push ebx
		push esi
		push edi
		mov DWORD PTR [ebp - 24], esp
		mov eax, DWORD PTR [ebp - 8]
		push eax
		mov eax, [ebp - 4]
		mov DWORD PTR [ebp - 8], eax
		mov DWORD PTR [ebp - 4], TRYLEVEL_NONE
		lea eax, DWORD PTR [ebp - 16]
		mov DWORD PTR fs:[0], eax
		ret
	}
}

__declspec(naked) VOID __SEH_epilog()
{
	__asm {
		mov ecx, DWORD PTR [ebp - 16]
		mov DWORD PTR fs:[0], ecx
		pop ecx
		pop edi
		pop esi
		pop ebx
		mov esp, ebp
		pop ebp
		push ecx
		ret
	}
}