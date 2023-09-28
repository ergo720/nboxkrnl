/*
 * ergo720                Copyright (c) 2023
 */

#include "..\mm\mm.hpp"
#include "..\mm\mi.hpp"
#include "..\ex\ex.hpp"
#include <assert.h>


// Block of data in front the actual chunk allocated
union CHUNK_HEADER {
	struct {
		ULONG Size;
		ULONG Tag;
	} Busy;
	struct {
		CHUNK_HEADER *Flink;
		ULONG Unused;
	} Free;
};

static_assert(sizeof(CHUNK_HEADER) == 8);

#define POOL_SIZE            PAGE_SIZE
#define CHUNK_OVERHEAD       sizeof(CHUNK_HEADER)
#define CHUNK_SHIFT          5
#define NUM_CHUNK_LISTS      64

// Macros to ensure thread safety
#define PoolLock() KeRaiseIrqlToDpcLevel()
#define PoolUnlock(Irql) KfLowerIrql(Irql)

// Tracks lists to chunks with a size multiple of 32 bytes
static CHUNK_HEADER *ChunkListsHead[NUM_CHUNK_LISTS] = { nullptr };


static CHUNK_HEADER *CreatePool(SIZE_T ChunkSize)
{
	CHUNK_HEADER *Start = (CHUNK_HEADER *)MiAllocateSystemMemory(POOL_SIZE, PAGE_READWRITE, Pool, FALSE);
	if (Start == nullptr) {
		return nullptr;
	}

	CHUNK_HEADER *Addr = Start;
	for (unsigned i = 0; i < (POOL_SIZE / ChunkSize - 1); ++i) {
		Addr->Free.Flink = (CHUNK_HEADER *)((uint8_t *)Addr + ChunkSize);
		Addr = Addr->Free.Flink;
	}

	Addr->Free.Flink = nullptr;
	return Start;
}

static PVOID AllocChunk(SIZE_T ListIdx, ULONG Tag)
{
	SIZE_T ChunkSize = (ListIdx + 1) << CHUNK_SHIFT;
	if (ChunkListsHead[ListIdx] == nullptr) {
		ChunkListsHead[ListIdx] = CreatePool(ChunkSize);
		if (ChunkListsHead[ListIdx] == nullptr) {
			return nullptr;
		}
	}

	CHUNK_HEADER *Addr = ChunkListsHead[ListIdx];
	ChunkListsHead[ListIdx] = ChunkListsHead[ListIdx]->Free.Flink;
	Addr->Busy.Size = ChunkSize;
	Addr->Busy.Tag = Tag;

	return Addr + 1;
}

static VOID FreeChunk(CHUNK_HEADER *Addr, SIZE_T ListIdx)
{
	Addr->Free.Flink = ChunkListsHead[ListIdx];
	ChunkListsHead[ListIdx] = Addr;
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
	if (NumberOfBytes > (POOL_SIZE / 2 - CHUNK_OVERHEAD)) {
		return MiAllocateSystemMemory(NumberOfBytes, PAGE_READWRITE, Pool, FALSE);
	}

	KIRQL OldIrql = PoolLock();
	SIZE_T ListIdx = (NumberOfBytes + CHUNK_OVERHEAD - 1) >> CHUNK_SHIFT;
	assert((ListIdx >= 0) && (ListIdx < NUM_CHUNK_LISTS));
	PVOID Addr = AllocChunk(ListIdx, Tag);
	assert(!CHECK_ALIGNMENT(Addr, POOL_SIZE));
	PoolUnlock(OldIrql);

	return Addr;
}

EXPORTNUM(17) VOID XBOXAPI ExFreePool
(
	PVOID P
)
{
	if (CHECK_ALIGNMENT(P, POOL_SIZE)) {
		MiFreeSystemMemory(P, 0);
		return;
	}

	KIRQL OldIrql = PoolLock();
	CHUNK_HEADER *Header = (CHUNK_HEADER *)((uint8_t *)P - CHUNK_OVERHEAD);
	FreeChunk(Header, (Header->Busy.Size - 1) >> CHUNK_SHIFT);
	PoolUnlock(OldIrql);
}

EXPORTNUM(23) ULONG XBOXAPI ExQueryPoolBlockSize
(
	PVOID PoolBlock
)
{
	if (CHECK_ALIGNMENT(PoolBlock, POOL_SIZE)) {
		return MmQueryAllocationSize(PoolBlock);
	}

	CHUNK_HEADER *Header = (CHUNK_HEADER *)((uint8_t *)PoolBlock - CHUNK_OVERHEAD);
	return Header->Busy.Size - CHUNK_OVERHEAD;
}
