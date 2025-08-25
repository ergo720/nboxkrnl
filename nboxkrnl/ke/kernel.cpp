/*
 * ergo720                Copyright (c) 2022
 * PatrickvL              Copyright (c) 2016-2017
 */

#pragma once

#include "ki.hpp"
#include "ex.hpp"
#include "hal.hpp"
#include "..\kernel.hpp"
#include "..\kernel_version.hpp"
#include <assert.h>

#define XBOX_ACPI_FREQUENCY 3375000 // 3.375 MHz

static_assert(sizeof(IoRequest) == 44);
static_assert(sizeof(IoInfoBlockOc) == 36);


const char *NboxkrnlVersion = _NBOXKRNL_VERSION;

XBOX_KEY_DATA XboxCERTKey;

EXPORTNUM(120) volatile KSYSTEM_TIME KeInterruptTime = { 0, 0, 0 };

EXPORTNUM(154) volatile KSYSTEM_TIME KeSystemTime = { 0, 0, 0 };

EXPORTNUM(156) volatile DWORD KeTickCount = 0;

EXPORTNUM(157) ULONG KeTimeIncrement = CLOCK_TIME_INCREMENT;

EXPORTNUM(321) XBOX_KEY_DATA XboxEEPROMKey;

EXPORTNUM(322) XBOX_HARDWARE_INFO XboxHardwareInfo =
{
	0,     // Flags: 1=INTERNAL_USB, 2=DEVKIT, 4=MACROVISION, 8=CHIHIRO
	0xA2,  // GpuRevision, byte read from NV2A first register, at 0xFD0000000 - see NV_PMC_BOOT_0
	0xD3,  // McpRevision, Retail 1.6
	0,     // unknown
	0      // unknown
};

EXPORTNUM(324) XBOX_KRNL_VERSION XboxKrnlVersion =
{
	1,
	0,
	5838, // kernel build 5838
#if _DEBUG
	1 | (1 << 15) // =0x8000 -> debug kernel flag
#else
	1
#endif
};

// Source: Cxbx-Reloaded
EXPORTNUM(125) ULONGLONG XBOXAPI KeQueryInterruptTime()
{
	LARGE_INTEGER InterruptTime;

	while (true) {
		InterruptTime.u.HighPart = KeInterruptTime.HighTime;
		InterruptTime.u.LowPart = KeInterruptTime.LowTime;

		if (InterruptTime.u.HighPart == KeInterruptTime.High2Time) {
			break;
		}
	}

	return (ULONGLONG)InterruptTime.QuadPart;
}

EXPORTNUM(126) ULONGLONG XBOXAPI KeQueryPerformanceCounter()
{
	DWORD OldEflags = save_int_state_and_disable();
	ULONGLONG Value = inl(KE_ACPI_TIME_LOW);
	Value |= (ULONGLONG(inl(KE_ACPI_TIME_HIGH)) << 32);
	restore_int_state(OldEflags);

	return Value;
}

EXPORTNUM(127) ULONGLONG XBOXAPI KeQueryPerformanceFrequency()
{
	return XBOX_ACPI_FREQUENCY;
}

// Source: Cxbx-Reloaded
EXPORTNUM(128) VOID XBOXAPI KeQuerySystemTime
(
	PLARGE_INTEGER CurrentTime
)
{
	LARGE_INTEGER SystemTime;

	while (true) {
		SystemTime.u.HighPart = KeSystemTime.HighTime;
		SystemTime.u.LowPart = KeSystemTime.LowTime;

		if (SystemTime.u.HighPart == KeSystemTime.High2Time) {
			break;
		}
	}

	*CurrentTime = SystemTime;
}

EXPORTNUM(151) VOID XBOXAPI KeStallExecutionProcessor
(
	ULONG MicroSeconds
)
{
	DWORD LoopCounterValue = HalCounterPerMicroseconds * MicroSeconds;
	if (LoopCounterValue == 0) {
		return;
	}

	ASM_BEGIN
		ASM(mov ecx, LoopCounterValue);
loop_start:
		ASM(sub ecx, 1);
		ASM(jnz loop_start);
	ASM_END
}

// Source: Cxbx-Reloaded
VOID KeSetSystemTime(PLARGE_INTEGER NewTime, PLARGE_INTEGER OldTime)
{
	KIRQL OldIrql, OldIrql2;
	LARGE_INTEGER DeltaTime;
	PLIST_ENTRY ListHead, NextEntry;
	PKTIMER Timer;
	LIST_ENTRY TempList, TempList2;
	ULONG Hand, i;

	/* Sanity checks */
	assert((NewTime->u.HighPart & 0xF0000000) == 0);
	assert(KeGetCurrentIrql() <= DISPATCH_LEVEL);

	/* Lock the dispatcher, and raise IRQL */
	OldIrql = KeRaiseIrqlToDpcLevel();
	OldIrql2 = KfRaiseIrql(HIGH_LEVEL);

	/* Query the system time now */
	KeQuerySystemTime(OldTime);

	KeSystemTime.High2Time = NewTime->HighPart;
	KeSystemTime.LowTime = NewTime->LowPart;
	KeSystemTime.HighTime = NewTime->HighPart;

	/* Calculate the difference between the new and the old time */
	DeltaTime.QuadPart = NewTime->QuadPart - OldTime->QuadPart;

	KfLowerIrql(OldIrql2);

	/* Setup a temporary list of absolute timers */
	InitializeListHead(&TempList);

	/* Loop current timers */
	for (i = 0; i < TIMER_TABLE_SIZE; i++) {
		/* Loop the entries in this table and lock the timers */
		ListHead = &KiTimerTableListHead[i];
		NextEntry = ListHead->Flink;
		while (NextEntry != ListHead) {
			/* Get the timer */
			Timer = CONTAINING_RECORD(NextEntry, KTIMER, TimerListEntry);
			NextEntry = NextEntry->Flink;

			/* Is it absolute? */
			if (Timer->Header.Absolute) {
				/* Remove it from the timer list */
				KiRemoveTimer(Timer);

				/* Insert it into our temporary list */
				InsertTailList(&TempList, &Timer->TimerListEntry);
			}
		}
	}

	/* Setup a temporary list of expired timers */
	InitializeListHead(&TempList2);

	/* Loop absolute timers */
	while (TempList.Flink != &TempList) {
		/* Get the timer */
		Timer = CONTAINING_RECORD(TempList.Flink, KTIMER, TimerListEntry);
		RemoveEntryList(&Timer->TimerListEntry);

		/* Update the due time and handle */
		Timer->DueTime.QuadPart -= DeltaTime.QuadPart;
		Hand = KiComputeTimerTableIndex(Timer->DueTime.QuadPart);

		/* Lock the timer and re-insert it */
		if (KiReinsertTimer(Timer, Timer->DueTime)) {
			/* Remove it from the timer list */
			KiRemoveTimer(Timer);

			/* Insert it into our temporary list */
			InsertTailList(&TempList2, &Timer->TimerListEntry);
		}
	}

	/* Process expired timers. This releases the dispatcher and timer locks */
	KiTimerListExpire(&TempList2, OldIrql);
}

static NTSTATUS HostToNtStatus(IoStatus Status)
{
	switch (Status)
	{
	case IoStatus::Success:
		return STATUS_SUCCESS;

	case IoStatus::Pending:
		return STATUS_PENDING;

	case IoStatus::Error:
		return STATUS_IO_DEVICE_ERROR;

	case IoStatus::Failed:
		return STATUS_ACCESS_DENIED;

	case IoStatus::IsADirectory:
		return STATUS_FILE_IS_A_DIRECTORY;

	case IoStatus::NotADirectory:
		return STATUS_NOT_A_DIRECTORY;

	case IoStatus::NameNotFound:
		return STATUS_OBJECT_NAME_NOT_FOUND;

	case IoStatus::PathNotFound:
		return STATUS_OBJECT_PATH_NOT_FOUND;

	case IoStatus::Corrupt:
		return STATUS_FILE_CORRUPT_ERROR;

	case IoStatus::Full:
		return STATUS_DISK_FULL;

	case CannotDelete:
		return STATUS_CANNOT_DELETE;

	case NotEmpty:
		return STATUS_DIRECTORY_NOT_EMPTY;
	}

	KeBugCheckLogEip(UNREACHABLE_CODE_REACHED);
}

static VOID SubmitIoRequestToHost(void *RequestAddr)
{
	outl(IO_START, (ULONG_PTR)RequestAddr);
	while (inl(IO_CHECK_ENQUEUE)) {
		outl(IO_RETRY, 0);
	}
}

static VOID RetrieveIoRequestFromHost(volatile IoInfoBlockOc *Info, ULONG Id)
{
	// TODO: instead of polling the IO like this, the host should signal I/O completion by raising a HDD interrupt, so that we can handle the event in the ISR

	Info->Header.Id = Id;
	Info->Header.Ready = 0;

	do {
		outl(IO_QUERY, (ULONG_PTR)Info);
	} while (!Info->Header.Ready);

	Info->Header.NtStatus = HostToNtStatus(Info->Header.HostStatus);
}

IoInfoBlock SubmitIoRequestToHost(ULONG Type, ULONG Handle)
{
	IoInfoBlockOc InfoBlockOc;
	IoRequest Packet;
	Packet.Header.Id = InterlockedIncrement(&IopHostRequestId);
	Packet.Header.Type = Type;
	Packet.m_xx.Handle = Handle;
	SubmitIoRequestToHost(&Packet);
	RetrieveIoRequestFromHost(&InfoBlockOc, Packet.Header.Id);

	return InfoBlockOc.Header;
}

IoInfoBlock SubmitIoRequestToHost(ULONG Type, LONGLONG Offset, ULONG Size, ULONG Address, ULONG Handle)
{
	IoInfoBlockOc InfoBlockOc;
	IoRequest Packet;
	Packet.Header.Id = InterlockedIncrement(&IopHostRequestId);
	Packet.Header.Type = Type;
	Packet.m_rw.Offset = Offset;
	Packet.m_rw.Size = Size;
	Packet.m_rw.Address = Address;
	Packet.m_rw.Handle = Handle;
	Packet.m_rw.Timestamp = 0;
	SubmitIoRequestToHost(&Packet);
	RetrieveIoRequestFromHost(&InfoBlockOc, Packet.Header.Id);

	return InfoBlockOc.Header;
}

IoInfoBlock SubmitIoRequestToHost(ULONG Type, LONGLONG Offset, ULONG Size, ULONG Address, ULONG Handle, ULONG Timestamp)
{
	IoInfoBlockOc InfoBlockOc;
	IoRequest Packet;
	Packet.Header.Id = InterlockedIncrement(&IopHostRequestId);
	Packet.Header.Type = Type;
	Packet.m_rw.Offset = Offset;
	Packet.m_rw.Size = Size;
	Packet.m_rw.Address = Address;
	Packet.m_rw.Handle = Handle;
	Packet.m_rw.Timestamp = Timestamp;
	SubmitIoRequestToHost(&Packet);
	RetrieveIoRequestFromHost(&InfoBlockOc, Packet.Header.Id);

	return InfoBlockOc.Header;
}

IoInfoBlockOc SubmitIoRequestToHost(ULONG Type, LONGLONG InitialSize, ULONG Size, ULONG Handle, ULONG Path, ULONG Attributes,
	ULONG Timestamp, ULONG DesiredAccess, ULONG CreateOptions)
{
	IoInfoBlockOc InfoBlock;
	IoRequest Packet;
	Packet.Header.Id = InterlockedIncrement(&IopHostRequestId);
	Packet.Header.Type = Type;
	Packet.m_oc.InitialSize = InitialSize;
	Packet.m_oc.Size = Size;
	Packet.m_oc.Handle = Handle;
	Packet.m_oc.Path = Path;
	Packet.m_oc.Attributes = Attributes;
	Packet.m_oc.Timestamp = Timestamp;
	Packet.m_oc.DesiredAccess = DesiredAccess;
	Packet.m_oc.CreateOptions = CreateOptions;
	SubmitIoRequestToHost(&Packet);
	RetrieveIoRequestFromHost(&InfoBlock, Packet.Header.Id);

	return InfoBlock;
}

// Source: Cxbx-Reloaded
VOID InitializeListHead(PLIST_ENTRY pListHead)
{
	pListHead->Flink = pListHead->Blink = pListHead;
}

// Source: Cxbx-Reloaded
BOOLEAN IsListEmpty(PLIST_ENTRY pListHead)
{
	return (pListHead->Flink == pListHead);
}

// Source: Cxbx-Reloaded
VOID InsertTailList(PLIST_ENTRY pListHead, PLIST_ENTRY pEntry)
{
	PLIST_ENTRY _EX_ListHead = pListHead;
	PLIST_ENTRY _EX_Blink = _EX_ListHead->Blink;

	pEntry->Flink = _EX_ListHead;
	pEntry->Blink = _EX_Blink;
	_EX_Blink->Flink = pEntry;
	_EX_ListHead->Blink = pEntry;
}

// Source: Cxbx-Reloaded
VOID InsertHeadList(PLIST_ENTRY pListHead, PLIST_ENTRY pEntry)
{
	PLIST_ENTRY _EX_ListHead = pListHead;
	PLIST_ENTRY _EX_Flink = _EX_ListHead->Flink;

	pEntry->Flink = _EX_Flink;
	pEntry->Blink = _EX_ListHead;
	_EX_Flink->Blink = pEntry;
	_EX_ListHead->Flink = pEntry;
}

// Source: Cxbx-Reloaded
VOID RemoveEntryList(PLIST_ENTRY pEntry)
{
	PLIST_ENTRY _EX_Flink = pEntry->Flink;
	PLIST_ENTRY _EX_Blink = pEntry->Blink;
	_EX_Blink->Flink = _EX_Flink;
	_EX_Flink->Blink = _EX_Blink;
}

// Source: Cxbx-Reloaded
PLIST_ENTRY RemoveTailList(PLIST_ENTRY pListHead)
{
	PLIST_ENTRY pList = pListHead->Blink;
	RemoveEntryList(pList);
	return pList;
}

// Source: Cxbx-Reloaded
PLIST_ENTRY RemoveHeadList(PLIST_ENTRY pListHead)
{
	PLIST_ENTRY pList = pListHead->Flink;
	RemoveEntryList(pList);
	return pList;
}
