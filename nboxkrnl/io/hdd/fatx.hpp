/*
 * ergo720                Copyright (c) 2023
 */

#pragma once

#include "..\..\io\io.hpp"
#include "..\..\ex\ex.hpp"

#define FATX_VOLUME_DISMOUNTED 2
#define FATX_MAX_FILE_NAME_LENGTH 42


struct FSCACHE_EXTENSION {
	PDEVICE_OBJECT TargetDeviceObject;
	LARGE_INTEGER PartitionLength;
	ULONG SectorSize;
};
using PFSCACHE_EXTENSION = FSCACHE_EXTENSION *;

struct FAT_VOLUME_EXTENSION {
	FSCACHE_EXTENSION CacheExtension;
	UCHAR Unknown1[20];
	ULONG BytesPerCluster;
	UCHAR SectorShift;
	UCHAR ClusterShift;
	UCHAR Unknown2;
	UCHAR Flags;
	UCHAR Unknown3[24];
	ERWLOCK VolumeMutex;
	UCHAR Unknown4[96];
	ULONG VolumeID;
	UCHAR Unknown5[8];
};
using PFAT_VOLUME_EXTENSION = FAT_VOLUME_EXTENSION *;


NTSTATUS FatxCreateVolume(PDEVICE_OBJECT DeviceObject);
