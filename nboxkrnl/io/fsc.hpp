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

#include "mi.hpp"
#include "hdd\fatx.hpp"


struct FSCACHE_ELEMENT {
	ULONG AlignedByteOffset;
	PFSCACHE_EXTENSION CacheExtension;
	union {
		struct {
			ULONG NumOfUsers : 8;
			ULONG MarkForDeletion : 1;
			ULONG WriteInProgress : 1;
			ULONG Unused : 2;
			ULONG CachePageAlignedAddr : 20;
		};
		PCHAR CacheBuffer;
	};
	LIST_ENTRY ListEntry;
};
using PFSCACHE_ELEMENT = FSCACHE_ELEMENT *;


#ifdef __cplusplus
extern "C" {
#endif

EXPORTNUM(35) DLLEXPORT ULONG XBOXAPI FscGetCacheSize();

EXPORTNUM(37) DLLEXPORT NTSTATUS XBOXAPI FscSetCacheSize
(
	ULONG NumberOfCachePages
);

#ifdef __cplusplus
}
#endif


inline ULONG FscCurrNumberOfCachePages = 0;

NTSTATUS FscMapElementPage(PFSCACHE_EXTENSION CacheExtension, ULONGLONG ByteOffset, PVOID *ReturnedBuffer, BOOLEAN IsWrite);
VOID FscUnmapElementPage(PVOID Buffer);
VOID FscFlushPartitionElements(PFSCACHE_EXTENSION CacheExtension);
