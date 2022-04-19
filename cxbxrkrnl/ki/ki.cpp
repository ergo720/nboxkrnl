/*
 * ergo720                Copyright (c) 2022
 * cxbxr devs
 */

#include "ki.h"
#include "..\kernel.h"


KPCR KiPcr;


void KiInitSystem()
{
	InitializeListHead(&KiWaitInListHead);

	KeInitializeDpc(&KiTimerExpireDpc, KiTimerExpiration, nullptr);
	for (unsigned i = 0; i < TIMER_TABLE_SIZE; ++i) {
		InitializeListHead(&KiTimerTableListHead[i].Entry);
		KiTimerTableListHead[i].Time.u.HighPart = 0xFFFFFFFF;
		KiTimerTableListHead[i].Time.u.LowPart = 0;
	}
}
