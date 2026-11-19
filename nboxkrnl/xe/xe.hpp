/*
* SPDX-License-Identifier: GPL-2.0+
*
* ergo720                Copyright (c) 2023
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

#include "xbe.hpp"
#include "ob.hpp"


struct LAUNCH_DATA_HEADER {
	DWORD dwLaunchDataType;
	DWORD dwTitleId;
	CHAR szLaunchPath[520];
	DWORD dwFlags;
};
using PLAUNCH_DATA_HEADER = LAUNCH_DATA_HEADER *;

struct LAUNCH_DATA_PAGE {
	LAUNCH_DATA_HEADER Header;
	UCHAR Pad[492];
	UCHAR LaunchData[3072];
};
using PLAUNCH_DATA_PAGE = LAUNCH_DATA_PAGE *;

#ifdef __cplusplus
extern "C" {
#endif

extern EXPORTNUM(164) DLLEXPORT PLAUNCH_DATA_PAGE LaunchDataPage;

extern EXPORTNUM(326) DLLEXPORT OBJECT_STRING XeImageFileName;

EXPORTNUM(327) DLLEXPORT NTSTATUS XBOXAPI XeLoadSection
(
	PXBE_SECTION Section
);

EXPORTNUM(328) DLLEXPORT NTSTATUS XBOXAPI XeUnloadSection
(
	PXBE_SECTION Section
);

#ifdef __cplusplus
}
#endif

VOID XBOXAPI XbeStartupThread(PVOID Opaque);
