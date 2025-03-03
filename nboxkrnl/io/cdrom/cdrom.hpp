/*
 * ergo720                Copyright (c) 2024
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
