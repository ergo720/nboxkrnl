/*
 * ergo720                Copyright (c) 2023
 */

#include "fsc.hpp"
#include <string.h>
#include <assert.h>

#define MAX_NUMBER_OF_CACHE_PAGES 2048


// Array that tracks information about each page allocated for file system cache usage
static PFSCACHE_ELEMENT FscCacheElementArray = nullptr;

// List of all cache elements currently allocated. Most recently used elements are at the tail, and least used are at the head
static LIST_ENTRY FscCacheElementListHead;

static INITIALIZE_GLOBAL_KEVENT(FscUpdateNumOfPages, SynchronizationEvent, TRUE);
static INITIALIZE_GLOBAL_KEVENT(FscConcurrentWriteEvent, NotificationEvent, FALSE);
static INITIALIZE_GLOBAL_KEVENT(FscReleasedPagesEvent, SynchronizationEvent, FALSE);


static ULONG FscByteOffsetToAlignedOffset(ULONGLONG ByteOffset)
{
	// Note that this cannot cause a truncation error because an xbox HDD is only 8/10 GiB large

	return (ULONG)(ByteOffset >> PAGE_SHIFT);
}

static PFSCACHE_ELEMENT FscFindElement(PFSCACHE_EXTENSION CacheExtension, ULONG AlignedByteOffset)
{
	assert(KeGetCurrentIrql() == DISPATCH_LEVEL);

	PLIST_ENTRY Entry = FscCacheElementListHead.Blink;
	while (Entry != &FscCacheElementListHead) {
		PFSCACHE_ELEMENT Element = CONTAINING_RECORD(Entry, FSCACHE_ELEMENT, ListEntry);
		if ((Element->CacheExtension == CacheExtension) && (Element->AlignedByteOffset == AlignedByteOffset)) {
			RemoveEntryList(&Element->ListEntry);
			InsertTailList(&FscCacheElementListHead, &Element->ListEntry);

			return Element;
		}

		if (Element->CacheExtension == nullptr) {
			break;
		}

		Entry = Element->ListEntry.Blink;
	}

	return nullptr;
}

static PFSCACHE_ELEMENT FscFindElement(PVOID CacheBuffer)
{
	assert(KeGetCurrentIrql() == DISPATCH_LEVEL);

	PMMPTE Pte = GetPteAddress(CacheBuffer);
	PXBOX_PFN Pfn = GetPfnElement(Pte->Hw >> PAGE_SHIFT);

	return &FscCacheElementArray[Pfn->FscCache.Index];
}

static PFSCACHE_ELEMENT FscAllocateFreeElement()
{
	assert(KeGetCurrentIrql() == DISPATCH_LEVEL);

	PLIST_ENTRY Entry = FscCacheElementListHead.Flink;
	while (Entry != &FscCacheElementListHead) {
		PFSCACHE_ELEMENT Element = CONTAINING_RECORD(Entry, FSCACHE_ELEMENT, ListEntry);
		if (Element->NumOfUsers == 0) {
			return Element;
		}

		Entry = Element->ListEntry.Blink;
	}

	ULONG PagesLeftToAllocate = MAX_NUMBER_OF_CACHE_PAGES - FscCurrNumberOfCachePages;
	if (PagesLeftToAllocate) {
		if (!NT_SUCCESS(FscSetCacheSize(FscCurrNumberOfCachePages + (16 < PagesLeftToAllocate ? 16 : PagesLeftToAllocate)))) {
			return nullptr;
		}
	} else {
		return nullptr;
	}

	return FscAllocateFreeElement(); // can't fail now
}

static VOID FscReBuildCacheElementList()
{
	assert(KeGetCurrentIrql() == DISPATCH_LEVEL);

	// At the end of this function, valid elements are at the tail of the list, while invalid ones are at the head
	InitializeListHead(&FscCacheElementListHead);

	for (ULONG Index = 0; Index < FscCurrNumberOfCachePages; ++Index) {
		if (FscCacheElementArray[Index].CacheExtension) {
			InsertTailList(&FscCacheElementListHead, &FscCacheElementArray[Index].ListEntry);
		}
		else {
			InsertHeadList(&FscCacheElementListHead, &FscCacheElementArray[Index].ListEntry);
		}
	}
}

static NTSTATUS FscIncreaseCacheSize(ULONG NumberOfCachePages)
{
	assert(KeGetCurrentIrql() == DISPATCH_LEVEL);
	assert(NumberOfCachePages > FscCurrNumberOfCachePages);

	PFSCACHE_ELEMENT NewFscCacheElementArray = (PFSCACHE_ELEMENT)ExAllocatePoolWithTag(sizeof(FSCACHE_ELEMENT) * NumberOfCachePages, 'AcsF');
	if (NewFscCacheElementArray == nullptr) {
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	PCHAR CacheBuffer = (PCHAR)MiAllocateSystemMemory((NumberOfCachePages - FscCurrNumberOfCachePages) << PAGE_SHIFT, PAGE_READWRITE, Cache, FALSE);
	if (CacheBuffer == nullptr) {
		ExFreePool(NewFscCacheElementArray);
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	memcpy(NewFscCacheElementArray, FscCacheElementArray, sizeof(FSCACHE_ELEMENT) * FscCurrNumberOfCachePages);

	for (ULONG Index = FscCurrNumberOfCachePages; Index < NumberOfCachePages; ++Index) {
		NewFscCacheElementArray[Index].CacheExtension = nullptr; // marks the element as invalid
		NewFscCacheElementArray[Index].CacheBuffer = CacheBuffer; // clears all flag bits
		PMMPTE Pte = GetPteAddress(CacheBuffer);
		PXBOX_PFN Pfn = GetPfnElement(Pte->Hw >> PAGE_SHIFT);
		Pfn->FscCache.Index = Index;
		CacheBuffer += PAGE_SIZE;
	}

	ExFreePool(FscCacheElementArray);

	FscCacheElementArray = NewFscCacheElementArray;
	FscCurrNumberOfCachePages = NumberOfCachePages;

	FscReBuildCacheElementList();

	return STATUS_SUCCESS;
}

static NTSTATUS FscReduceCacheSize(ULONG NumberOfCachePages)
{
	assert(KeGetCurrentIrql() == DISPATCH_LEVEL);
	assert(NumberOfCachePages < FscCurrNumberOfCachePages);

	PFSCACHE_ELEMENT NewFscCacheElementArray;
	if (NumberOfCachePages == 0) {
		NewFscCacheElementArray = nullptr;
	}
	else {
		ULONG SizeOfNewCacheElementArray = sizeof(FSCACHE_ELEMENT) * NumberOfCachePages;
		NewFscCacheElementArray = (PFSCACHE_ELEMENT)ExAllocatePoolWithTag(SizeOfNewCacheElementArray, 'AcsF');
		if (NewFscCacheElementArray == nullptr) {
			return STATUS_INSUFFICIENT_RESOURCES;
		}
	}

	ULONG NumOfPagesToDelete = FscCurrNumberOfCachePages - NumberOfCachePages;
Retry:
	PLIST_ENTRY Entry = FscCacheElementListHead.Flink;
	while (Entry != &FscCacheElementListHead) {
		PFSCACHE_ELEMENT Element = CONTAINING_RECORD(Entry, FSCACHE_ELEMENT, ListEntry);
		if ((Element->NumOfUsers == 0) && (Element->MarkForDeletion == 0)) {
			Element->MarkForDeletion = 1;
			Element->CacheExtension = nullptr;
			--NumOfPagesToDelete;
			if (NumOfPagesToDelete == 0) {
				break;
			}
		}
	}

	if (NumOfPagesToDelete) {
		// NOTE: when this is called, this thread holds the FscUpdateNumOfPages lock, so FscCurrNumberOfCachePages cannot change after the IRQL is lowered
		MiUnlock(PASSIVE_LEVEL);
		KeWaitForSingleObject(&FscReleasedPagesEvent, Executive, KernelMode, FALSE, nullptr);
		MiLock();
		goto Retry;
	}

	for (ULONG NewIndex = 0, Index = 0; Index < FscCurrNumberOfCachePages; ++Index) {
		if (FscCacheElementArray[Index].MarkForDeletion) {
			NumOfPagesToDelete += (MiFreeSystemMemory(FscCacheElementArray[Index].CacheBuffer, PAGE_SIZE) >> PAGE_SHIFT);
		}
		else {
			// NOTE: this case is not triggered when NumberOfCachePages == 0, because the above loop will mark all elements for deletion
			NewFscCacheElementArray[NewIndex] = FscCacheElementArray[Index];
			++NewIndex;
		}
	}

	assert(NumOfPagesToDelete == (FscCurrNumberOfCachePages - NumberOfCachePages));

	ExFreePool(FscCacheElementArray);

	FscCacheElementArray = NewFscCacheElementArray;
	FscCurrNumberOfCachePages = NumberOfCachePages;

	FscReBuildCacheElementList();

	return STATUS_SUCCESS;
}

VOID FscFlushPartitionElements(PFSCACHE_EXTENSION CacheExtension)
{
	// Acquire a lock to avoid somebody else changing the number of cache pages while we are trying to flush them
	KeWaitForSingleObject(&FscUpdateNumOfPages, Executive, KernelMode, FALSE, nullptr);

	KIRQL OldIrql = MiLock();

	ULONG NumOfPagesToFlushLeft = 0;
	PLIST_ENTRY Entry = FscCacheElementListHead.Blink;
	while (Entry != &FscCacheElementListHead) {
		PFSCACHE_ELEMENT Element = CONTAINING_RECORD(Entry, FSCACHE_ELEMENT, ListEntry);
		if (Element->CacheExtension == CacheExtension) {
			++NumOfPagesToFlushLeft;
		}

		if (Element->CacheExtension == nullptr) {
			break;
		}

		Entry = Element->ListEntry.Blink;
	}

Retry:
	Entry = FscCacheElementListHead.Blink;
	while (Entry != &FscCacheElementListHead) {
		PFSCACHE_ELEMENT Element = CONTAINING_RECORD(Entry, FSCACHE_ELEMENT, ListEntry);
		if ((Element->CacheExtension == CacheExtension) && (Element->NumOfUsers == 0) && (Element->MarkForDeletion == 0)) {
			--NumOfPagesToFlushLeft;
			Element->MarkForDeletion = 1;
			Element->CacheExtension = nullptr;
			RemoveEntryList(&Element->ListEntry);
		}

		if (Element->CacheExtension == nullptr) {
			break;
		}

		Entry = Element->ListEntry.Blink;
	}

	if (NumOfPagesToFlushLeft) {
		MiUnlock(PASSIVE_LEVEL);
		KeWaitForSingleObject(&FscReleasedPagesEvent, Executive, KernelMode, FALSE, nullptr);
		MiLock();
		goto Retry;
	}

	MiUnlock(OldIrql);

	KeSetEvent(&FscUpdateNumOfPages, 0, FALSE);
}

NTSTATUS FscMapElementPage(PFSCACHE_EXTENSION CacheExtension, ULONGLONG ByteOffset, PVOID *ReturnedBuffer, BOOLEAN IsWrite)
{
	KIRQL OldIrql = MiLock();

Retry:
	ULONG AlignedByteOffset = FscByteOffsetToAlignedOffset(ByteOffset);
	PFSCACHE_ELEMENT Element = FscFindElement(CacheExtension, AlignedByteOffset);
	if (Element) {
		if (Element->WriteInProgress == 0) {
			++Element->NumOfUsers;
			Element->WriteInProgress = IsWrite ? 1 : 0;
			*ReturnedBuffer = PVOID((ULONG(Element->CacheBuffer) & ~PAGE_MASK) + BYTE_OFFSET(ByteOffset));
			MiUnlock(OldIrql);

			return STATUS_SUCCESS;
		}

		MiUnlock(OldIrql);
		KeWaitForSingleObject(&FscConcurrentWriteEvent, Executive, KernelMode, FALSE, nullptr);
		OldIrql = MiLock();

		goto Retry;
	}
	else {
		if (Element = FscAllocateFreeElement(); Element == nullptr) {
			MiUnlock(OldIrql);
			return STATUS_NO_MEMORY;
		}

		// Set NumOfUsers before releasing the Mm lock so that FscReduceCacheSize doesn't remove the element we are using
		// Also set WriteInProgress so that FscMapElementPage doesn't attempt to use this element
		Element->AlignedByteOffset = AlignedByteOffset;
		Element->CacheExtension = CacheExtension;
		Element->NumOfUsers = 1;
		Element->WriteInProgress = 1;

		MiUnlock(OldIrql);

		IoInfoBlock InfoBlock = SubmitIoRequestToHost(
			IoRequestType::Read | DEV_TYPE(CacheExtension->DeviceType),
			ByteOffset,
			PAGE_SIZE,
			ULONG(Element->CacheBuffer) & ~PAGE_MASK,
			CacheExtension->HostHandle
		);

		OldIrql = MiLock();

		NTSTATUS Status = InfoBlock.NtStatus;
		if (Status == STATUS_SUCCESS) {
			Element->WriteInProgress = IsWrite ? 1 : 0;
			*ReturnedBuffer = PVOID((ULONG(Element->CacheBuffer) & ~PAGE_MASK) + BYTE_OFFSET(ByteOffset));
		}
		else if (Status == STATUS_PENDING) {
			// Should not happen right now, because RetrieveIoRequestFromHost is always synchronous
			RIP_API_MSG("Asynchronous IO is not supported");
		}
		else {
			Element->CacheBuffer = PCHAR(ULONG(Element->CacheBuffer) & ~PAGE_MASK);
		}

		MiUnlock(OldIrql);

		return Status;
	}
}

VOID FscUnmapElementPage(PVOID Buffer)
{
	KIRQL OldIrql = MiLock();

	// NOTE: there can only be a single write happening at any given time, since other threads attempting new writes will block in FscMapElementPage
	PFSCACHE_ELEMENT Element = FscFindElement(Buffer);
	--Element->NumOfUsers;
	Element->WriteInProgress = 0;
	if ((Element->NumOfUsers == 0) && (IsListEmpty(&FscReleasedPagesEvent.Header.WaitListHead) == FALSE)) {
		KeSetEvent(&FscReleasedPagesEvent, 0, FALSE);
	}
	KeSetEvent(&FscConcurrentWriteEvent, 0, FALSE);
	FscConcurrentWriteEvent.Header.SignalState = 0;

	MiUnlock(OldIrql);
}

EXPORTNUM(35) ULONG XBOXAPI FscGetCacheSize()
{
	return FscCurrNumberOfCachePages;
}

EXPORTNUM(37) NTSTATUS XBOXAPI FscSetCacheSize
(
	ULONG NumberOfCachePages
)
{
	KeWaitForSingleObject(&FscUpdateNumOfPages, Executive, KernelMode, FALSE, nullptr);

	KIRQL OldIrql = MiLock();

	NTSTATUS Status = STATUS_SUCCESS;
	if (NumberOfCachePages > MAX_NUMBER_OF_CACHE_PAGES) {
		Status = STATUS_INVALID_PARAMETER;
	}
	else if (NumberOfCachePages > FscCurrNumberOfCachePages) {
		Status = FscIncreaseCacheSize(NumberOfCachePages);
	}
	else if (NumberOfCachePages < FscCurrNumberOfCachePages) {
		Status = FscReduceCacheSize(NumberOfCachePages);
	}

	MiUnlock(OldIrql);

	KeSetEvent(&FscUpdateNumOfPages, 0, FALSE);

	return Status;
}
