/*
 * ergo720                Copyright (c) 2024
 */

#pragma once

#include "io.hpp"
#include "ex.hpp"


struct RAW_VOLUME_EXTENSION {
	PDEVICE_OBJECT TargetDeviceObject;
	BOOLEAN Dismounted;
	ERWLOCK VolumeMutex;
	SHARE_ACCESS ShareAccess;
};
using PRAW_VOLUME_EXTENSION = RAW_VOLUME_EXTENSION *;


NTSTATUS RawCreateVolume(PDEVICE_OBJECT DeviceObject);
