/*
 * ergo720                Copyright (c) 2023
 */

#pragma once

#include "io.hpp"
#include "ex.hpp"

#define FATX_VOLUME_DISMOUNTED 2
#define FATX_MAX_FILE_NAME_LENGTH 42


struct FATX_FILE_INFO {
	UCHAR FileNameLength;
	CHAR FileName[FATX_MAX_FILE_NAME_LENGTH];
	ULONG FileSize;
	ULONGLONG HostHandle;
	SHARE_ACCESS ShareAccess;
	ULONG Flags;
	LARGE_INTEGER CreationTime;
	LARGE_INTEGER LastAccessTime;
	LARGE_INTEGER LastWriteTime;
	ULONG RefCounter;
	LIST_ENTRY ListEntry;
};
using PFATX_FILE_INFO = FATX_FILE_INFO *;

struct FSCACHE_EXTENSION {
	PDEVICE_OBJECT TargetDeviceObject;
	LARGE_INTEGER PartitionLength;
	ULONG SectorSize;
	ULONG DeviceType;
	ULONGLONG HostHandle;
};
using PFSCACHE_EXTENSION = FSCACHE_EXTENSION *;

struct FAT_VOLUME_EXTENSION {
	FSCACHE_EXTENSION CacheExtension;
	FATX_FILE_INFO VolumeInfo;
	ULONG NumberOfClusters;
	ULONG BytesPerCluster;
	UCHAR SectorShift;
	UCHAR ClusterShift;
	UCHAR Flags;
	ULONG NumberOfClustersAvailable;
	ERWLOCK VolumeMutex;
	ULONG VolumeID;
	ULONG FileObjectCount;
	LIST_ENTRY OpenFileList;
};
using PFAT_VOLUME_EXTENSION = FAT_VOLUME_EXTENSION *;


NTSTATUS FatxCreateVolume(PDEVICE_OBJECT DeviceObject);
