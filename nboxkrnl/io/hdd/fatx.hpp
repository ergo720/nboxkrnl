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

#include "io.hpp"
#include "ex.hpp"

#define FATX_VOLUME_DISMOUNTED 2
#define FATX_MAX_FILE_NAME_LENGTH 42


struct FATX_FILE_INFO {
	UCHAR FileNameLength;
	CHAR FileName[FATX_MAX_FILE_NAME_LENGTH];
	UCHAR FileAttributes;
	ULONG FileSize;
	ULONG HostHandle;
	SHARE_ACCESS ShareAccess;
	ULONG Flags;
	ULONG CreationTime;
	ULONG LastAccessTime;
	ULONG LastWriteTime;
	ULONG RefCounter;
	LIST_ENTRY ListEntry;
};
using PFATX_FILE_INFO = FATX_FILE_INFO *;

struct FSCACHE_EXTENSION {
	PDEVICE_OBJECT TargetDeviceObject;
	LARGE_INTEGER PartitionLength;
	ULONG SectorSize;
	ULONG DeviceType;
	ULONG HostHandle;
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
