/*
 * ergo720                Copyright (c) 2023
 */

#include "mm.hpp"
#include "mi.hpp"
#include "ex.hpp"
#include <assert.h>


#define POOL_SIZE            4096
#define CHUNK_IDX_OFFSET     5
#define NUM_CHUNK_LISTS      6

// Macros to ensure thread safety
#define PoolLock() KeRaiseIrqlToDpcLevel()
#define PoolUnlock(Irql) KfLowerIrql(Irql)


// Block of data found at the start of a free chunk only
struct CHUNK_HEADER {
	struct CHUNK_HEADER *Flink; // links to the next free chunk
};

// Block of data found at the beginning of a page that hosts a pool
struct POOL_HEADER {
	SIZE_T ChunkSize; // constant for all chunks in this pool, expressed as a power of 2
	ULONG TagArray[1]; // actual size depends on the chunk size
};

// Tracks lists to chunks with different sizes
static CHUNK_HEADER *ChunkListsHead[NUM_CHUNK_LISTS] = {
	nullptr, // 32, 128 chunks, 4 + 512 bytes for POOL_HEADER
	nullptr, // 64, 64 chunks, 4 + 256 bytes for POOL_HEADER
	nullptr, // 128, 32 chunks, 4 + 128 bytes for POOL_HEADER
	nullptr, // 256, 16 chunks, 4 + 64 bytes for POOL_HEADER
	nullptr, // 512, 8 chunks, 4 + 32 bytes for POOL_HEADER
	nullptr, // 1024, 4 chunks, 4 + 16 bytes for POOL_HEADER
};

static CHUNK_HEADER *CreatePool(SIZE_T ChunkSize)
{
	CHUNK_HEADER *Start = (CHUNK_HEADER *)MiAllocateSystemMemory(POOL_SIZE, PAGE_READWRITE, Pool, FALSE);
	if (Start == nullptr) {
		return nullptr;
	}

	// Initialize all CHUNK_HEADERs in the pool
	assert(ChunkSize && !(ChunkSize & (ChunkSize - 1))); // must be a power of 2
	unsigned FirstChunkAvailableIdx = (ChunkSize >= 256) ? 1 : (ChunkSize == 128) ? 2 : (ChunkSize == 64) ? 5 : 17;
	CHUNK_HEADER *Addr = Start, *FirstChunkAvailableAddr = nullptr;
	for (unsigned i = 0; i < (POOL_SIZE / ChunkSize - 1); ++i) {
		if (i == FirstChunkAvailableIdx) {
			FirstChunkAvailableAddr = Addr;
		}
		Addr->Flink = (CHUNK_HEADER *)((uint8_t *)Addr + ChunkSize);
		Addr = Addr->Flink;
	}
	Addr->Flink = nullptr;
	assert(FirstChunkAvailableAddr);

	// Initialize POOL_HEADER
	POOL_HEADER *Pool = (POOL_HEADER *)Start;
	Pool->ChunkSize = ChunkSize;

	return FirstChunkAvailableAddr;
}

static PVOID AllocChunk(SIZE_T NumberOfBytes, ULONG Tag)
{
	NumberOfBytes = next_pow2(NumberOfBytes);
	SIZE_T ListIdx;
	bit_scan_reverse(&ListIdx, NumberOfBytes);
	ListIdx -= CHUNK_IDX_OFFSET;
	assert((ListIdx >= 0) && (ListIdx < NUM_CHUNK_LISTS));

	if (ChunkListsHead[ListIdx] == nullptr) {
		ChunkListsHead[ListIdx] = CreatePool(NumberOfBytes);
		if (ChunkListsHead[ListIdx] == nullptr) {
			return nullptr;
		}
	}

	CHUNK_HEADER *Addr = ChunkListsHead[ListIdx];
	ChunkListsHead[ListIdx] = ChunkListsHead[ListIdx]->Flink;
	POOL_HEADER *Pool = (POOL_HEADER *)ROUND_DOWN((ULONG_PTR)Addr, POOL_SIZE);
	Pool->TagArray[((ULONG_PTR)Addr - (ULONG_PTR)Pool) / Pool->ChunkSize] = Tag;

	return Addr;
}

static VOID FreeChunk(PVOID Addr)
{
	POOL_HEADER *Pool = (POOL_HEADER *)ROUND_DOWN((ULONG_PTR)Addr, POOL_SIZE);
	SIZE_T ListIdx;
	bit_scan_reverse(&ListIdx, Pool->ChunkSize);
	ListIdx -= CHUNK_IDX_OFFSET;
	CHUNK_HEADER *Chunk = (CHUNK_HEADER *)Addr;
	Chunk->Flink = ChunkListsHead[ListIdx];
	ChunkListsHead[ListIdx] = Chunk;
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
	NumberOfBytes = (NumberOfBytes < 32) ? 32 : NumberOfBytes; // smallest chunk size is 32

	if (NumberOfBytes > (POOL_SIZE >> 2)) {
		return MiAllocateSystemMemory(NumberOfBytes, PAGE_READWRITE, Pool, FALSE);
	}

	KIRQL OldIrql = PoolLock();
	PVOID Addr = AllocChunk(NumberOfBytes, Tag);
	PoolUnlock(OldIrql);
	assert(!CHECK_ALIGNMENT(Addr, POOL_SIZE));

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
	FreeChunk(P);
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

	POOL_HEADER *Pool = (POOL_HEADER *)ROUND_DOWN((ULONG_PTR)PoolBlock, POOL_SIZE);
	return Pool->ChunkSize;
}
