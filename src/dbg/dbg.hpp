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

#pragma once

#include "..\types.hpp"

#ifdef __cplusplus
extern "C" {
#endif

EXPORTNUM(5) DLLEXPORT VOID XBOXAPI DbgBreakPoint();

EXPORTNUM(8) DLLEXPORT ULONG CDECL DbgPrint
(
	const CHAR *Format,
	...
);

#ifdef __cplusplus
}
#endif
