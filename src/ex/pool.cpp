/*-
* SPDX-License-Identifier: BSD-2-Clause
*
* Copyright (c) 2008 ReactOS Portable Systems Group. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions
* are met:
* 1. Redistributions of source code must retain the above copyright
*    notice, this list of conditions and the following disclaimer.
* 2. Redistributions in binary form must reproduce the above copyright
*    notice, this list of conditions and the following disclaimer in the
*    documentation and/or other materials provided with the distribution.
*
* THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
* ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
* OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
* LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
* OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
* SUCH DAMAGE.
*/

#include "mm.hpp"
#include "mi.hpp"
#include "ex.hpp"
#include "dbg.hpp"
#include <assert.h>


// Macros to ensure thread safety
#define PoolLock() KeRaiseIrqlToDpcLevel()
#define PoolUnlock(Irql) KfLowerIrql(Irql)
#define ExLockPool(Desc) PoolLock()
#define ExUnlockPool(Desc, OldIrql) PoolUnlock(OldIrql)


#define POOL_BLOCK_SIZE  8
#define POOL_LISTS_PER_PAGE (PAGE_SIZE / POOL_BLOCK_SIZE)
#define BASE_POOL_TYPE_MASK 1
#define POOL_MAX_ALLOC (PAGE_SIZE - (sizeof(POOL_HEADER) + POOL_BLOCK_SIZE))

#define POOL_BIG_TABLE_ENTRY_FREE 0x1
/*
* This defines when we shrink or expand the table.
* 3 --> keep the number of used entries in the 33%-66% of the table capacity.
* 4 --> 25% - 75%
* etc.
*/
#define POOL_BIG_TABLE_USE_RATE 4

#define PROTECTED_POOL 0x80000000


/* Pool block/header/list access macros */
#define POOL_ENTRY(x)       (PPOOL_HEADER)((ULONG_PTR)(x) - sizeof(POOL_HEADER))
#define POOL_FREE_BLOCK(x)  (PLIST_ENTRY)((ULONG_PTR)(x)  + sizeof(POOL_HEADER))
#define POOL_BLOCK(x, i)    (PPOOL_HEADER)((ULONG_PTR)(x) + ((i) * POOL_BLOCK_SIZE))
#define POOL_NEXT_BLOCK(x)  POOL_BLOCK((x), (x)->BlockSize)
#define POOL_PREV_BLOCK(x)  POOL_BLOCK((x), -((x)->PreviousSize))


typedef enum _POOL_TYPE
{
	NonPagedPool,
} POOL_TYPE;

#ifdef ENABLE_POOL_TAG_TRACKING
typedef struct _POOL_TRACKER_TABLE
{
	ULONG Key;
	LONG NonPagedAllocs;
	LONG NonPagedFrees;
	SIZE_T NonPagedBytes;
	LONG PagedAllocs;
	LONG PagedFrees;
	SIZE_T PagedBytes;
} POOL_TRACKER_TABLE, *PPOOL_TRACKER_TABLE;

typedef struct _POOL_TRACKER_BIG_PAGES
{
	PVOID Va;
	ULONG Key;
	ULONG NumberOfPages;
	PVOID QuotaObject;
} POOL_TRACKER_BIG_PAGES, *PPOOL_TRACKER_BIG_PAGES;
#endif

typedef struct _POOL_DESCRIPTOR
{
	ULONG PoolIndex;
	ULONG RunningAllocs;
	ULONG RunningDeAllocs;
	ULONG TotalPages;
	ULONG TotalBigPages;
	ULONG Threshold;
	PVOID LockAddress;
	PVOID PendingFrees;
	LONG PendingFreeDepth;
	SIZE_T TotalBytes;
	SIZE_T Spare0;
	LIST_ENTRY ListHeads[POOL_LISTS_PER_PAGE];
} POOL_DESCRIPTOR, *PPOOL_DESCRIPTOR;

typedef struct _POOL_HEADER
{
	union
	{
		struct
		{
			USHORT PreviousSize:9;
			USHORT PoolIndex:7;
			USHORT BlockSize:9;
			USHORT PoolType:7;
		};
		ULONG Ulong1;
	};
	union
	{
		ULONG PoolTag;
		struct
		{
			USHORT AllocatorBackTraceIndex;
			USHORT PoolTagHash;
		};
	};
} POOL_HEADER, *PPOOL_HEADER;


// Only the non-paged pool is supported on Xbox
static POOL_DESCRIPTOR NonPagedPoolDescriptor;
#ifdef ENABLE_POOL_TAG_TRACKING
static SIZE_T PoolTrackTableSize, PoolTrackTableMask;
static SIZE_T PoolBigPageTableSize, PoolBigPageTableHash;
static PPOOL_TRACKER_BIG_PAGES PoolBigPageTable;
static ULONG ExpPoolBigEntriesInUse;
static ULONG ExpBigTableExpansionFailed;
static ULONG PoolHitTag;
static PPOOL_TRACKER_TABLE PoolTrackTable;
static BOOLEAN ExStopBadTags;
static ULONG ExPoolFailures;
#endif


static PLIST_ENTRY ExpEncodePoolLink(PLIST_ENTRY Link)
{
	return (PLIST_ENTRY)((ULONG_PTR)Link | 1);
}

static PLIST_ENTRY ExpDecodePoolLink(PLIST_ENTRY Link)
{
	return (PLIST_ENTRY)((ULONG_PTR)Link & ~1);
}

static BOOLEAN ExpIsPoolListEmpty(PLIST_ENTRY ListHead)
{
	return (ExpDecodePoolLink(ListHead->Flink) == ListHead);
}

static VOID ExpCheckPoolIrqlLevel(POOL_TYPE PoolType, SIZE_T NumberOfBytes, PVOID Entry)
{
#if _DEBUG
	//
	// Validate IRQL: It must be APC_LEVEL or lower for Paged Pool, and it must
	// be DISPATCH_LEVEL or lower for Non Paged Pool
	//
	if (KeGetCurrentIrql() > DISPATCH_LEVEL)
	{
		//
		// Take the system down
		//
		KeBugCheckEx(BAD_POOL_CALLER,
			!Entry ? POOL_ALLOC_IRQL_INVALID : POOL_FREE_IRQL_INVALID,
			KeGetCurrentIrql(),
			PoolType,
			!Entry ? NumberOfBytes : (ULONG_PTR)Entry);
	}
#endif
}

static VOID ExpCheckPoolHeader(PPOOL_HEADER Entry)
{
#if _DEBUG
	PPOOL_HEADER PreviousEntry, NextEntry;

	/* Is there a block before this one? */
	if (Entry->PreviousSize)
	{
		/* Get it */
		PreviousEntry = POOL_PREV_BLOCK(Entry);

		/* The two blocks must be on the same page! */
		if ((PPOOL_HEADER)ROUND_DOWN_4K(Entry) != (PPOOL_HEADER)ROUND_DOWN_4K(PreviousEntry))
		{
			/* Something is awry */
			KeBugCheckEx(BAD_POOL_HEADER,
				6,
				(ULONG_PTR)PreviousEntry,
				__LINE__,
				(ULONG_PTR)Entry);
		}

		/* This block should also indicate that it's as large as we think it is */
		if (PreviousEntry->BlockSize != Entry->PreviousSize)
		{
			/* Otherwise, someone corrupted one of the sizes */
			DbgPrint("PreviousEntry BlockSize %lu, tag %.4s. Entry PreviousSize %lu, tag %.4s",
				PreviousEntry->BlockSize, (char *)&PreviousEntry->PoolTag,
				Entry->PreviousSize, (char *)&Entry->PoolTag);
			KeBugCheckEx(BAD_POOL_HEADER,
				5,
				(ULONG_PTR)PreviousEntry,
				__LINE__,
				(ULONG_PTR)Entry);
		}
	}
	else if ((PPOOL_HEADER)ROUND_DOWN_4K(Entry) != Entry)
	{
		/* If there's no block before us, we are the first block, so we should be on a page boundary */
		KeBugCheckEx(BAD_POOL_HEADER,
			7,
			0,
			__LINE__,
			(ULONG_PTR)Entry);
	}

	/* This block must have a size */
	if (!Entry->BlockSize)
	{
		/* Someone must've corrupted this field */
		if (Entry->PreviousSize)
		{
			PreviousEntry = POOL_PREV_BLOCK(Entry);
			DbgPrint("PreviousEntry tag %.4s. Entry tag %.4s",
				(char *)&PreviousEntry->PoolTag,
				(char *)&Entry->PoolTag);
		}
		else
		{
			DbgPrint("Entry tag %.4s",
				(char *)&Entry->PoolTag);
		}
		KeBugCheckEx(BAD_POOL_HEADER,
			8,
			0,
			__LINE__,
			(ULONG_PTR)Entry);
	}

	/* Okay, now get the next block */
	NextEntry = POOL_NEXT_BLOCK(Entry);

	/* If this is the last block, then we'll be page-aligned, otherwise, check this block */
	if ((PPOOL_HEADER)ROUND_DOWN_4K(NextEntry) != NextEntry)
	{
		/* The two blocks must be on the same page! */
		if ((PPOOL_HEADER)ROUND_DOWN_4K(Entry) != (PPOOL_HEADER)ROUND_DOWN_4K(NextEntry))
		{
			/* Something is messed up */
			KeBugCheckEx(BAD_POOL_HEADER,
				9,
				(ULONG_PTR)NextEntry,
				__LINE__,
				(ULONG_PTR)Entry);
		}

		/* And this block should think we are as large as we truly are */
		if (NextEntry->PreviousSize != Entry->BlockSize)
		{
			/* Otherwise, someone corrupted the field */
			DbgPrint("Entry BlockSize %lu, tag %.4s. NextEntry PreviousSize %lu, tag %.4s",
				Entry->BlockSize, (char *)&Entry->PoolTag,
				NextEntry->PreviousSize, (char *)&NextEntry->PoolTag);
			KeBugCheckEx(BAD_POOL_HEADER,
				5,
				(ULONG_PTR)NextEntry,
				__LINE__,
				(ULONG_PTR)Entry);
		}
	}
#endif
}

static VOID ExpCheckPoolLinks(PLIST_ENTRY ListHead)
{
#if _DEBUG
	if ((ExpDecodePoolLink(ExpDecodePoolLink(ListHead->Flink)->Blink) != ListHead) ||
		(ExpDecodePoolLink(ExpDecodePoolLink(ListHead->Blink)->Flink) != ListHead))
	{
		KeBugCheckEx(BAD_POOL_HEADER,
			3,
			(ULONG_PTR)ListHead,
			(ULONG_PTR)ExpDecodePoolLink(ExpDecodePoolLink(ListHead->Flink)->Blink),
			(ULONG_PTR)ExpDecodePoolLink(ExpDecodePoolLink(ListHead->Blink)->Flink));
	}
#endif
}

static VOID ExpCheckPoolBlocks(PVOID Block)
{
#if _DEBUG
	BOOLEAN FoundBlock = FALSE;
	SIZE_T Size = 0;
	PPOOL_HEADER Entry;

	/* Get the first entry for this page, make sure it really is the first */
	Entry = (PPOOL_HEADER)ROUND_DOWN_4K(Block);
	assert(Entry->PreviousSize == 0);

	/* Now scan each entry */
	while (TRUE)
	{
		/* When we actually found our block, remember this */
		if (Entry == Block) FoundBlock = TRUE;

		/* Now validate this block header */
		ExpCheckPoolHeader(Entry);

		/* And go to the next one, keeping track of our size */
		Size += Entry->BlockSize;
		Entry = POOL_NEXT_BLOCK(Entry);

		/* If we hit the last block, stop */
		if (Size >= (PAGE_SIZE / POOL_BLOCK_SIZE)) break;

		/* If we hit the end of the page, stop */
		if ((PPOOL_HEADER)ROUND_DOWN_4K(Entry) == Entry) break;
	}

	/* We must've found our block, and we must have hit the end of the page */
	if (((PPOOL_HEADER)ROUND_DOWN_4K(Entry) != Entry) || !(FoundBlock))
	{
		/* Otherwise, the blocks are messed up */
		KeBugCheckEx(BAD_POOL_HEADER, 10, (ULONG_PTR)Block, __LINE__, (ULONG_PTR)Entry);
	}
#endif
}

static PLIST_ENTRY ExpRemovePoolHeadList(PLIST_ENTRY ListHead)
{
	PLIST_ENTRY Entry, Flink;
	Entry = ExpDecodePoolLink(ListHead->Flink);
	Flink = ExpDecodePoolLink(Entry->Flink);
	ListHead->Flink = ExpEncodePoolLink(Flink);
	Flink->Blink = ExpEncodePoolLink(ListHead);
	return Entry;
}

static VOID ExpRemovePoolEntryList(PLIST_ENTRY Entry)
{
	PLIST_ENTRY Blink, Flink;
	Flink = ExpDecodePoolLink(Entry->Flink);
	Blink = ExpDecodePoolLink(Entry->Blink);
	Flink->Blink = ExpEncodePoolLink(Blink);
	Blink->Flink = ExpEncodePoolLink(Flink);
}

static VOID ExpInsertPoolHeadList(PLIST_ENTRY ListHead, PLIST_ENTRY Entry)
{
	PLIST_ENTRY Flink;
	ExpCheckPoolLinks(ListHead);
	Flink = ExpDecodePoolLink(ListHead->Flink);
	Entry->Flink = ExpEncodePoolLink(Flink);
	Entry->Blink = ExpEncodePoolLink(ListHead);
	Flink->Blink = ExpEncodePoolLink(Entry);
	ListHead->Flink = ExpEncodePoolLink(Entry);
	ExpCheckPoolLinks(ListHead);
}

static VOID ExpInsertPoolTailList(PLIST_ENTRY ListHead, PLIST_ENTRY Entry)
{
	PLIST_ENTRY Blink;
	ExpCheckPoolLinks(ListHead);
	Blink = ExpDecodePoolLink(ListHead->Blink);
	Entry->Flink = ExpEncodePoolLink(ListHead);
	Entry->Blink = ExpEncodePoolLink(Blink);
	Blink->Flink = ExpEncodePoolLink(Entry);
	ListHead->Blink = ExpEncodePoolLink(Entry);
	ExpCheckPoolLinks(ListHead);
}

#ifdef ENABLE_POOL_TAG_TRACKING
static ULONG ExpComputePartialHashForAddress(PVOID BaseAddress)
{
	ULONG Result;
	//
	// Compute the hash by converting the address into a page number, and then
	// XORing each nibble with the next one.
	//
	// We do *NOT* AND with the bucket mask at this point because big table expansion
	// might happen. Therefore, the final step of the hash must be performed
	// while holding the expansion pushlock, and this is why we call this a
	// "partial" hash only.
	//
	Result = (ULONG)((ULONG_PTR)BaseAddress >> PAGE_SHIFT);
	return (Result >> 24) ^ (Result >> 16) ^ (Result >> 8) ^ Result;
}

static ULONG ExpComputeHashForTag(ULONG Tag, SIZE_T BucketMask)
{
	//
	// Compute the hash by multiplying with a large prime number and then XORing
	// with the HIDWORD of the result.
	//
	// Finally, AND with the bucket mask to generate a valid index/bucket into
	// the table
	//
	ULONGLONG Result = (ULONGLONG)40543 * Tag;
	return (ULONG)BucketMask & ((ULONG)Result ^ (Result >> 32));
}

static VOID ExpRemovePoolTracker(ULONG Key, SIZE_T NumberOfBytes)
{
	ULONG Hash, Index;
	PPOOL_TRACKER_TABLE Table, TableEntry;
	SIZE_T TableMask, TableSize;

	//
	// Remove the PROTECTED_POOL flag which is not part of the tag
	//
	Key &= ~PROTECTED_POOL;

	//
	// With WinDBG you can set a tag you want to break on when an allocation is
	// attempted
	//
	if (Key == PoolHitTag) DbgBreakPoint();

	//
	// Why the double indirection? Because normally this function is also used
	// when doing session pool allocations, which has another set of tables,
	// sizes, and masks that live in session pool. Now we don't support session
	// pool so we only ever use the regular tables, but I'm keeping the code this
	// way so that the day we DO support session pool, it won't require that
	// many changes
	//
	Table = PoolTrackTable;
	TableMask = PoolTrackTableMask;
	TableSize = PoolTrackTableSize;
	DBG_UNREFERENCED_LOCAL_VARIABLE(TableSize);

	//
	// Compute the hash for this key, and loop all the possible buckets
	//
	Hash = ExpComputeHashForTag(Key, TableMask);
	Index = Hash;
	while (TRUE)
	{
		//
		// Have we found the entry for this tag? */
		//
		TableEntry = &Table[Hash];
		if (TableEntry->Key == Key)
		{
			//
			// Decrement the counters of the nonpaged pool
			//
			InterlockedIncrement(&TableEntry->NonPagedFrees);
			InterlockedExchangeAddSizeT(&TableEntry->NonPagedBytes,
				-(SSIZE_T)NumberOfBytes);
			return;
		}

		//
		// We should have only ended up with an empty entry if we've reached
		// the last bucket
		//
		if (!TableEntry->Key)
		{
			DbgPrint("Empty item reached in tracker table. Hash=0x%lx, TableMask=0x%lx, Tag=0x%08lx, NumberOfBytes=%lu",
				Hash, TableMask, Key, (ULONG)NumberOfBytes);
			assert(Hash == TableMask);
		}

		//
		// This path is hit when we don't have an entry, and the current bucket
		// is full, so we simply try the next one
		//
		Hash = (Hash + 1) & TableMask;
		if (Hash == Index) break;
	}

	//
	// And finally this path is hit when all the buckets are full, and we need
	// some expansion. This path is not yet supported in ReactOS and so we'll
	// ignore the tag
	//
	DbgPrint("Out of pool tag space, ignoring...");
}

static VOID ExpInsertPoolTracker(ULONG Key, SIZE_T NumberOfBytes)
{
	ULONG Hash, Index;
	KIRQL OldIrql;
	PPOOL_TRACKER_TABLE Table, TableEntry;
	SIZE_T TableMask, TableSize;

	//
	// Remove the PROTECTED_POOL flag which is not part of the tag
	//
	Key &= ~PROTECTED_POOL;

	//
	// With WinDBG you can set a tag you want to break on when an allocation is
	// attempted
	//
	if (Key == PoolHitTag) DbgBreakPoint();

	//
	// There is also an internal flag you can set to break on malformed tags
	//
	if (ExStopBadTags) assert(Key & 0xFFFFFF00);

	//
	// Why the double indirection? Because normally this function is also used
	// when doing session pool allocations, which has another set of tables,
	// sizes, and masks that live in session pool. Now we don't support session
	// pool so we only ever use the regular tables, but I'm keeping the code this
	// way so that the day we DO support session pool, it won't require that
	// many changes
	//
	Table = PoolTrackTable;
	TableMask = PoolTrackTableMask;
	TableSize = PoolTrackTableSize;
	DBG_UNREFERENCED_LOCAL_VARIABLE(TableSize);

	//
	// Compute the hash for this key, and loop all the possible buckets
	//
	Hash = ExpComputeHashForTag(Key, TableMask);
	Index = Hash;
	while (TRUE)
	{
		//
		// Do we already have an entry for this tag? */
		//
		TableEntry = &Table[Hash];
		if (TableEntry->Key == Key)
		{
			//
			// Increment the counters of the nonpaged pool
			//
			InterlockedIncrement(&TableEntry->NonPagedAllocs);
			InterlockedExchangeAddSizeT(&TableEntry->NonPagedBytes, NumberOfBytes);
			return;
		}

		//
		// We don't have an entry yet, but we've found a free bucket for it
		//
		if (!(TableEntry->Key) && (Hash != PoolTrackTableSize - 1))
		{
			//
			// We need to hold the lock while creating a new entry, since other
			// processors might be in this code path as well
			//
			OldIrql = PoolLock();
			if (!PoolTrackTable[Hash].Key)
			{
				//
				// We've won the race, so now create this entry in the bucket
				//
				assert(Table[Hash].Key == 0);
				PoolTrackTable[Hash].Key = Key;
				TableEntry->Key = Key;
			}
			PoolUnlock(OldIrql);

			//
			// Now we force the loop to run again, and we should now end up in
			// the code path above which does the interlocked increments...
			//
			continue;
		}

		//
		// This path is hit when we don't have an entry, and the current bucket
		// is full, so we simply try the next one
		//
		Hash = (Hash + 1) & TableMask;
		if (Hash == Index) break;
	}

	//
	// And finally this path is hit when all the buckets are full, and we need
	// some expansion. This path is not yet supported in ReactOS and so we'll
	// ignore the tag
	//
	DbgPrint("Out of pool tag space, ignoring...");
}

static BOOLEAN ExpReallocateBigPageTable(KIRQL OldIrql, BOOLEAN Shrink)
{
	SIZE_T OldSize = PoolBigPageTableSize;
	SIZE_T NewSize, NewSizeInBytes;
	PPOOL_TRACKER_BIG_PAGES NewTable;
	PPOOL_TRACKER_BIG_PAGES OldTable;
	ULONG i;
	ULONG PagesFreed;
	ULONG Hash;
	ULONG HashMask;

	/* Must be holding ExpLargePoolTableLock */
	assert(KeGetCurrentIrql() == DISPATCH_LEVEL);

	/* Make sure we don't overflow */
	if (Shrink)
	{
		NewSize = OldSize / 2;

		/* Make sure we don't shrink too much. */
		assert(NewSize >= ExpPoolBigEntriesInUse);

		NewSize = ROUND_UP(NewSize, PAGE_SIZE / sizeof(POOL_TRACKER_BIG_PAGES));
		assert(NewSize <= OldSize);

		/* If there is only one page left, then keep it around. Not a failure either. */
		if (NewSize == OldSize)
		{
			assert(NewSize == (PAGE_SIZE / sizeof(POOL_TRACKER_BIG_PAGES)));
			PoolUnlock(OldIrql);
			return TRUE;
		}
	}
	else
	{
		if (!NT_SUCCESS(RtlSIZETMult(2, OldSize, &NewSize)))
		{
			DbgPrint("Overflow expanding big page table. Size=%lu", OldSize);
			PoolUnlock(OldIrql);
			return FALSE;
		}

		/* Make sure we don't stupidly waste pages */
		NewSize = ROUND_DOWN(NewSize, PAGE_SIZE / sizeof(POOL_TRACKER_BIG_PAGES));
		assert(NewSize > OldSize);
	}

	if (!NT_SUCCESS(RtlSIZETMult(sizeof(POOL_TRACKER_BIG_PAGES), NewSize, &NewSizeInBytes)))
	{
		DbgPrint("Overflow while calculating big page table size. Size=%lu", OldSize);
		PoolUnlock(OldIrql);
		return FALSE;
	}

	NewTable = (PPOOL_TRACKER_BIG_PAGES)MiAllocateSystemMemory(NewSizeInBytes, PAGE_READWRITE, Pool, FALSE);
	if (NewTable == NULL)
	{
		DbgPrint("Could not allocate %lu bytes for new big page table", NewSizeInBytes);
		PoolUnlock(OldIrql);
		return FALSE;
	}

	DbgPrint("%s big pool tracker table to %lu entries", Shrink ? "Shrinking" : "Expanding", NewSize);

	/* Initialize the new table */
	RtlZeroMemory(NewTable, NewSizeInBytes);
	for (i = 0; i < NewSize; i++)
	{
		NewTable[i].Va = (PVOID)POOL_BIG_TABLE_ENTRY_FREE;
	}

	/* Copy over all items */
	OldTable = PoolBigPageTable;
	HashMask = NewSize - 1;
	for (i = 0; i < OldSize; i++)
	{
		/* Skip over empty items */
		if ((ULONG_PTR)OldTable[i].Va & POOL_BIG_TABLE_ENTRY_FREE)
		{
			continue;
		}

		/* Recalculate the hash due to the new table size */
		Hash = ExpComputePartialHashForAddress(OldTable[i].Va) % HashMask;

		/* Find the location in the new table */
		while (!((ULONG_PTR)NewTable[Hash].Va & POOL_BIG_TABLE_ENTRY_FREE))
		{
			if (++Hash == NewSize)
				Hash = 0;
		}

		/* We must have space */
		assert((ULONG_PTR)NewTable[Hash].Va & POOL_BIG_TABLE_ENTRY_FREE);

		/* Finally, copy the item */
		NewTable[Hash] = OldTable[i];
	}

	/* Activate the new table */
	PoolBigPageTable = NewTable;
	PoolBigPageTableSize = NewSize;
	PoolBigPageTableHash = PoolBigPageTableSize - 1;

	/* Release the lock, we're done changing global state */
	PoolUnlock(OldIrql);

	/* Free the old table and update our tracker */
	PagesFreed = MiFreeSystemMemory(OldTable, 0);
	ExpRemovePoolTracker('looP', PagesFreed << PAGE_SHIFT);
	ExpInsertPoolTracker('looP', ROUND_UP(NewSizeInBytes, PAGE_SIZE));

	return TRUE;
}

static ULONG ExpFindAndRemoveTagBigPages(PVOID Va, PULONG_PTR BigPages)
{
	BOOLEAN FirstTry = TRUE;
	SIZE_T TableSize;
	KIRQL OldIrql;
	ULONG PoolTag, Hash;
	PPOOL_TRACKER_BIG_PAGES Entry;
	assert(((ULONG_PTR)Va & POOL_BIG_TABLE_ENTRY_FREE) == 0);

	//
	// As the table is expandable, these values must only be read after acquiring
	// the lock to avoid a teared access during an expansion
	//
	Hash = ExpComputePartialHashForAddress(Va);
	OldIrql = PoolLock();
	Hash &= PoolBigPageTableHash;
	TableSize = PoolBigPageTableSize;

	//
	// Loop while trying to find this big page allocation
	//
	while (PoolBigPageTable[Hash].Va != Va)
	{
		//
		// Increment the size until we go past the end of the table
		//
		if (++Hash >= TableSize)
		{
			//
			// Is this the second time we've tried?
			//
			if (!FirstTry)
			{
				//
				// This means it was never inserted into the pool table and it
				// received the special "BIG" tag -- return that and return 0
				// so that the code can ask Mm for the page count instead
				//
				PoolUnlock(OldIrql);
				*BigPages = 0;
				return ' GIB';
			}

			//
			// The first time this happens, reset the hash index and try again
			//
			Hash = 0;
			FirstTry = FALSE;
		}
	}

	//
	// Now capture all the information we need from the entry, since after we
	// release the lock, the data can change
	//
	Entry = &PoolBigPageTable[Hash];
	*BigPages = Entry->NumberOfPages;
	PoolTag = Entry->Key;

	//
	// Set the free bit, and decrement the number of allocations. Finally, release
	// the lock and return the tag that was located
	//
	Entry->Va = (PVOID)((ULONG_PTR)Entry->Va | POOL_BIG_TABLE_ENTRY_FREE);

	ExpPoolBigEntriesInUse--;

	/* If reaching 12.5% of the size (or whatever integer rounding gets us to),
	* halve the allocation size, which will get us to 25% of space used. */
	if (ExpPoolBigEntriesInUse < (PoolBigPageTableSize / (POOL_BIG_TABLE_USE_RATE * 2)))
	{
		/* Shrink the table. */
		ExpReallocateBigPageTable(OldIrql, TRUE);
	}
	else
	{
		PoolUnlock(OldIrql);
	}
	return PoolTag;
}

static BOOLEAN ExpAddTagForBigPages(PVOID Va, ULONG Key, ULONG NumberOfPages)
{
	ULONG Hash, i = 0;
	PVOID OldVa;
	KIRQL OldIrql;
	SIZE_T TableSize;
	PPOOL_TRACKER_BIG_PAGES Entry, EntryEnd, EntryStart;
	assert(((ULONG_PTR)Va & POOL_BIG_TABLE_ENTRY_FREE) == 0);

	//
	// As the table is expandable, these values must only be read after acquiring
	// the lock to avoid a teared access during an expansion
	// NOTE: Windows uses a special reader/writer SpinLock to improve
	// performance in the common case (add/remove a tracker entry)
	//
Retry:
	Hash = ExpComputePartialHashForAddress(Va);
	OldIrql = PoolLock();
	Hash &= PoolBigPageTableHash;
	TableSize = PoolBigPageTableSize;

	//
	// We loop from the current hash bucket to the end of the table, and then
	// rollover to hash bucket 0 and keep going from there. If we return back
	// to the beginning, then we attempt expansion at the bottom of the loop
	//
	EntryStart = Entry = &PoolBigPageTable[Hash];
	EntryEnd = &PoolBigPageTable[TableSize];
	do
	{
		//
		// Make sure that this is a free entry and attempt to atomically make the
		// entry busy now
		// NOTE: the Interlocked operation cannot fail with an exclusive SpinLock
		//
		OldVa = Entry->Va;
		if (((ULONG_PTR)OldVa & POOL_BIG_TABLE_ENTRY_FREE) &&
			(InterlockedCompareExchangePointer(&Entry->Va, Va, OldVa) == OldVa))
		{
			//
			// We now own this entry, write down the size and the pool tag
			//
			Entry->Key = Key;
			Entry->NumberOfPages = NumberOfPages;

			//
			// Add one more entry to the count, and see if we're getting within
			// 75% of the table size, at which point we'll do an expansion now
			// to avoid blocking too hard later on.
			//
			// Note that we only do this if it's also been the 16th time that we
			// keep losing the race or that we are not finding a free entry anymore,
			// which implies a massive number of concurrent big pool allocations.
			//
			ExpPoolBigEntriesInUse++;
			if ((i >= 16) && (ExpPoolBigEntriesInUse > (TableSize * (POOL_BIG_TABLE_USE_RATE - 1) / POOL_BIG_TABLE_USE_RATE)))
			{
				DbgPrint("Attempting expansion since we now have %lu entries",
					ExpPoolBigEntriesInUse);
				assert(TableSize == PoolBigPageTableSize);
				ExpReallocateBigPageTable(OldIrql, FALSE);
				return TRUE;
			}

			//
			// We have our entry, return
			//
			PoolUnlock(OldIrql);
			return TRUE;
		}

		//
		// We don't have our entry yet, so keep trying, making the entry list
		// circular if we reach the last entry. We'll eventually break out of
		// the loop once we've rolled over and returned back to our original
		// hash bucket
		//
		i++;
		if (++Entry >= EntryEnd) Entry = &PoolBigPageTable[0];
	} while (Entry != EntryStart);

	//
	// This means there's no free hash buckets whatsoever, so we now have
	// to attempt expanding the table
	//
	assert(TableSize == PoolBigPageTableSize);
	if (ExpReallocateBigPageTable(OldIrql, FALSE))
	{
		goto Retry;
	}
	ExpBigTableExpansionFailed++;
	DbgPrint("Big pool table expansion failed");
	return FALSE;
}

static VOID ExpSeedHotTags()
{
	ULONG i, Key, Hash, Index;
	PPOOL_TRACKER_TABLE TrackTable = PoolTrackTable;
	ULONG TagList[] =
	{
		'  oI',
		' laH',
		'PldM',
		'LooP',
		'tSbO',
		' prI',
		'bdDN',
		'LprI',
		'pOoI',
		' ldM',
		'eliF',
		'aVMC',
		'dSeS',
		'CFtN',
		'looP',
		'rPCT',
		'bNMC',
		'dTeS',
		'sFtN',
		'TPCT',
		'CPCT',
		' yeK',
		'qSbO',
		'mNoI',
		'aEoI',
		'cPCT',
		'aFtN',
		'0ftN',
		'tceS',
		'SprI',
		'ekoT',
		'  eS',
		'lCbO',
		'cScC',
		'lFtN',
		'cAeS',
		'mfSF',
		'kWcC',
		'miSF',
		'CdfA',
		'EdfA',
		'orSF',
		'nftN',
		'PRIU',
		'rFpN',
		'RFpN',
		'aPeS',
		'sUeS',
		'FpcA',
		'MpcA',
		'cSeS',
		'mNbO',
		'sFpN',
		'uLeS',
		'DPcS',
		'nevE',
		'vrqR',
		'ldaV',
		'  pP',
		'SdaV',
		' daV',
		'LdaV',
		'FdaV',
		' GIB',
	};

	//
	// Loop all 64 hot tags
	//
	assert((sizeof(TagList) / sizeof(ULONG)) == 64);
	for (i = 0; i < sizeof(TagList) / sizeof(ULONG); i++)
	{
		//
		// Get the current tag, and compute its hash in the tracker table
		//
		Key = TagList[i];
		Hash = ExpComputeHashForTag(Key, PoolTrackTableMask);

		//
		// Loop all the hashes in this index/bucket
		//
		Index = Hash;
		while (TRUE)
		{
			//
			// Find an empty entry, and make sure this isn't the last hash that
			// can fit.
			//
			// On checked builds, also make sure this is the first time we are
			// seeding this tag.
			//
			assert(TrackTable[Hash].Key != Key);
			if (!(TrackTable[Hash].Key) && (Hash != PoolTrackTableSize - 1))
			{
				//
				// It has been seeded, move on to the next tag
				//
				TrackTable[Hash].Key = Key;
				break;
			}

			//
			// This entry was already taken, compute the next possible hash while
			// making sure we're not back at our initial index.
			//
			assert(TrackTable[Hash].Key != Key);
			Hash = (Hash + 1) & PoolTrackTableMask;
			if (Hash == Index) break;
		}
	}
}
#endif

static VOID ExInitializePoolDescriptor(PPOOL_DESCRIPTOR PoolDescriptor, ULONG PoolIndex, ULONG Threshold, PVOID PoolLock)
{
	PLIST_ENTRY NextEntry, LastEntry;

	//
	// Setup the descriptor based on the caller's request
	//
	PoolDescriptor->PoolIndex = PoolIndex;
	PoolDescriptor->Threshold = Threshold;
	PoolDescriptor->LockAddress = PoolLock;

	//
	// Initialize accounting data
	//
	PoolDescriptor->RunningAllocs = 0;
	PoolDescriptor->RunningDeAllocs = 0;
	PoolDescriptor->TotalPages = 0;
	PoolDescriptor->TotalBytes = 0;
	PoolDescriptor->TotalBigPages = 0;

	//
	// Nothing pending for now
	//
	PoolDescriptor->PendingFrees = NULL;
	PoolDescriptor->PendingFreeDepth = 0;

	//
	// Loop all the descriptor's allocation lists and initialize them
	//
	NextEntry = PoolDescriptor->ListHeads;
	LastEntry = NextEntry + POOL_LISTS_PER_PAGE;
	while (NextEntry < LastEntry)
	{
		NextEntry->Flink = NextEntry->Blink = ExpEncodePoolLink(NextEntry);
		NextEntry++;
	}
}

BOOLEAN ExInitializePool()
{
#ifdef ENABLE_POOL_TAG_TRACKING
	ULONG i;

	//
	// Allocate the tracker table
	//
	PoolTrackTableSize = 1024;
	PoolTrackTable = (PPOOL_TRACKER_TABLE)MmDbgAllocateMemory((PoolTrackTableSize + 1) * sizeof(POOL_TRACKER_TABLE), PAGE_READWRITE);
	if (!PoolTrackTable) {
		PoolTrackTable = (PPOOL_TRACKER_TABLE)MiAllocateSystemMemory((PoolTrackTableSize + 1) * sizeof(POOL_TRACKER_TABLE), PAGE_READWRITE, Pool, FALSE);
		if (!PoolTrackTable) {
			return FALSE;
		}
	}

	//
	// Add one entry, compute the hash, and zero the table
	//
	PoolTrackTableSize++;
	PoolTrackTableMask = PoolTrackTableSize - 2;

	RtlZeroMemory(PoolTrackTable,
		PoolTrackTableSize * sizeof(POOL_TRACKER_TABLE));

	//
	// Finally, add the most used tags to speed up those allocations
	//
	ExpSeedHotTags();

	//
	// We now do the exact same thing with the tracker table for big pages
	//
	PoolBigPageTableSize = 4096;
	PoolBigPageTable = (PPOOL_TRACKER_BIG_PAGES)MmDbgAllocateMemory(PoolBigPageTableSize * sizeof(POOL_TRACKER_BIG_PAGES), PAGE_READWRITE);
	if (!PoolBigPageTable) {
		PoolBigPageTable = (PPOOL_TRACKER_BIG_PAGES)MiAllocateSystemMemory(PoolBigPageTableSize * sizeof(POOL_TRACKER_BIG_PAGES), PAGE_READWRITE, Pool, FALSE);
		if (!PoolBigPageTable) {
			return FALSE;
		}
	}

	//
	// An extra entry is not needed for for the big pool tracker, so just
	// compute the hash and zero it
	//
	PoolBigPageTableHash = PoolBigPageTableSize - 1;
	RtlZeroMemory(PoolBigPageTable,
		PoolBigPageTableSize * sizeof(POOL_TRACKER_BIG_PAGES));
	for (i = 0; i < PoolBigPageTableSize; i++)
	{
		PoolBigPageTable[i].Va = (PVOID)POOL_BIG_TABLE_ENTRY_FREE;
	}

	//
	// During development, print this out so we can see what's happening
	//
	DbgPrint("EXPOOL: Pool Tracker Table at: 0x%p with 0x%lx bytes",
		PoolTrackTable, PoolTrackTableSize * sizeof(POOL_TRACKER_TABLE));
	DbgPrint("EXPOOL: Big Pool Tracker Table at: 0x%p with 0x%lx bytes",
		PoolBigPageTable, PoolBigPageTableSize * sizeof(POOL_TRACKER_BIG_PAGES));

	//
	// Insert the generic tracker for all of big pool
	//
	ExpInsertPoolTracker('looP',
		ROUND_UP_4K(PoolBigPageTableSize *
			sizeof(POOL_TRACKER_BIG_PAGES)));
#endif

	//
	// Initialize the nonpaged pool descriptor
	//
	ExInitializePoolDescriptor(&NonPagedPoolDescriptor, 0, 0, NULL);

	return TRUE;
}

EXPORTNUM(14) PVOID XBOXAPI ExAllocatePool
(
	SIZE_T NumberOfBytes
)
{
	return ExAllocatePoolWithTag(NumberOfBytes, 'enoN');
}

EXPORTNUM(15) PVOID XBOXAPI ExAllocatePoolWithTag
(
	SIZE_T NumberOfBytes,
	ULONG Tag
)
{
	PPOOL_DESCRIPTOR PoolDesc;
	PLIST_ENTRY ListHead;
	PPOOL_HEADER Entry, NextEntry, FragmentEntry;
	KIRQL OldIrql;
	USHORT BlockSize, i;
	ULONG OriginalType;


	//
	// Get the pool type and its corresponding vector for this request
	//
	OriginalType = NonPagedPool;
	PoolDesc = &NonPagedPoolDescriptor;

	//
	// Check if this is a big page allocation
	//
	if (NumberOfBytes > POOL_MAX_ALLOC)
	{
		//
		// Allocate pages for it
		//
		Entry = (PPOOL_HEADER)MiAllocateSystemMemory(NumberOfBytes, PAGE_READWRITE, Pool, FALSE);
		if (!Entry)
		{
#ifdef ENABLE_POOL_TAG_TRACKING
			//
			// Internal debugging
			//
			ExPoolFailures++;
#endif

			return NULL;
		}

		//
		// Increment required counters
		//
		InterlockedExchangeAdd((PLONG)&PoolDesc->TotalBigPages,
			(LONG)BYTES_TO_PAGES(NumberOfBytes));
		InterlockedExchangeAddSizeT(&PoolDesc->TotalBytes, NumberOfBytes);
		InterlockedIncrement((PLONG)&PoolDesc->RunningAllocs);

#ifdef ENABLE_POOL_TAG_TRACKING
		//
		// Add a tag for the big page allocation and switch to the generic "BIG"
		// tag if we failed to do so, then insert a tracker for this allocation.
		//
		if (!ExpAddTagForBigPages(Entry,
			Tag,
			(ULONG)BYTES_TO_PAGES(NumberOfBytes)))
		{
			Tag = ' GIB';
		}
		ExpInsertPoolTracker(Tag, ROUND_UP_4K(NumberOfBytes));
#endif
		return Entry;
	}

	//
	// Should never request 0 bytes from the pool, but since so many drivers do
	// it, we'll just assume they want 1 byte, based on NT's similar behavior
	//
	if (!NumberOfBytes) NumberOfBytes = 1;

	//
	// A pool allocation is defined by its data, a linked list to connect it to
	// the free list (if necessary), and a pool header to store accounting info.
	// Calculate this size, then convert it into a block size (units of pool
	// headers)
	//
	// Note that i cannot overflow (past POOL_LISTS_PER_PAGE) because any such
	// request would've been treated as a POOL_MAX_ALLOC earlier and resulted in
	// the direct allocation of pages.
	//
	i = (USHORT)((NumberOfBytes + sizeof(POOL_HEADER) + (POOL_BLOCK_SIZE - 1))
		/ POOL_BLOCK_SIZE);
	assert(i < POOL_LISTS_PER_PAGE);

	// NOTE: there's no lookaside list support on the Xbox

	//
	// Loop in the free lists looking for a block if this size. Start with the
	// list optimized for this kind of size lookup
	//
	ListHead = &PoolDesc->ListHeads[i];
	do
	{
		//
		// Are there any free entries available on this list?
		//
		if (!ExpIsPoolListEmpty(ListHead))
		{
			//
			// Acquire the pool lock now
			//
			OldIrql = ExLockPool(PoolDesc);

			//
			// And make sure the list still has entries
			//
			if (ExpIsPoolListEmpty(ListHead))
			{
				//
				// Someone raced us (and won) before we had a chance to acquire
				// the lock.
				//
				// Try again!
				//
				ExUnlockPool(PoolDesc, OldIrql);
				continue;
			}

			//
			// Remove a free entry from the list
			// Note that due to the way we insert free blocks into multiple lists
			// there is a guarantee that any block on this list will either be
			// of the correct size, or perhaps larger.
			//
			ExpCheckPoolLinks(ListHead);
			Entry = POOL_ENTRY(ExpRemovePoolHeadList(ListHead));
			ExpCheckPoolLinks(ListHead);
			ExpCheckPoolBlocks(Entry);
			assert(Entry->BlockSize >= i);
			assert(Entry->PoolType == 0);

			//
			// Check if this block is larger that what we need. The block could
			// not possibly be smaller, due to the reason explained above (and
			// we would've asserted on a checked build if this was the case).
			//
			if (Entry->BlockSize != i)
			{
				//
				// Is there an entry before this one?
				//
				if (Entry->PreviousSize == 0)
				{
					//
					// There isn't anyone before us, so take the next block and
					// turn it into a fragment that contains the leftover data
					// that we don't need to satisfy the caller's request
					//
					FragmentEntry = POOL_BLOCK(Entry, i);
					FragmentEntry->BlockSize = Entry->BlockSize - i;

					//
					// And make it point back to us
					//
					FragmentEntry->PreviousSize = i;

					//
					// Now get the block that follows the new fragment and check
					// if it's still on the same page as us (and not at the end)
					//
					NextEntry = POOL_NEXT_BLOCK(FragmentEntry);
					if ((PVOID)ROUND_DOWN_4K(NextEntry) != NextEntry)
					{
						//
						// Adjust this next block to point to our newly created
						// fragment block
						//
						NextEntry->PreviousSize = FragmentEntry->BlockSize;
					}
				}
				else
				{
					//
					// There is a free entry before us, which we know is smaller
					// so we'll make this entry the fragment instead
					//
					FragmentEntry = Entry;

					//
					// And then we'll remove from it the actual size required.
					// Now the entry is a leftover free fragment
					//
					Entry->BlockSize -= i;

					//
					// Now let's go to the next entry after the fragment (which
					// used to point to our original free entry) and make it
					// reference the new fragment entry instead.
					//
					// This is the entry that will actually end up holding the
					// allocation!
					//
					Entry = POOL_NEXT_BLOCK(Entry);
					Entry->PreviousSize = FragmentEntry->BlockSize;

					//
					// And now let's go to the entry after that one and check if
					// it's still on the same page, and not at the end
					//
					NextEntry = POOL_BLOCK(Entry, i);
					if ((PVOID)ROUND_DOWN_4K(NextEntry) != NextEntry)
					{
						//
						// Make it reference the allocation entry
						//
						NextEntry->PreviousSize = i;
					}
				}

				//
				// Now our (allocation) entry is the right size
				//
				Entry->BlockSize = i;

				//
				// And the next entry is now the free fragment which contains
				// the remaining difference between how big the original entry
				// was, and the actual size the caller needs/requested.
				//
				FragmentEntry->PoolType = 0;
				BlockSize = FragmentEntry->BlockSize;

				//
				// Now check if enough free bytes remained for us to have a
				// "full" entry, which contains enough bytes for a linked list
				// and thus can be used for allocations (up to 8 bytes...)
				//
				ExpCheckPoolLinks(&PoolDesc->ListHeads[BlockSize - 1]);
				if (BlockSize != 1)
				{
					//
					// Insert the free entry into the free list for this size
					//
					ExpInsertPoolTailList(&PoolDesc->ListHeads[BlockSize - 1],
						POOL_FREE_BLOCK(FragmentEntry));
					ExpCheckPoolLinks(POOL_FREE_BLOCK(FragmentEntry));
				}
			}

			//
			// We have found an entry for this allocation, so set the pool type
			// and release the lock since we're done
			//
			Entry->PoolType = OriginalType + 1;
			ExpCheckPoolBlocks(Entry);
			ExUnlockPool(PoolDesc, OldIrql);

			//
			// Increment required counters
			//
			InterlockedExchangeAddSizeT(&PoolDesc->TotalBytes, Entry->BlockSize * POOL_BLOCK_SIZE);
			InterlockedIncrement((PLONG)&PoolDesc->RunningAllocs);

#ifdef ENABLE_POOL_TAG_TRACKING
			//
			// Track this allocation
			//
			ExpInsertPoolTracker(Tag,
				Entry->BlockSize * POOL_BLOCK_SIZE);
#endif

			//
			// Return the pool allocation
			//
			Entry->PoolTag = Tag;
			(POOL_FREE_BLOCK(Entry))->Flink = NULL;
			(POOL_FREE_BLOCK(Entry))->Blink = NULL;
			return POOL_FREE_BLOCK(Entry);
		}
	} while (++ListHead != &PoolDesc->ListHeads[POOL_LISTS_PER_PAGE]);

	//
	// There were no free entries left, so we have to allocate a new fresh page
	//
	Entry = (PPOOL_HEADER)MiAllocateSystemMemory(PAGE_SIZE, PAGE_READWRITE, Pool, FALSE);
	if (!Entry)
	{
#ifdef ENABLE_POOL_TAG_TRACKING
		//
		// Internal debugging
		//
		ExPoolFailures++;
#endif

		//
		// Return NULL to the caller in all other cases
		//
		return NULL;
	}

	//
	// Setup the entry data
	//
	Entry->Ulong1 = 0;
	Entry->BlockSize = i;
	Entry->PoolType = OriginalType + 1;

	//
	// This page will have two entries -- one for the allocation (which we just
	// created above), and one for the remaining free bytes, which we're about
	// to create now. The free bytes are the whole page minus what was allocated
	// and then converted into units of block headers.
	//
	BlockSize = (PAGE_SIZE / POOL_BLOCK_SIZE) - i;
	FragmentEntry = POOL_BLOCK(Entry, i);
	FragmentEntry->Ulong1 = 0;
	FragmentEntry->BlockSize = BlockSize;
	FragmentEntry->PreviousSize = i;

	//
	// Increment required counters
	//
	InterlockedIncrement((PLONG)&PoolDesc->TotalPages);
	InterlockedExchangeAddSizeT(&PoolDesc->TotalBytes, Entry->BlockSize * POOL_BLOCK_SIZE);

	//
	// Now check if enough free bytes remained for us to have a "full" entry,
	// which contains enough bytes for a linked list and thus can be used for
	// allocations (up to 8 bytes...)
	//
	if (FragmentEntry->BlockSize != 1)
	{
		//
		// Excellent -- acquire the pool lock
		//
		OldIrql = ExLockPool(PoolDesc);

		//
		// And insert the free entry into the free list for this block size
		//
		ExpCheckPoolLinks(&PoolDesc->ListHeads[BlockSize - 1]);
		ExpInsertPoolTailList(&PoolDesc->ListHeads[BlockSize - 1],
			POOL_FREE_BLOCK(FragmentEntry));
		ExpCheckPoolLinks(POOL_FREE_BLOCK(FragmentEntry));

		//
		// Release the pool lock
		//
		ExpCheckPoolBlocks(Entry);
		ExUnlockPool(PoolDesc, OldIrql);
	}
	else
	{
		//
		// Simply do a sanity check
		//
		ExpCheckPoolBlocks(Entry);
	}

	//
	// Increment performance counters and track this allocation
	//
	InterlockedIncrement((PLONG)&PoolDesc->RunningAllocs);
#ifdef ENABLE_POOL_TAG_TRACKING
	ExpInsertPoolTracker(Tag,
		Entry->BlockSize * POOL_BLOCK_SIZE);
#endif

	//
	// And return the pool allocation
	//
	ExpCheckPoolBlocks(Entry);
	Entry->PoolTag = Tag;
	return POOL_FREE_BLOCK(Entry);
}

EXPORTNUM(17) VOID XBOXAPI ExFreePool
(
	PVOID P
)
{
	PPOOL_HEADER Entry, NextEntry;
	USHORT BlockSize;
	KIRQL OldIrql;
	POOL_TYPE PoolType;
	PPOOL_DESCRIPTOR PoolDesc;
	BOOLEAN Combined = FALSE;
	PFN_NUMBER PageCount, RealPageCount;

	//
	// Check if this is a big page allocation
	//
	if ((PVOID)ROUND_DOWN_4K(P) == P)
	{
		//
		// We need to find the tag for it, so first we need to find out what
		// kind of allocation this was (paged or nonpaged), then we can go
		// ahead and try finding the tag for it. Remember to get rid of the
		// PROTECTED_POOL tag if it's found.
		//
		// Note that if at insertion time, we failed to add the tag for a big
		// pool allocation, we used a special tag called 'BIG' to identify the
		// allocation, and we may get this tag back. In this scenario, we must
		// manually get the size of the allocation by actually counting through
		// the PFN database.
		//
		PoolType = NonPagedPool;
		ExpCheckPoolIrqlLevel(PoolType, 0, P);
#ifdef ENABLE_POOL_TAG_TRACKING
		ULONG Tag = ExpFindAndRemoveTagBigPages(P, &PageCount);
		if (!Tag)
		{
			DbgPrint("We do not know the size of this allocation. This is not yet supported");
			assert(Tag == ' GIB');
			PageCount = 1; // We are going to lie! This might screw up accounting?
		}
		else if (Tag & PROTECTED_POOL)
		{
			Tag &= ~PROTECTED_POOL;
		}

		//
		// We have our tag and our page count, so we can go ahead and remove this
		// tracker now
		//
		ExpRemovePoolTracker(Tag, PageCount << PAGE_SHIFT);

		//
		// Update counters
		//
		PoolDesc = &NonPagedPoolDescriptor;
		InterlockedIncrement((PLONG)&PoolDesc->RunningDeAllocs);
		InterlockedExchangeAddSizeT(&PoolDesc->TotalBytes,
			-(LONG_PTR)(PageCount << PAGE_SHIFT));

		//
		// Do the real free now and update the last counter with the big page count
		//
		RealPageCount = MiFreeSystemMemory(P, 0);
		assert(RealPageCount == PageCount);
		InterlockedExchangeAdd((PLONG)&PoolDesc->TotalBigPages,
			-(LONG)RealPageCount);
		return;
#else
		//
		// Do the real free now and update the last counters with the big page count
		//
		PoolDesc = &NonPagedPoolDescriptor;
		RealPageCount = PageCount = MiFreeSystemMemory(P, 0);
		InterlockedIncrement((PLONG)&PoolDesc->RunningDeAllocs);
		InterlockedExchangeAddSizeT(&PoolDesc->TotalBytes,
			-(LONG_PTR)(PageCount << PAGE_SHIFT));
		InterlockedExchangeAdd((PLONG)&PoolDesc->TotalBigPages,
			-(LONG)RealPageCount);
		return;
#endif
	}

	//
	// Get the entry for this pool allocation
	// The pointer math here may look wrong or confusing, but it is quite right
	//
	Entry = (PPOOL_HEADER)P;
	Entry--;
	assert((ULONG_PTR)Entry % POOL_BLOCK_SIZE == 0);

	//
	// Get the size of the entry, and it's pool type, then load the descriptor
	// for this pool type
	//
	BlockSize = Entry->BlockSize;
	PoolType = (POOL_TYPE)((Entry->PoolType - 1) & BASE_POOL_TYPE_MASK);
	PoolDesc = &NonPagedPoolDescriptor;

	//
	// Make sure that the IRQL makes sense
	//
	ExpCheckPoolIrqlLevel(PoolType, 0, P);
#ifdef ENABLE_POOL_TAG_TRACKING
	//
	// Get the pool tag and get rid of the PROTECTED_POOL flag
	//
	ULONG Tag = Entry->PoolTag;
	if (Tag & PROTECTED_POOL) Tag &= ~PROTECTED_POOL;

	//
	// Track the removal of this allocation
	//
	ExpRemovePoolTracker(Tag,
		BlockSize * POOL_BLOCK_SIZE);
#endif

	// NOTE: pool quotas are not supported on Xbox
	// NOTE: there's no lookaside list support on the Xbox

	//
	// Get the pointer to the next entry
	//
	NextEntry = POOL_BLOCK(Entry, BlockSize);

	//
	// Update performance counters
	//
	InterlockedIncrement((PLONG)&PoolDesc->RunningDeAllocs);
	InterlockedExchangeAddSizeT(&PoolDesc->TotalBytes, -BlockSize * POOL_BLOCK_SIZE);

	//
	// Acquire the pool lock
	//
	OldIrql = ExLockPool(PoolDesc);

	//
	// Check if the next allocation is at the end of the page
	//
	ExpCheckPoolBlocks(Entry);
	if ((PPOOL_HEADER)ROUND_DOWN_4K(NextEntry) != NextEntry)
	{
		//
		// We may be able to combine the block if it's free
		//
		if (NextEntry->PoolType == 0)
		{
			//
			// The next block is free, so we'll do a combine
			//
			Combined = TRUE;

			//
			// Make sure there's actual data in the block -- anything smaller
			// than this means we only have the header, so there's no linked list
			// for us to remove
			//
			if ((NextEntry->BlockSize != 1))
			{
				//
				// The block is at least big enough to have a linked list, so go
				// ahead and remove it
				//
				ExpCheckPoolLinks(POOL_FREE_BLOCK(NextEntry));
				ExpRemovePoolEntryList(POOL_FREE_BLOCK(NextEntry));
				ExpCheckPoolLinks(ExpDecodePoolLink((POOL_FREE_BLOCK(NextEntry))->Flink));
				ExpCheckPoolLinks(ExpDecodePoolLink((POOL_FREE_BLOCK(NextEntry))->Blink));
			}

			//
			// Our entry is now combined with the next entry
			//
			Entry->BlockSize = Entry->BlockSize + NextEntry->BlockSize;
		}
	}

	//
	// Now check if there was a previous entry on the same page as us
	//
	if (Entry->PreviousSize)
	{
		//
		// Great, grab that entry and check if it's free
		//
		NextEntry = POOL_PREV_BLOCK(Entry);
		if (NextEntry->PoolType == 0)
		{
			//
			// It is, so we can do a combine
			//
			Combined = TRUE;

			//
			// Make sure there's actual data in the block -- anything smaller
			// than this means we only have the header so there's no linked list
			// for us to remove
			//
			if ((NextEntry->BlockSize != 1))
			{
				//
				// The block is at least big enough to have a linked list, so go
				// ahead and remove it
				//
				ExpCheckPoolLinks(POOL_FREE_BLOCK(NextEntry));
				ExpRemovePoolEntryList(POOL_FREE_BLOCK(NextEntry));
				ExpCheckPoolLinks(ExpDecodePoolLink((POOL_FREE_BLOCK(NextEntry))->Flink));
				ExpCheckPoolLinks(ExpDecodePoolLink((POOL_FREE_BLOCK(NextEntry))->Blink));
			}

			//
			// Combine our original block (which might've already been combined
			// with the next block), into the previous block
			//
			NextEntry->BlockSize = NextEntry->BlockSize + Entry->BlockSize;

			//
			// And now we'll work with the previous block instead
			//
			Entry = NextEntry;
		}
	}

	//
	// By now, it may have been possible for our combined blocks to actually
	// have made up a full page (if there were only 2-3 allocations on the
	// page, they could've all been combined).
	//
	if (((PPOOL_HEADER)ROUND_DOWN_4K(Entry) == Entry) &&
		((PPOOL_HEADER)ROUND_DOWN_4K(POOL_NEXT_BLOCK(Entry)) == POOL_NEXT_BLOCK(Entry)))
	{
		//
		// In this case, release the pool lock, update the performance counter,
		// and free the page
		//
		ExUnlockPool(PoolDesc, OldIrql);
		InterlockedExchangeAdd((PLONG)&PoolDesc->TotalPages, -1);
		MiFreeSystemMemory(Entry, 0);
		return;
	}

	//
	// Otherwise, we now have a free block (or a combination of 2 or 3)
	//
	Entry->PoolType = 0;
	BlockSize = Entry->BlockSize;
	assert(BlockSize != 1);

	//
	// Check if we actually did combine it with anyone
	//
	if (Combined)
	{
		//
		// Get the first combined block (either our original to begin with, or
		// the one after the original, depending if we combined with the previous)
		//
		NextEntry = POOL_NEXT_BLOCK(Entry);

		//
		// As long as the next block isn't on a page boundary, have it point
		// back to us
		//
		if ((PPOOL_HEADER)ROUND_DOWN_4K(NextEntry) != NextEntry) NextEntry->PreviousSize = BlockSize;
	}

	//
	// Insert this new free block, and release the pool lock
	//
	ExpInsertPoolHeadList(&PoolDesc->ListHeads[BlockSize - 1], POOL_FREE_BLOCK(Entry));
	ExpCheckPoolLinks(POOL_FREE_BLOCK(Entry));
	ExUnlockPool(PoolDesc, OldIrql);
}

EXPORTNUM(23) ULONG XBOXAPI ExQueryPoolBlockSize
(
	PVOID PoolBlock
)
{
	if (CHECK_ALIGNMENT(PoolBlock, PAGE_SIZE)) {
		return MmQueryAllocationSize(PoolBlock);
	}

	//
	// Small block: the header sits directly before the returned pointer
	//
	PPOOL_HEADER Entry = (PPOOL_HEADER)PoolBlock - 1;

	//
	// Allocated blocks have a nonzero pool type
	//
	assert(Entry->PoolType != 0);

	//
	// Usable bytes = block size minus the header
	//
	return ((SIZE_T)Entry->BlockSize * POOL_BLOCK_SIZE) - sizeof(POOL_HEADER);
}
