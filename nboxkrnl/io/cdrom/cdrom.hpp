/*
 * ergo720                Copyright (c) 2024
 */

#pragma once

#include "..\..\types.hpp"

#define CDROM_ALIGNMENT_REQUIREMENT 1
#define CDROM_SECTOR_SIZE 2048
#define CDROM_TOTAL_NUM_OF_SECTORS 3820880 // assuming DVD-9 (dual layer), redump reports a total size of 7825162240 bytes


BOOLEAN CdromInitDriver();
