/*
 * ergo720                Copyright (c) 2022
 */

#include "ke.hpp"
#include "..\hal\halp.hpp"
#include "..\dbg\dbg.hpp"


EXPORTNUM(95)
VOID XBOXAPI KeBugCheck(
	ULONG BugCheckCode)
{
	KeBugCheckEx(BugCheckCode, 0, 0, 0, 0);
}

EXPORTNUM(96)
VOID XBOXAPI KeBugCheckEx(
	ULONG     BugCheckCode,
	ULONG_PTR BugCheckParameter1,
	ULONG_PTR BugCheckParameter2,
	ULONG_PTR BugCheckParameter3,
	ULONG_PTR BugCheckParameter4)
{
	DbgPrint("Fatal error of the kernel with code: 0x%08lx\n(0x%p, 0x%p, 0x%p, 0x%p)\n\n",
	         BugCheckCode,
	         BugCheckParameter1,
	         BugCheckParameter2,
	         BugCheckParameter3,
	         BugCheckParameter4);

	HalpShutdownSystem();
}
