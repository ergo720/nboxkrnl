/*
* SPDX-License-Identifier: GPL-2.0+
*
* ergo720                Copyright (c) 2023
* PatrickvL              Copyright (c) 2017
* Fisherman166           Copyright (c) 2020
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

#include "ex.hpp"


// Source: Cxbx-Reloaded
EXPORTNUM(12) VOID XBOXAPI ExAcquireReadWriteLockExclusive
(
	PERWLOCK ReadWriteLock
)
{
	disable();
	if (InterlockedIncrement(reinterpret_cast<LONG *>(&ReadWriteLock->LockCount))) {
		ReadWriteLock->WritersWaitingCount++;
		enable();
		KeWaitForSingleObject(&ReadWriteLock->WriterEvent, Executive, KernelMode, FALSE, nullptr);
	}
	else {
		enable();
	}
}

// Source: Cxbx-Reloaded
EXPORTNUM(13) VOID XBOXAPI ExAcquireReadWriteLockShared
(
	PERWLOCK ReadWriteLock
)
{
	disable();
	bool MustWaitOnActiveWrite = ReadWriteLock->ReadersEntryCount == 0;
	bool MustWaitOnAQueuedWrite = (ReadWriteLock->ReadersEntryCount != 0) && (ReadWriteLock->WritersWaitingCount != 0);
	bool MustWait = MustWaitOnActiveWrite || MustWaitOnAQueuedWrite;
	if (InterlockedIncrement(reinterpret_cast<LONG *>(&ReadWriteLock->LockCount)) && MustWait) {
		ReadWriteLock->ReadersWaitingCount++;
		enable();
		KeWaitForSingleObject(&ReadWriteLock->ReaderSemaphore, Executive, KernelMode, FALSE, nullptr);
	}
	else {
		ReadWriteLock->ReadersEntryCount++;
		enable();
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
	disable();
	if (InterlockedDecrement(reinterpret_cast<LONG *>(&ReadWriteLock->LockCount)) == -1) {
		ReadWriteLock->ReadersEntryCount = 0;
		enable();
		return;
	}

	if (ReadWriteLock->ReadersEntryCount == 0) {
		if (ReadWriteLock->ReadersWaitingCount != 0) {
			ULONG OriReadersWaiting = ReadWriteLock->ReadersWaitingCount;
			ReadWriteLock->ReadersEntryCount = ReadWriteLock->ReadersWaitingCount;
			ReadWriteLock->ReadersWaitingCount = 0;
			enable();
			KeReleaseSemaphore(&ReadWriteLock->ReaderSemaphore, PRIORITY_BOOST_SEMAPHORE, OriReadersWaiting, FALSE);
			return;
		}
	}
	else {
		ReadWriteLock->ReadersEntryCount--;
		if (ReadWriteLock->ReadersEntryCount != 0) {
			enable();
			return;
		}
	}

	ReadWriteLock->WritersWaitingCount--;
	enable();
	KeSetEvent(&ReadWriteLock->WriterEvent, PRIORITY_BOOST_EVENT, FALSE);
}
