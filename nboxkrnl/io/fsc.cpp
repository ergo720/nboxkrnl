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
static INITIALIZE_GLOBAL_KEVENT(FscReleasedPagesEvent, SynchronizationEvent, FALSE);


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

	ULONG SizeOfNewCacheElementArray = sizeof(FSCACHE_ELEMENT) * NumberOfCachePages;
	PFSCACHE_ELEMENT NewFscCacheElementArray = (PFSCACHE_ELEMENT)ExAllocatePoolWithTag(SizeOfNewCacheElementArray, 'AcsF');
	if (NewFscCacheElementArray == nullptr) {
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	PCHAR CacheBuffer = (PCHAR)MiAllocateSystemMemory((NumberOfCachePages - FscCurrNumberOfCachePages) << PAGE_SHIFT, PAGE_READWRITE, Cache, FALSE);
	if (CacheBuffer == nullptr) {
		ExFreePool(NewFscCacheElementArray);
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	memcpy(NewFscCacheElementArray, FscCacheElementArray, SizeOfNewCacheElementArray);

	for (ULONG Index = FscCurrNumberOfCachePages; Index < NumberOfCachePages; ++Index) {
		NewFscCacheElementArray[Index].CacheExtension = nullptr; // marks the element as invalid
		NewFscCacheElementArray[Index].CacheBuffer = CacheBuffer; // clears all flag bits
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

	ULONG SizeOfNewCacheElementArray = sizeof(FSCACHE_ELEMENT) * NumberOfCachePages;
	PFSCACHE_ELEMENT NewFscCacheElementArray = (PFSCACHE_ELEMENT)ExAllocatePoolWithTag(SizeOfNewCacheElementArray, 'AcsF');
	if (NewFscCacheElementArray == nullptr) {
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	ULONG NumOfPagesToDelete = FscCurrNumberOfCachePages - NumberOfCachePages;
	retry:
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
		goto retry;
	}

	for (ULONG NewIndex = 0, Index = 0; Index < FscCurrNumberOfCachePages; ++Index) {
		if (FscCacheElementArray[Index].MarkForDeletion) {
			NumOfPagesToDelete += (MiFreeSystemMemory(FscCacheElementArray[Index].CacheBuffer, PAGE_SIZE) >> PAGE_SHIFT);
		}
		else {
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
