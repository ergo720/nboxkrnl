/*
 * ergo720                Copyright (c) 2022
 */

#include "ki.hpp"
#include "..\kernel.hpp"
#include "..\rtl\rtl.hpp"


KPCR KiPcr = { 0 };
KPROCESS KiUniqueProcess = { 0 };
KPROCESS KiIdleProcess = { 0 };


VOID KiInitSystem()
{
	InitializeListHead(&KiWaitInListHead);

	KeInitializeDpc(&KiTimerExpireDpc, KiTimerExpiration, nullptr);
	for (unsigned i = 0; i < TIMER_TABLE_SIZE; ++i) {
		InitializeListHead(&KiTimerTableListHead[i].Entry);
		KiTimerTableListHead[i].Time.u.HighPart = 0xFFFFFFFF;
		KiTimerTableListHead[i].Time.u.LowPart = 0;
	}

	for (unsigned i = 0; i < NUM_OF_THREAD_PRIORITIES; ++i) {
		InitializeListHead(&KiReadyThreadLists[i]);
	}
}

VOID KiInitializeProcess(PKPROCESS Process, KPRIORITY BasePriority, LONG ThreadQuantum)
{
	InitializeListHead(&Process->ReadyListHead);
	InitializeListHead(&Process->ThreadListHead);
	Process->BasePriority = BasePriority;
	Process->ThreadQuantum = ThreadQuantum;
}

VOID KiIdleLoopThread()
{
	RIP_UNIMPLEMENTED();
}
