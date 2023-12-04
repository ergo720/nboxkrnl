/*
 * ergo720                Copyright (c) 2023
 * PatrickvL              Copyright (c) 2017
 * Fisherman166           Copyright (c) 2020
 */

#include "..\ex\ex.hpp"


// Source: Cxbx-Reloaded
EXPORTNUM(12) VOID XBOXAPI ExAcquireReadWriteLockExclusive
(
	PERWLOCK ReadWriteLock
)
{
	__asm cli
	if (InterlockedIncrement(reinterpret_cast<LONG *>(&ReadWriteLock->LockCount))) {
		ReadWriteLock->WritersWaitingCount++;
		__asm sti
		KeWaitForSingleObject(&ReadWriteLock->WriterEvent, Executive, KernelMode, FALSE, nullptr);
	}
	else {
		__asm sti
	}
}

// Source: Cxbx-Reloaded
EXPORTNUM(13) VOID XBOXAPI ExAcquireReadWriteLockShared
(
	PERWLOCK ReadWriteLock
)
{
	__asm cli
	bool MustWaitOnActiveWrite = ReadWriteLock->ReadersEntryCount == 0;
	bool MustWaitOnAQueuedWrite = (ReadWriteLock->ReadersEntryCount != 0) && (ReadWriteLock->WritersWaitingCount != 0);
	bool MustWait = MustWaitOnActiveWrite || MustWaitOnAQueuedWrite;
	if (InterlockedIncrement(reinterpret_cast<LONG *>(&ReadWriteLock->LockCount)) && MustWait) {
		ReadWriteLock->ReadersWaitingCount++;
		__asm sti
		KeWaitForSingleObject(&ReadWriteLock->ReaderSemaphore, Executive, KernelMode, FALSE, nullptr);
	}
	else {
		ReadWriteLock->ReadersEntryCount++;
		__asm sti
	}
}

// Source: Cxbx-Reloaded
EXPORTNUM(18) VOID XBOXAPI ExInitializeReadWriteLock
(
	PERWLOCK ReadWriteLock
)
{
	ReadWriteLock->LockCount = -1;
	ReadWriteLock->WritersWaitingCount = 0;
	ReadWriteLock->ReadersWaitingCount = 0;
	ReadWriteLock->ReadersEntryCount = 0;
	KeInitializeEvent(&ReadWriteLock->WriterEvent, SynchronizationEvent, FALSE);
	KeInitializeSemaphore(&ReadWriteLock->ReaderSemaphore, 0, MAXLONG);
}

// Source: Cxbx-Reloaded
EXPORTNUM(28) VOID XBOXAPI ExReleaseReadWriteLock
(
	PERWLOCK ReadWriteLock
)
{
	__asm cli
	if (InterlockedDecrement(reinterpret_cast<LONG *>(&ReadWriteLock->LockCount)) == -1) {
		ReadWriteLock->ReadersEntryCount = 0;
		__asm sti
		return;
	}

	if (ReadWriteLock->ReadersEntryCount == 0) {
		if (ReadWriteLock->ReadersWaitingCount != 0) {
			ULONG OriReadersWaiting = ReadWriteLock->ReadersWaitingCount;
			ReadWriteLock->ReadersEntryCount = ReadWriteLock->ReadersWaitingCount;
			ReadWriteLock->ReadersWaitingCount = 0;
			__asm sti
			KeReleaseSemaphore(&ReadWriteLock->ReaderSemaphore, PRIORITY_BOOST_SEMAPHORE, OriReadersWaiting, FALSE);
			return;
		}
	}
	else {
		ReadWriteLock->ReadersEntryCount--;
		if (ReadWriteLock->ReadersEntryCount != 0) {
			__asm sti
			return;
		}
	}

	ReadWriteLock->WritersWaitingCount--;
	__asm sti
	KeSetEvent(&ReadWriteLock->WriterEvent, PRIORITY_BOOST_EVENT, FALSE);
}
