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

#include "..\kernel.hpp"
#include "dbg.hpp"
#include "rtl.hpp"
#include <nanoprintf.h>
#include <string.h>
#include <stdarg.h>


EXPORTNUM(5) VOID XBOXAPI DbgBreakPoint()
{
	RIP_UNIMPLEMENTED();
}

EXPORTNUM(8) ULONG CDECL DbgPrint
(
	const CHAR *Format,
	...
)
{
	if (Format != nullptr) {
		// We print at most 512 characters long strings
		char buff[512];
		va_list vlist;
		va_start(vlist, Format);
		npf_vsnprintf(buff, sizeof(buff), Format, vlist);
		va_end(vlist);

		outl(DBG_OUTPUT_STR_PORT, reinterpret_cast<ULONG>(buff));
	}

	return STATUS_SUCCESS;
}
