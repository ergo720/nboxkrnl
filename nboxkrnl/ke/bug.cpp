/*
 * ergo720                Copyright (c) 2022
 */

#include "ke.hpp"
#include "halp.hpp"
#include "dbg.hpp"


EXPORTNUM(95) VOID XBOXAPI KeBugCheck
(
	ULONG BugCheckCode
)
{
	KeBugCheckEx(BugCheckCode, 0, 0, 0, 0);
}

EXPORTNUM(96) VOID XBOXAPI KeBugCheckEx
(
	ULONG BugCheckCode,
	ULONG_PTR BugCheckParameter1,
	ULONG_PTR BugCheckParameter2,
	ULONG_PTR BugCheckParameter3,
	ULONG_PTR BugCheckParameter4
)
{
  // This routine terminates the system. It should be used when a failure is not expected, for example those caused by bugs

	DbgPrint("Fatal error of the kernel with code: 0x%08lx (0x%08lx, 0x%08lx, 0x%08lx, 0x%08lx)",
		BugCheckCode,
		BugCheckParameter1,
		BugCheckParameter2,
		BugCheckParameter3,
		BugCheckParameter4);

	HalpShutdownSystem();
}

VOID __declspec(naked, noinline) CDECL KeBugCheckLogEip(ULONG BugCheckCode)
{
	// This function must be naked and never inlined because it needs to capture the return address placed on the stack by the caller

	ASM_BEGIN
		ASM(mov eax, dword ptr [esp]);
		ASM(mov ecx, dword ptr [esp + 4]);
		ASM(push 0);
		ASM(push 0);
		ASM(push 0);
		ASM(push eax);
		ASM(push ecx);
		ASM(call KeBugCheckEx); // won't return
	ASM_END
}
