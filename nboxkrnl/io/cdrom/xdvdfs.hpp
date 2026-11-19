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

#include "io.hpp"
#include "rtl.hpp"
#include "ex.hpp"


struct XISO_FILE_INFO {
	USHORT FileNameLength;
	PCHAR FileName;
	ULONG FileSize;
	ULONG HostHandle;
	ULONG Flags;
	ULONG RefCounter;
	LARGE_INTEGER Timestamp;
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

NTSTATUS XdvdfsCreateVolume(PDEVICE_OBJECT DeviceObject);
