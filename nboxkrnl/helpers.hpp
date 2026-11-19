/*
* SPDX-License-Identifier: GPL-2.0+
*
* ergo720                Copyright (c) 2025
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

#pragma once


// clang-format off
static inline VOID outl(USHORT Port, ULONG Value)
{
	__asm {
		mov eax, Value
		mov dx, Port
		out dx, eax
	}
}

static inline VOID outw(USHORT Port, USHORT Value)
{
	__asm {
		mov ax, Value
		mov dx, Port
		out dx, ax
	}
}

static inline VOID outb(USHORT Port, BYTE Value)
{
	__asm {
		mov al, Value
		mov dx, Port
		out dx, al
	}
}

static inline ULONG inl(USHORT Port)
{
	__asm {
		mov dx, Port
		in eax, dx
	}
}

static inline USHORT inw(USHORT Port)
{
	__asm {
		mov dx, Port
		in ax, dx
	}
}

static inline BYTE inb(USHORT Port)
{
	__asm {
		mov dx, Port
		in al, dx
	}
}

static inline VOID enable()
{
	__asm sti
}

static inline VOID disable()
{
	__asm cli
}

static inline DWORD save_int_state_and_disable()
{
	DWORD Eflags;

	__asm {
		pushfd
		cli
		pop Eflags
	}

	return Eflags;
}

static inline VOID restore_int_state(DWORD Eflags)
{
	__asm {
		push Eflags
		popfd
	}
}

static inline UCHAR bit_scan_forward(ULONG *Index, ULONG Mask)
{
	if (Mask == 0) {
		return 0;
	}

	ULONG Value;
	__asm {
		bsf eax, Mask
		mov Value, eax
	}
	*Index = Value;

	return 1;
}

static inline UCHAR bit_scan_reverse(ULONG *Index, ULONG Mask)
{
	if (Mask == 0) {
		return 0;
	}

	ULONG Value;
	__asm {
		bsr eax, Mask
		mov Value, eax
	}
	*Index = Value;

	return 1;
}

static inline VOID CDECL atomic_store64(LONGLONG *dst, LONGLONG val)
{
	DWORD OldEflags = save_int_state_and_disable();

	*dst = val;

	restore_int_state(OldEflags);
}

static inline VOID CDECL atomic_add64(LONGLONG *dst, LONGLONG val)
{
	DWORD OldEflags = save_int_state_and_disable();

	LONGLONG temp = *dst;
	temp += val;
	*dst = temp;

	restore_int_state(OldEflags);
}
// clang-format on
