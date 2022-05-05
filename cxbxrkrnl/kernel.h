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

void inline InsertTailList(PLIST_ENTRY pListHead, PLIST_ENTRY pEntry)
{
	PLIST_ENTRY _EX_ListHead = pListHead;
	PLIST_ENTRY _EX_Blink = _EX_ListHead->Blink;

	pEntry->Flink = _EX_ListHead;
	pEntry->Blink = _EX_Blink;
	_EX_Blink->Flink = pEntry;
	_EX_ListHead->Blink = pEntry;
}
