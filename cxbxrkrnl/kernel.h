/*
 * ergo720                Copyright (c) 2022
 * cxbxr devs
 */

#pragma once

#include "types.h"


void inline InitializeListHead(PLIST_ENTRY pListHead)
{
	pListHead->Flink = pListHead->Blink = pListHead;
}
