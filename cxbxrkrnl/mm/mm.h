/*
 * ergo720                Copyright (c) 2022
 * cxbxr devs
 */

#pragma once

#include "..\types.h"

#define ALIGN_DOWN(length, type)     ((ULONG)(length) & ~(sizeof(type) - 1))
#define ALIGN_UP(length, type)       (((ULONG)(length) + sizeof(type) - 1) & ~(sizeof(type) - 1))
