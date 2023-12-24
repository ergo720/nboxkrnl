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
	UCHAR Unknown1[27];
	UCHAR Flags;
	UCHAR Unknown2[24];
	ERWLOCK VolumeMutex;
	UCHAR Unknown3[108];
};
using PFAT_VOLUME_EXTENSION = FAT_VOLUME_EXTENSION *;


NTSTATUS FatxCreateVolume(PDEVICE_OBJECT DeviceObject);
