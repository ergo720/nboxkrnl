/*
 * ergo720                Copyright (c) 2024
 */

#pragma once

#include "..\..\io\io.hpp"


struct XISO_VOLUME_EXTENSION {
	PDEVICE_OBJECT TargetDeviceObject;
	ULONG SectorSize;
	ULONG PartitionSectorCount;
	ULARGE_INTEGER PartitionLength;
};
using PXISO_VOLUME_EXTENSION = XISO_VOLUME_EXTENSION *;

NTSTATUS XisoCreateVolume(PDEVICE_OBJECT DeviceObject);
