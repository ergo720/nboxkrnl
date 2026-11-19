/*
* SPDX-License-Identifier: GPL-2.0+
*
* ergo720                Copyright (c) 2023
* LukeUsher              Copyright (c) 2018
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

#include "ke.hpp"
#include "rtl.hpp"


EXPORTNUM(108) VOID XBOXAPI KeInitializeEvent
(
	PKEVENT Event,
	EVENT_TYPE Type,
	BOOLEAN SignalState
)
{
	Event->Header.Type = Type;
	Event->Header.Size = sizeof(KEVENT) / sizeof(LONG);
	Event->Header.SignalState = SignalState;
	InitializeListHead(&(Event->Header.WaitListHead));
}

// Source: Cxbx-Reloaded
EXPORTNUM(145) LONG XBOXAPI KeSetEvent
(
	PKEVENT Event,
	KPRIORITY Increment,
	BOOLEAN	Wait
)
{
	KIRQL OldIrql = KeRaiseIrqlToDpcLevel();
	LONG OldState = Event->Header.SignalState;

	if (IsListEmpty(&Event->Header.WaitListHead)) {
		Event->Header.SignalState = 1;
	}
	else {
		PKWAIT_BLOCK WaitBlock = CONTAINING_RECORD(Event->Header.WaitListHead.Flink, KWAIT_BLOCK, WaitListEntry);
		if ((Event->Header.Type == NotificationEvent) || (WaitBlock->WaitType == WaitAll)) {
			if (OldState == 0) {
				Event->Header.SignalState = 1;
				KiWaitTest(Event, Increment);
			}
		}
		else {
			KiUnwaitThread(WaitBlock->Thread, WaitBlock->WaitKey, Increment);
		}
	}

	if (Wait) {
		PKTHREAD Thread = KeGetCurrentThread();
		Thread->WaitNext = Wait;
		Thread->WaitIrql = OldIrql;
	}
	else {
		KiUnlockDispatcherDatabase(OldIrql);
	}

	return OldState;
}
