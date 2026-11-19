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

#include "..\..\types.hpp"
#include "iop.hpp"

#define PE_PARTFLAGS_IN_USE	0x80000000

// NOTE: the sizes below are in sector units, one sector == 512 bytes
#define XBOX_SWAPPART_LBA_SIZE      0x177000
#define XBOX_SWAPPART1_LBA_START    0x400
#define XBOX_SWAPPART2_LBA_START    (XBOX_SWAPPART1_LBA_START + XBOX_SWAPPART_LBA_SIZE)
#define XBOX_SWAPPART3_LBA_START    (XBOX_SWAPPART2_LBA_START + XBOX_SWAPPART_LBA_SIZE)
#define XBOX_SYSPART_LBA_START      (XBOX_SWAPPART3_LBA_START + XBOX_SWAPPART_LBA_SIZE)
#define XBOX_SYSPART_LBA_SIZE       0xfa000
#define XBOX_MUSICPART_LBA_START    (XBOX_SYSPART_LBA_START + XBOX_SYSPART_LBA_SIZE)
#define XBOX_MUSICPART_LBA_SIZE     0x9896b0
#define XBOX_CONFIG_AREA_LBA_START  0
#define XBOX_CONFIG_AREA_LBA_SIZE   XBOX_SWAPPART1_LBA_START

#define XBOX_MAX_NUM_OF_PARTITIONS 14 // accounts for non-standard partitions

#define HDD_ALIGNMENT_REQUIREMENT 1
#define HDD_SECTOR_SIZE (ULONGLONG)512
#define HDD_MAXIMUM_TRANSFER_BYTES (HDD_SECTOR_SIZE * 256)


#pragma pack(1)
struct XBOX_PARTITION_TABLE {
	UCHAR Magic[16];
	CHAR Res0[32];
	struct {
		UCHAR Name[16];
		ULONG Flags;
		ULONG LBAStart;
		ULONG LBASize;
		ULONG Reserved;
	} TableEntries[14];
};
#pragma pack()


// Setup during initialization by HddInitDriver(), then never changed again
inline ULONGLONG HddTotalByteSize = 0;
inline ULONGLONG HddTotalSectorCount = 0;

BOOLEAN HddInitDriver();
