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
#include "ex.hpp"


struct RAW_VOLUME_EXTENSION {
	PDEVICE_OBJECT TargetDeviceObject;
	BOOLEAN Dismounted;
	ERWLOCK VolumeMutex;
	SHARE_ACCESS ShareAccess;
};
using PRAW_VOLUME_EXTENSION = RAW_VOLUME_EXTENSION *;


NTSTATUS RawCreateVolume(PDEVICE_OBJECT DeviceObject);
