/*
 * ergo720                Copyright (c) 2024
 */

#pragma once

#include "io.hpp"
#include "rtl.hpp"
#include "ex.hpp"


struct XISO_FILE_INFO {
	USHORT FileNameLength;
	PCHAR FileName;
	ULONG FileSize;
	ULONGLONG HostHandle;
	ULONG Flags;
	ULONG RefCounter;
	LIST_ENTRY ListEntry;
};
using PXISO_FILE_INFO = XISO_FILE_INFO *;

struct XISO_VOLUME_EXTENSION {
	XISO_FILE_INFO VolumeInfo;
	PDEVICE_OBJECT TargetDeviceObject;
	ULONG SectorSize;
	ULONG PartitionSectorCount;
	ULONG FileObjectCount;
	ULARGE_INTEGER PartitionLength;
	BOOLEAN Dismounted;
	ERWLOCK VolumeMutex;
	LIST_ENTRY OpenFileList;
};
using PXISO_VOLUME_EXTENSION = XISO_VOLUME_EXTENSION *;

NTSTATUS XisoCreateVolume(PDEVICE_OBJECT DeviceObject);
