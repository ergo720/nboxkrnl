/*
* SPDX-License-Identifier: GPL-2.0+
*
* ergo720                Copyright (c) 2022
*
* This program is free software; you can redistribute it
* and/or modify it under the terms of the GNU General Public
* License as published by the Free Software Foundation; either
* version 2 of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty
* of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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

	// clang-format off
	__asm {
		mov eax, dword ptr [esp]
		mov ecx, dword ptr [esp + 4]
		push 0
		push 0
		push 0
		push eax
		push ecx
		call KeBugCheckEx // won't return
	}
	// clang-format on
}
