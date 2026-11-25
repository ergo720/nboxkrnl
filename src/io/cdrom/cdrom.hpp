/*
* SPDX-License-Identifier: GPL-2.0+
*
* ergo720                Copyright (c) 2024
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

#define CDROM_ALIGNMENT_REQUIREMENT 1
#define CDROM_SECTOR_SIZE 2048
#define CDROM_TOTAL_NUM_OF_SECTORS 3820880 // assuming DVD-9 (dual layer), redump reports a total size of 7825162240 bytes


// Source: Cxbx-Reloaded
struct SCSI_PASS_THROUGH_DIRECT {
	USHORT Length;
	UCHAR ScsiStatus;
	UCHAR PathId;
	UCHAR TargetId;
	UCHAR Lun;
	UCHAR CdbLength;
	UCHAR SenseInfoLength;
	UCHAR DataIn;
	ULONG DataTransferLength;
	ULONG TimeOutValue;
	PVOID DataBuffer;
	ULONG SenseInfoOffset;
	UCHAR Cdb[16];
};
using PSCSI_PASS_THROUGH_DIRECT = SCSI_PASS_THROUGH_DIRECT *;

struct DVDX2_AUTHENTICATION_PAGE {
	UCHAR Unknown[2];
	UCHAR PartitionArea;
	UCHAR CDFValid;
	UCHAR Authentication;
	UCHAR Unknown2[3];
	ULONG Unknown3[3];
};
using PDVDX2_AUTHENTICATION_PAGE = DVDX2_AUTHENTICATION_PAGE *;

struct MODE_PARAMETER_HEADER10 {
	UCHAR ModeDataLength[2];
	UCHAR MediumType;
	UCHAR DeviceSpecificParameter;
	UCHAR Reserved[2];
	UCHAR BlockDescriptorLength[2];
};
using PMODE_PARAMETER_HEADER10 = MODE_PARAMETER_HEADER10 *;

struct DVDX2_AUTHENTICATION {
	MODE_PARAMETER_HEADER10 Header;
	DVDX2_AUTHENTICATION_PAGE AuthenticationPage;
};
using PDVDX2_AUTHENTICATION = DVDX2_AUTHENTICATION *;

BOOLEAN CdromInitDriver();
