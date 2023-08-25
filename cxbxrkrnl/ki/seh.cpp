/*
 * ergo720                Copyright (c) 2023
 */

#include "seh.h"


extern "C" int _except_handler3(EXCEPTION_RECORD *pExceptionRecord, EXCEPTION_REGISTRATION_SEH *pRegistrationFrame, 
	CONTEXT *pContextRecord, EXCEPTION_REGISTRATION_RECORD **pDispatcherContext)
{
	// TODO

	return 0;
}

// Check https://reactos-blog.blogspot.com/2009/08/inside-mind-of-reactos-developer.html for information about __SEH_prolog

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
		mov DWORD PTR [ebp - 4], -1
		lea eax, DWORD PTR [ebp - 16]
		mov DWORD PTR fs:[0], eax
		ret
	}
}
