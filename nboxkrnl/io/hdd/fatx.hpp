/*
 * ergo720                Copyright (c) 2023
 */

#pragma once

#include "..\..\io\io.hpp"
#include "..\..\ex\ex.hpp"

#define FATX_VOLUME_DISMOUNTED 2


struct FAT_VOLUME_EXTENSION {
	UCHAR Unknown1[43];
	UCHAR Flags;
	UCHAR Unknown2[108];
};
using PFAT_VOLUME_EXTENSION = FAT_VOLUME_EXTENSION *;


NTSTATUS MountFatxVolume(PDEVICE_OBJECT DeviceObject);
